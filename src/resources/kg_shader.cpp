//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>
#include <vector>
#include <unordered_map>

#include "vulkan/vulkan_core.h"
#include "spirv_reflect.h"

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "resources/kg_shader.hpp"
#include "resources/kg_mesh.hpp"
#include "resources/kg_texture.hpp"
#include "resources/kg_camera.hpp"
#include "core/kg_context.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::FromVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::Severity;

using KalaGraphics::Resources::Vertex;
using KalaGraphics::Resources::Vertex2D;

using std::unique_ptr;
using std::make_unique;
using std::to_string;
using std::vector;
using std::unordered_map;
using std::string;
using std::string_view;
using std::map;
using std::pair;
using std::make_pair;

using u8 = uint8_t;

struct ShaderModule
{
    bool success{};
    VkShaderModule vkModule{};
    SpvReflectShaderModule* spvModule{};
};

struct PipelineInfo
{
    vector<VkPipelineShaderStageCreateInfo> stages{};

    VkVertexInputBindingDescription bindingDescription{};
    vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    VkPipelineVertexInputStateCreateInfo vertexInput{};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    vector<VkDynamicState> dynamicStates{};
    VkPipelineViewportStateCreateInfo viewportState{};
    VkPipelineRasterizationStateCreateInfo rasterization{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo colorBlend{};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
};

static unique_ptr<PipelineInfo> GetPipelineInfo(
    bool is2D,
    vector<VkPipelineShaderStageCreateInfo> stages,
    VkPipelineLayout pipelineLayout,
    VkRenderPass renderPass)
{
    unique_ptr<PipelineInfo> pi = make_unique<PipelineInfo>();

    pi->stages = std::move(stages);

    pi->bindingDescription.binding   = 0;
    pi->bindingDescription.stride    = is2D ? sizeof(Vertex2D) : sizeof(Vertex);
    pi->bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    if (is2D)
    {
        pi->attributeDescriptions.push_back(
            {
                0,
                0,
                VK_FORMAT_R32G32_SFLOAT,
                offsetof(Vertex2D, pos)
            });
        pi->attributeDescriptions.push_back(
            {
                1,
                0,
                VK_FORMAT_R32G32_SFLOAT,
                offsetof(Vertex2D, uv)
            });
        pi->attributeDescriptions.push_back(
            {
                2,
                0,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                offsetof(Vertex2D, color)
            });
    }
    else
    {
        pi->attributeDescriptions.push_back(
            {
                0,
                0,
                VK_FORMAT_R32G32B32_SFLOAT,
                offsetof(Vertex, pos)
            });
        pi->attributeDescriptions.push_back(
            {
                1,
                0,
                VK_FORMAT_R32G32B32_SFLOAT,
                offsetof(Vertex, normal)
            });
        pi->attributeDescriptions.push_back(
            {
                2,
                0,
                VK_FORMAT_R32G32_SFLOAT,
                offsetof(Vertex, uv)
            });
        pi->attributeDescriptions.push_back(
            {
                3,
                0,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                offsetof(Vertex, color)
            });
    }

    pi->vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pi->vertexInput.vertexBindingDescriptionCount   = 1;
    pi->vertexInput.pVertexBindingDescriptions      = &pi->bindingDescription;
    pi->vertexInput.vertexAttributeDescriptionCount = scast<u32>(pi->attributeDescriptions.size());
    pi->vertexInput.pVertexAttributeDescriptions    = pi->attributeDescriptions.data();

    pi->inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pi->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    pi->dynamicStates = 
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    pi->dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pi->dynamicState.dynamicStateCount = scast<u32>(pi->dynamicStates.size());
    pi->dynamicState.pDynamicStates    = pi->dynamicStates.data();

    pi->viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pi->viewportState.viewportCount = 1;
    pi->viewportState.scissorCount  = 1;

    pi->rasterization.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pi->rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    pi->rasterization.cullMode    = VK_CULL_MODE_BACK_BIT;
    pi->rasterization.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    pi->rasterization.lineWidth   = 1.0f;

    //TODO: add proper multisampling/MSAA

    pi->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pi->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    //TODO: fix depth for 2D

    pi->depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pi->depthStencil.depthTestEnable  = VK_TRUE;
    pi->depthStencil.depthWriteEnable = VK_TRUE;
    pi->depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

    pi->colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;
    pi->colorBlendAttachment.blendEnable = VK_FALSE;

    pi->colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pi->colorBlend.attachmentCount = 1;
    pi->colorBlend.pAttachments    = &pi->colorBlendAttachment;

    pi->pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi->pipelineInfo.stageCount          = scast<u32>(pi->stages.size());
    pi->pipelineInfo.pStages             = pi->stages.data();
    pi->pipelineInfo.pVertexInputState   = &pi->vertexInput;
    pi->pipelineInfo.pInputAssemblyState = &pi->inputAssembly;
    pi->pipelineInfo.pViewportState      = &pi->viewportState;
    pi->pipelineInfo.pRasterizationState = &pi->rasterization;
    pi->pipelineInfo.pMultisampleState   = &pi->multisampling;
    pi->pipelineInfo.pDepthStencilState  = &pi->depthStencil;
    pi->pipelineInfo.pColorBlendState    = &pi->colorBlend;
    pi->pipelineInfo.pDynamicState       = &pi->dynamicState;
    pi->pipelineInfo.layout              = pipelineLayout;
    pi->pipelineInfo.renderPass          = renderPass;
    pi->pipelineInfo.subpass             = 0;

    return pi;
}

static void DestroySpvShaderModules(vector<SpvReflectShaderModule*> modules)
{
    for (SpvReflectShaderModule* m : modules)
    {
        spvReflectDestroyShaderModule(m);
        delete m;
    }
};

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Shader> registry{};

    KalaGraphicsRegistry<Shader>& Shader::GetRegistry() { return registry; }

    Shader* Shader::Initialize(u32 contextID)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to initialize shader because the logical device was invalid!");
        }

        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (!gctx)
        {
            Log::Print(
                "Failed to initialize shader because the graphics context '" + to_string(contextID) + "' was invalid!", 
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Shader> newShader = make_unique<Shader>();
        Shader* shaderPtr = newShader.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        shaderPtr->ID = newID;
        shaderPtr->contextID = contextID;

        //graphics context references this shader
        gctx->shaderIDs.push_back(newID);

        registry.AddContent(newID, std::move(newShader));

        Log::Print(
			"Created new shader '" + to_string(newID) 
            + "' for graphics context '" + to_string(contextID) + "'!",
			"KG_SHADER",
			LogType::LOG_SUCCESS);

        return shaderPtr;
    }

    u32 Shader::GetID() const { return ID; }

    u32 Shader::GetGraphicsContextID() const { return contextID; }
    void Shader::SetGraphicsContextID(u32 newValue)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to set shader graphics context ID "
                "because the logical device was invalid!");
        }

        if (contextID == newValue)
        {
            Log::Print("Failed to set shader '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        GraphicsContext* oldContext = GraphicsContext::GetRegistry().GetContent(contextID);
        GraphicsContext* newContext = GraphicsContext::GetRegistry().GetContent(newValue);
        if (!newContext)
        {
            Log::Print("Failed to set shader '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it was invalid!",
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        //can still switch graphics context even if there is no pipeline or shader data
        if (pipeline != VK_NULL_HANDLE)
        {
            auto add_stage = [](
                vector<VkPipelineShaderStageCreateInfo>& stages,
                VkShaderStageFlagBits flag,
                VkShaderModule module) -> void
                {
                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = flag;
                    stage.module = module;
                    stage.pName = "main";
                    stages.push_back(stage);
                };

            vector<VkPipelineShaderStageCreateInfo> stages{};

            add_stage(
                stages,
                VK_SHADER_STAGE_VERTEX_BIT,
                shaderModuleData.vkModule_vert);
            add_stage(
                stages,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                shaderModuleData.vkModule_frag);

            if (shaderModuleData.usingGeom)
            {
                add_stage(
                    stages,
                    VK_SHADER_STAGE_GEOMETRY_BIT,
                    shaderModuleData.vkModule_geom);
            }

            unique_ptr<PipelineInfo> newPipelineInfo = GetPipelineInfo(
                is2D,
                std::move(stages),
                pipelineLayout,
                newContext->GetRenderPass());

            VkPipeline newPipeline{};
            VkResult vkResult = vkCreateGraphicsPipelines(
                logicalDevice,
                VK_NULL_HANDLE,
                1,
                &newPipelineInfo->pipelineInfo,
                nullptr,
                &newPipeline);

            if (vkResult != VK_SUCCESS)
            {
                string message = 
                    "Failed to set shader '" + to_string(ID) + "' graphics context "
                    "to new value '" + to_string(newValue) + "'! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult);

                if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics shader error",
                        std::move(message));
                }
                else
                {
                    Log::Print(
                        message,
                        "KG_SHADER",
                        LogType::LOG_ERROR,
                        2);
                }

                return;
            }

            if (pipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(
                    logicalDevice,
                    pipeline,
                    nullptr);
            }

            pipeline = newPipeline;
        }

        //
        // FINISH
        //

        if (oldContext)
        {
            erase(
                oldContext->shaderIDs,
                ID);
        }
        newContext->shaderIDs.push_back(ID);

        contextID = newContext->GetID();

        Log::Print(
            "Set shader '" + to_string(ID) 
            + "' graphics context ID to '" + to_string(contextID) + "'!",
            "KG_SHADER",
            LogType::LOG_SUCCESS);
    }

    const vector<u32>& Shader::GetMeshIDs() const { return meshIDs; }
    const vector<u32>& Shader::GetTextureIDs() const { return textureIDs; }
    const vector<u32>& Shader::GetCameraIDs() const { return cameraIDs; }

    void Shader::SetShaderData(
        bool is2D,
        path&& vertPath,
        path&& fragPath,
        path&& geomPath)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to set shader '" + to_string(ID) + "' data because the logical device was invalid!");
        }

        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (!gctx)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to set shader '" + to_string(ID) + "' data because the graphics context '" + to_string(contextID) + "' was invalid!");
        }

        auto empty_path = [this](string_view shaderType) -> void
            {
                Log::Print(
                    "Failed to set shader '" + to_string(ID) + "' data because it did not contain a " 
                    + string(shaderType) + " shader file!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto bad_ext = [this](string_view shaderType) -> void
            {
                Log::Print(
                    "Failed to set shader '" + to_string(ID) + "' data because its " + string(shaderType) 
                    + " shader had a missing or incorrect extension!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto invalid_path = [this](
            string_view shaderType,
            string_view shaderPath) -> void
            {
                Log::Print(
                    "Failed to set shader '" + to_string(ID) + "' data because its " + string(shaderType) 
                    + " shader path '" + string(shaderPath) + "' was invalid!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };

        if (vertPath.empty())
        {
            empty_path("vertex");
            return;
        }
        if (!vertPath.has_extension()
            || vertPath.extension() != ".spv")
        {
            bad_ext("vertex");
            return;
        }
        if (!exists(vertPath))
        {
            invalid_path("vertex", vertPath.string());
            return;
        }

        if (fragPath.empty())
        {
            empty_path("fragment");
            return;
        }
        if (!fragPath.has_extension()
            || fragPath.extension() != ".spv")
        {
            bad_ext("fragment");
            return;
        }
        if (!exists(fragPath))
        {
            invalid_path("fragment", fragPath.string());
            return;
        }

        if (!geomPath.empty())
        {
            if (!geomPath.has_extension()
                || geomPath.extension() != ".spv")
            {
                bad_ext("geometry");
                return;
            }
            if (!exists(geomPath))
            {
                invalid_path("geometry", geomPath.string());
                return;
            }
        }

        unordered_map<string, string> shaderPaths =
        {
            { "vertex",   vertPath.string() },
            { "fragment", fragPath.string() },
            { "geometry", geomPath.string() }
        };
        unordered_map<string, string> seen{};

        for (auto& [shaderStage, shaderPath] : shaderPaths)
        {
            if (shaderPath.empty()) continue;

            auto [it, inserted] = seen.insert({shaderPath, shaderStage});
            if (!inserted)
            {
                Log::Print(
                    "Failed to initialize shader because " + shaderStage + " was the same as " + it->second + "!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);

                return;
            }
        }

        auto create_shader_module = [&logicalDevice, this](
            string_view shaderType, 
            const path& shaderPath) -> ShaderModule
            {
                vector<u8> outData{};
                string errMsg = ReadBinaryDataFromFile(shaderPath, outData);

                if (!errMsg.empty())
                {
                    Log::Print(
                        "Failed to read binary data from shader type " + string(shaderType) 
                        + "' for setting shader data for shader '" + to_string(ID) 
                        + "' under graphics context '" + to_string(contextID) + "'! Reason : " + errMsg,
                        "KG_SHADER",
                        LogType::LOG_ERROR,
                        2);

                    return { false };
                }

                VkShaderModuleCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.codeSize = outData.size();
                createInfo.pCode = rcast<const u32*>(outData.data());

                ShaderModule shaderModule{};

                VkShaderModule vkShaderModule{};
                VkResult vkResult = vkCreateShaderModule(
                    logicalDevice,
                    &createInfo,
                    nullptr,
                    &vkShaderModule);

                if (vkResult != VK_SUCCESS)
                {
                    string message = 
                        "Failed to set shader '" + to_string(ID) + "' data when creating module '" 
                        + string(shaderType) + "' for shader '" + to_string(ID)
                        + "' under graphics context '" + to_string(contextID) + "! Reason: " 
                        + GraphicsContext::GetVkResultMessage(vkResult);

                    if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics shader error",
                            std::move(message));
                    }
                    else
                    {
                        Log::Print(
                            message,
                            "KG_SHADER",
                            LogType::LOG_ERROR,
                            2);
                    }

                    return { false };
                }

                SpvReflectShaderModule* spvShaderModule = new SpvReflectShaderModule{};
                SpvReflectResult reflResult = spvReflectCreateShaderModule(
                    outData.size(),
                    outData.data(),
                    spvShaderModule);

                if (reflResult != SPV_REFLECT_RESULT_SUCCESS)
                {
                    Log::Print(
                        "Failed to set shader '" + to_string(ID) + "' data when creating module '" 
                        + string(shaderType) + "' for shader '" + to_string(ID)
                        + "' under graphics context '" + to_string(contextID)
                        + "'! Reflect result error code: " + to_string(static_cast<int>(reflResult)),
                        "KG_SHADER",
                        LogType::LOG_ERROR,
                        2);

                    delete spvShaderModule;

                    return { false };
                }

                return 
                { 
                    .success = true, 
                    .vkModule = vkShaderModule, 
                    .spvModule = spvShaderModule
                };
            };

        //
        // MODULES
        //

        ShaderModuleData newShaderModuleData{};

        ShaderModule module_vert = create_shader_module("vertex", vertPath);
        if (!module_vert.success) return;
        else
        {
            newShaderModuleData.vkModule_vert = module_vert.vkModule;
            newShaderModuleData.spvModule_vert = FromVar(module_vert.spvModule);
        }

        ShaderModule module_frag = create_shader_module("fragment", fragPath);
        if (!module_frag.success)
        {
            DestroyVkShaderModules({ module_vert.vkModule });
            DestroySpvShaderModules({ module_vert.spvModule });

            return;
        }
        else
        {
            newShaderModuleData.vkModule_frag = module_frag.vkModule;
            newShaderModuleData.spvModule_frag = FromVar(module_frag.spvModule);
        }

        ShaderModule module_geom{};
        if (!geomPath.empty())
        {
            module_geom = create_shader_module("geometry", geomPath);
            if (!module_geom.success)
            {
                DestroyVkShaderModules(
                    { 
                        module_vert.vkModule,
                        module_frag.vkModule
                    });

                DestroySpvShaderModules(
                    {
                        module_vert.spvModule,
                        module_frag.spvModule
                    });

                return;
            }
            else
            {
                newShaderModuleData.usingGeom = true;

                newShaderModuleData.vkModule_geom = module_geom.vkModule;
                newShaderModuleData.spvModule_geom = FromVar(module_geom.spvModule);
            }
        }

        auto add_stage = [](
            vector<VkPipelineShaderStageCreateInfo>& stages,
            VkShaderStageFlagBits flag,
            VkShaderModule module) -> void
            {
                VkPipelineShaderStageCreateInfo stage{};
                stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage.stage = flag;
                stage.module = module;
                stage.pName = "main";
                stages.push_back(stage);
            };

        //
        // PUSH CONSTANT LAYOUT
        //

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = 0;
        pushConstantRange.offset = UINT32_MAX;
        pushConstantRange.size = 0;

        vector<SpvReflectShaderModule*> modules{};
        
        modules.push_back(ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_vert));
        modules.push_back(ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_frag));

        if (newShaderModuleData.usingGeom)
        {
            modules.push_back(ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_geom));
        }

        for (auto* mod : modules)
        {
            u32 count{};
            spvReflectEnumeratePushConstantBlocks(
                mod,
                &count,
                nullptr);

            for (u32 i = 0; i < count; ++i)
            {
                SpvReflectResult result{};
                const SpvReflectBlockVariable* pBlock = spvReflectGetPushConstantBlock(
                    mod,
                    i,
                    &result); 
                    
                if (result != SPV_REFLECT_RESULT_SUCCESS
                    || pBlock == nullptr)
                {
                    Log::Print(
                        "Failed to get push constant block '" + to_string(i) + "' "
                        "for shader stage " + to_string(scast<int>(mod->shader_stage))
                        + " in shader '" + to_string(ID) + "' "
                        " under graphics context '" + to_string(contextID) + "'!"
                        " SpvReflectResult: " + to_string(scast<int>(result)),
                        "KG_SHADER",
                        LogType::LOG_WARNING);

                    continue;
                }

                pushConstantRange.stageFlags |= scast<VkShaderStageFlags>(mod->shader_stage);

                u32 blockEnd = pBlock->offset + pBlock->size;
                pushConstantRange.offset = std::min(
                    pushConstantRange.offset,
                    pBlock->offset);

                u32 currentEnd = pushConstantRange.offset + pushConstantRange.size;
                pushConstantRange.size = 
                    std::max(currentEnd, blockEnd)
                    - pushConstantRange.offset;
            }
        }

        if (pushConstantRange.stageFlags == 0)
        {
            pushConstantRange.offset = 0;
            pushConstantRange.size = 0;
        }

        //
        // DESCRIPTOR SET LAYOUT
        //

        map<pair<u32, u32>, VkDescriptorSetLayoutBinding> bindingMap{};

        for (auto* mod : modules)
        {
            u32 bindingCount{};
            spvReflectEnumerateDescriptorBindings(
                mod,
                &bindingCount,
                nullptr);

            vector<SpvReflectDescriptorBinding*> reflBindings(bindingCount);
            spvReflectEnumerateDescriptorBindings(
                mod,
                &bindingCount,
                reflBindings.data());

            for (u32 i = 0; i < bindingCount; ++i)
            {
                auto* refl = reflBindings[i];
                auto key = make_pair(refl->set, refl->binding);

                auto& layoutBinding = bindingMap[key];
                layoutBinding.binding = refl->binding;
                layoutBinding.descriptorType = scast<VkDescriptorType>(refl->descriptor_type);
                layoutBinding.descriptorCount = std::max(
                    1u,
                    refl->count);
                layoutBinding.stageFlags |= 
                    scast<VkShaderStageFlags>(refl->accessed)
                    | scast<VkShaderStageFlags>(mod->shader_stage);
                layoutBinding.pImmutableSamplers = nullptr;
            }
        }

        map<u32, vector<VkDescriptorSetLayoutBinding>> bindingsBySet{};
        for (const auto& [key, layoutBinding] : bindingMap)
        {
            bindingsBySet[key.first].push_back(layoutBinding);
        }
    
        vector<VkDescriptorSetLayout> newDescriptorSetLayouts(bindingsBySet.size());
        u32 setIndex{};
        for (const auto& [set, setBindings] : bindingsBySet)
        {
            VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
            setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            setLayoutInfo.bindingCount = scast<u32>(setBindings.size());
            setLayoutInfo.pBindings = setBindings.data();

            VkResult vkResult = vkCreateDescriptorSetLayout(
                logicalDevice,
                &setLayoutInfo,
                nullptr,
                &newDescriptorSetLayouts[setIndex]);

            if (vkResult != VK_SUCCESS)
            {
                //destroy already created descriptor set layouts
                for (u32 i = 0; i < setIndex; ++i)
                {
                    vkDestroyDescriptorSetLayout(
                        logicalDevice,
                        newDescriptorSetLayouts[i],
                        nullptr);
                }

                vector<VkShaderModule> badVkShaders = 
                {
                    newShaderModuleData.vkModule_vert,
                    newShaderModuleData.vkModule_frag
                };
                if (newShaderModuleData.usingGeom)
                {
                    badVkShaders.push_back(newShaderModuleData.vkModule_geom);
                }

                vector<SpvReflectShaderModule*> badSpvShaders =
                {
                    ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_vert),
                    ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_frag)
                };
                if (newShaderModuleData.usingGeom)
                {
                    badSpvShaders.push_back(ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_geom));
                }

                DestroyVkShaderModules(badVkShaders);
                DestroySpvShaderModules(badSpvShaders);

                string message =
                    "Failed to create descriptor set layout for set " + to_string(set)
                    + " when assigning new shader data for shader '" + to_string(ID)
                    + "' under graphics context '" + to_string(contextID) + "'! Reason: "
                    + GraphicsContext::GetVkResultMessage(vkResult);

                if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics shader error", 
                        std::move(message));
                }
                else
                {
                    Log::Print(
                        message, 
                        "KG_SHADER", 
                        LogType::LOG_ERROR, 
                        2);
                }

                return;
            }
            ++setIndex;
        }
        
        //
        // PIPELINE LAYOUT
        //

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = scast<u32>(newDescriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts            = newDescriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = (pushConstantRange.size > 0) ? 1u : 0u;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout newPipelineLayout{};
        VkResult vkResult = vkCreatePipelineLayout(
            logicalDevice,
            &pipelineLayoutInfo,
            nullptr,
            &newPipelineLayout);

        if (vkResult != VK_SUCCESS)
        {
            for (auto& sl : newDescriptorSetLayouts)
            {
                vkDestroyDescriptorSetLayout(
                    logicalDevice,
                    sl,
                    nullptr);
            }

            vector<VkShaderModule> badVkShaders = 
            {
                newShaderModuleData.vkModule_vert,
                newShaderModuleData.vkModule_frag
            };
            if (newShaderModuleData.usingGeom)
            {
                badVkShaders.push_back(newShaderModuleData.vkModule_geom);
            }

            vector<SpvReflectShaderModule*> badSpvShaders =
            {
                ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_vert),
                ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_frag)
            };
            if (newShaderModuleData.usingGeom)
            {
                badSpvShaders.push_back(ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_geom));
            }

            DestroyVkShaderModules(badVkShaders);
            DestroySpvShaderModules(badSpvShaders);

            string message = 
                "Failed to create pipeline layout when assigning new shader data for shader '" + to_string(ID) 
                + "' under graphics context '" + to_string(contextID) + "'! Reason: " 
                + GraphicsContext::GetVkResultMessage(vkResult);

            if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    std::move(message));
            }
            else
            {
                Log::Print(
                    message,
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            }

            return;
        }

        //
        // PIPELINE
        //

        vector<VkPipelineShaderStageCreateInfo> stages{};

        add_stage(
            stages,
            VK_SHADER_STAGE_VERTEX_BIT,
            newShaderModuleData.vkModule_vert);
        add_stage(
            stages,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            newShaderModuleData.vkModule_frag);

        if (newShaderModuleData.usingGeom)
        {
            add_stage(
                stages,
                VK_SHADER_STAGE_GEOMETRY_BIT,
                newShaderModuleData.vkModule_geom);
        }

        unique_ptr<PipelineInfo> newPipelineInfo = GetPipelineInfo(
            is2D,
            std::move(stages),
            newPipelineLayout,
            gctx->GetRenderPass());

        VkPipeline newPipeline{};
        vkResult = vkCreateGraphicsPipelines(
            logicalDevice,
            VK_NULL_HANDLE,
            1,
            &newPipelineInfo->pipelineInfo,
            nullptr,
            &newPipeline);

        if (vkResult != VK_SUCCESS)
        {
            vkDestroyPipelineLayout(
                logicalDevice,
                newPipelineLayout,
                nullptr);

            for (auto& sl : newDescriptorSetLayouts)
            {
                vkDestroyDescriptorSetLayout(
                    logicalDevice,
                    sl,
                    nullptr);
            }

            vector<VkShaderModule> badVkShaders = 
            {
                newShaderModuleData.vkModule_vert,
                newShaderModuleData.vkModule_frag
            };
            if (newShaderModuleData.usingGeom)
            {
                badVkShaders.push_back(newShaderModuleData.vkModule_geom);
            }

            vector<SpvReflectShaderModule*> badSpvShaders =
            {
                ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_vert),
                ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_frag)
            };
            if (newShaderModuleData.usingGeom)
            {
                badSpvShaders.push_back(ToVar<SpvReflectShaderModule*>(newShaderModuleData.spvModule_geom));
            }

            DestroyVkShaderModules(badVkShaders);
            DestroySpvShaderModules(badSpvShaders);

            string message = 
                "Failed to create new pipeline when assigning new shader data for shader '" + to_string(ID) 
                + "' under graphics context '" + to_string(contextID) + "'! Reason: " 
                + GraphicsContext::GetVkResultMessage(vkResult);

            if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    std::move(message));
            }
            else
            {
                Log::Print(
                    message,
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            }

            return;
        }

        //
        // DELETE OLD DATA
        //

        if (pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(
                logicalDevice,
                pipeline,
                nullptr);
        }
        if (pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(
                logicalDevice,
                pipelineLayout,
                nullptr);
        }
        if (!descriptorSetLayouts.empty())
        {
            for (auto& sl : descriptorSetLayouts)
            {
                vkDestroyDescriptorSetLayout(
                    logicalDevice,
                    sl,
                    nullptr);
            }
        }

        if (shaderModuleData.vkModule_vert != VK_NULL_HANDLE)
        {
            vector<VkShaderModule> oldVkShaders = 
            {
                shaderModuleData.vkModule_vert,
                shaderModuleData.vkModule_frag
            };
            if (shaderModuleData.usingGeom)
            {
                oldVkShaders.push_back(shaderModuleData.vkModule_geom);
            }

            vector<SpvReflectShaderModule*> oldSpvShaders =
            {
                ToVar<SpvReflectShaderModule*>(shaderModuleData.spvModule_vert),
                ToVar<SpvReflectShaderModule*>(shaderModuleData.spvModule_frag)
            };
            if (shaderModuleData.usingGeom)
            {
                oldSpvShaders.push_back(ToVar<SpvReflectShaderModule*>(shaderModuleData.spvModule_geom));
            }

            DestroyVkShaderModules(oldVkShaders);
            DestroySpvShaderModules(oldSpvShaders);
        }

        //
        // FINISH
        //

        pipeline = newPipeline;
        pipelineLayout = newPipelineLayout;
        descriptorSetLayouts = std::move(newDescriptorSetLayouts);
        shaderModuleData = std::move(newShaderModuleData);

        Log::Print(
            "Updated shader '" + to_string(ID) + "' data!",
            "KG_SHADER",
            LogType::LOG_SUCCESS);
    }

    void Shader::Update(VkCommandBuffer cmdBuffer)
    {
        if (pipeline == VK_NULL_HANDLE)
        {
            if (missingPipelineWarningCount < 10)
            {
                Log::Print(
                    "Failed to render onto shader '" + to_string(ID) + "' "
                    "because the render pipeline is missing! "
                    "You should set shader data to use this shader. "
                    "This warning will only be given 10 times.",
                    "KG_SHADER",
                    LogType::LOG_WARNING);

                missingPipelineWarningCount++;
            }

            return;
        }

        if (meshIDs.empty())
        {
            if (missingMeshWarningCount < 10)
            {
                Log::Print(
                    "Failed to render onto shader '" + to_string(ID) + "' "
                    "because there are no meshes to draw! This warning will only be given 10 times.",
                    "KG_SHADER",
                    LogType::LOG_WARNING);

                missingMeshWarningCount++;
            }

            return;
        }

        vkCmdBindPipeline(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline);

        for (u32 cameraID : cameraIDs)
        {
            Camera* camera = Camera::GetRegistry().GetContent(cameraID);
            if (!camera)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    "Failed to update shader '" + to_string(ID) 
                    + "' because its camera '" + to_string(cameraID) + "' was invalid!");
            }

            if (camera->isActiveCamera)
            {
                vkCmdBindDescriptorSets(
                    cmdBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout,
                    0, // <<<< SET 0 BINDING 0 - CAMERA UBO SLOT
                    1,
                    &camera->vkDescriptorSet,
                    0,
                    nullptr);

                break;
            }
        }

        for (u32 meshID : meshIDs)
        {
            Mesh* mesh = Mesh::GetRegistry().GetContent(meshID);
            if (!mesh)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    "Failed to update shader '" + to_string(ID) 
                    + "' because its mesh '" + to_string(meshID) + "' was invalid!");
            }

            if (mesh->vkVertexBuffer == VK_NULL_HANDLE)
            {
                //skip mesh if it has no vertex buffer data
                if (mesh->verticesSize == 0) continue;
                //invalid mesh, vertex buffer was removed for calculated mesh data
                else
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics shader error",
                        "Failed to render mesh '" + to_string(meshID) 
                        + "' on shader '" + to_string(ID) 
                        + "' because its vertex buffer size is more than 0 but it "
                        "doesn't have a valid vertex buffer!");
                }
            }
            //vertex buffer was added but its data was not assigned
            else if (mesh->verticesSize == 0)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    "Failed to render mesh '" + to_string(meshID) 
                    + "' on shader '" + to_string(ID) 
                    + "' because its vertex buffer is valid but it "
                    "doesn't have vertex buffer data!");
            }

            vkCmdBindDescriptorSets(
                cmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                1, // <<<< SET 1 BINDING 0 - MESH UBO SLOT
                1,
                &mesh->vkDescriptorSet,
                0,
                nullptr);

            Texture* texture = Texture::GetRegistry().GetContent(mesh->textureID);
            if (!texture)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    "Failed to render mesh '" + to_string(meshID) 
                    + "' on shader '" + to_string(ID) 
                    + "' because its texture '" + to_string(mesh->textureID) + "' was invalid!");
            }

            vkCmdBindDescriptorSets(
                cmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                2, // <<<< SET 2 BINDING 0 - TEXTURE SAMPLER SLOT
                1,
                &texture->vkDescriptorSet,
                0,
                nullptr);

            VkDeviceSize offset{};
            vkCmdBindVertexBuffers(
                cmdBuffer,
                0,
                1,
                &mesh->vkVertexBuffer,
                &offset);

            if (mesh->vkIndexBuffer == VK_NULL_HANDLE)
            {
                vkCmdDraw(
                    cmdBuffer,
                    mesh->vertices.size(),
                    1,
                    0,
                    0);
            }
            else
            {
                //vertex buffer was added but its data was not assigned
                if (mesh->indicesSize == 0)
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics shader error",
                        "Failed to render mesh '" + to_string(meshID) 
                        + "' on shader '" + to_string(ID) 
                        + "' because its index buffer is valid but it "
                        "doesn't have index buffer data!");
                }
                else
                {
                    vkCmdBindIndexBuffer(
                        cmdBuffer,
                        mesh->vkIndexBuffer,
                        0,
                        VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(
                        cmdBuffer,
                        mesh->indices.size(),
                        1,
                        0,
                        0,
                        0);
                }
            }
        }
    }

    void Shader::DestroyVkShaderModules(vector<VkShaderModule> modules)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();

        for (const auto& m : modules)
        {
            vkDestroyShaderModule(
                logicalDevice,
                m,
                nullptr);
        }
    };

    void Shader::Destroy()
    {
        for (u32 cID : cameraIDs)
        {
            Camera* c = Camera::GetRegistry().GetContent(cID);
            if (c) c->shaderID = 0;
        }
        cameraIDs.clear();

        for (u32 tID : textureIDs)
        {
            Texture* t = Texture::GetRegistry().GetContent(tID);
            if (t) t->shaderID = 0;
        }
        textureIDs.clear();

        for (u32 mID : meshIDs)
        {
            Mesh* m = Mesh::GetRegistry().GetContent(mID);
            if (m) m->shaderID = 0;
        }
        meshIDs.clear();

        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (gctx
            && !isDestroyingGraphicsContext)
        {
            erase(
                gctx->shaderIDs, 
                ID);
        }

        registry.RemoveContent(ID);
    }

    Shader::~Shader()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();

        //drain the gpu before destroying this shader
        if (logicalDevice != VK_NULL_HANDLE) 
        {
            VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
            if (vkResult != VK_SUCCESS)
            {
                GraphicsContext::ForceClose(
                    "KalaGraphics shader error",
                    "Failed to destroy shader '" 
                    + to_string(ID) + "' because vkDeviceWaitIdle did not succeed!",
                    vkResult);
            }
        }

        Log::Print(
			"Destroying shader '" + to_string(ID) + "'.",
			"KG_SHADER",
			LogType::LOG_INFO);

        vkDestroyPipeline(
            logicalDevice,
            pipeline,
            nullptr);
        vkDestroyPipelineLayout(
            logicalDevice,
            pipelineLayout,
            nullptr);

        for (auto& sl : descriptorSetLayouts)
        {
            vkDestroyDescriptorSetLayout(
                logicalDevice,
                sl,
                nullptr);
        }

        if (shaderModuleData.vkModule_vert != VK_NULL_HANDLE)
        {
            vector<VkShaderModule> oldVkShaders = 
            {
                shaderModuleData.vkModule_vert,
                shaderModuleData.vkModule_frag
            };
            if (shaderModuleData.usingGeom)
            {
                oldVkShaders.push_back(shaderModuleData.vkModule_geom);
            }

            vector<SpvReflectShaderModule*> oldSpvShaders =
            {
                ToVar<SpvReflectShaderModule*>(shaderModuleData.spvModule_vert),
                ToVar<SpvReflectShaderModule*>(shaderModuleData.spvModule_frag)
            };
            if (shaderModuleData.usingGeom)
            {
                oldSpvShaders.push_back(ToVar<SpvReflectShaderModule*>(shaderModuleData.spvModule_geom));
            }

            DestroyVkShaderModules(oldVkShaders);
            DestroySpvShaderModules(oldSpvShaders);
        }
    }
}