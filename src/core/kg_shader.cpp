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

#include "core/kg_shader.hpp"
#include "resources/kg_mesh.hpp"
#include "resources/kg_texture.hpp"
#include "resources/kg_camera.hpp"
#include "core/kg_context.hpp"
#include "core/kg_viewport.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::FromVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::Viewport;
using KalaGraphics::Core::Shader;
using KalaGraphics::Core::Severity;
using KalaGraphics::Resources::Vertex;
using KalaGraphics::Resources::Vertex2D;
using KalaGraphics::Resources::Mesh;
using KalaGraphics::Resources::Texture;
using KalaGraphics::Resources::Camera;

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
using std::filesystem::absolute;

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
    pi->rasterization.cullMode    = is2D ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
    pi->rasterization.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    pi->rasterization.lineWidth   = 1.0f;

    //TODO: add proper multisampling/MSAA

    pi->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pi->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    pi->depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    if (!is2D)
    {
        pi->depthStencil.depthTestEnable  = VK_TRUE;
        pi->depthStencil.depthWriteEnable = VK_TRUE;
        pi->depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;
    }
    else
    {
        pi->depthStencil.depthTestEnable  = VK_FALSE;
        pi->depthStencil.depthWriteEnable = VK_FALSE;
        pi->depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;
    }

    pi->colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;

    pi->colorBlendAttachment.blendEnable = VK_TRUE;
    pi->colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    pi->colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pi->colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    pi->colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pi->colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pi->colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

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

namespace KalaGraphics::Core
{
    static KalaGraphicsRegistry<Shader> registry{};

    KalaGraphicsRegistry<Shader>& Shader::GetRegistry() { return registry; }

    Shader* Shader::Initialize(
        u32 viewportID,
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
                "Failed to initialize shader because the logical device was invalid!");
        }

        Viewport* vp{};
        string err = Viewport::GetRegistry().GetContent(viewportID, vp);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize shader because its viewport was invalid! Reason: " + err,
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        GraphicsContext* gctx{};
        err = GraphicsContext::GetRegistry().GetContent(vp->contextID, gctx);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize shader because its viewports graphics context was invalid! Reason: " + err,
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        auto empty_path = [](string_view shaderType) -> void
            {
                Log::Print(
                    "Failed to initialize shader because it did not contain a " 
                    + string(shaderType) + " shader file!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto bad_ext = [](string_view shaderType) -> void
            {
                Log::Print(
                    "Failed to initialize shader because its " + string(shaderType) 
                    + " shader had a missing or incorrect extension!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto invalid_path = [](
            string_view shaderType,
            string_view shaderPath) -> void
            {
                Log::Print(
                    "Failed to initialize shader because its " + string(shaderType) 
                    + " shader path '" + string(shaderPath) + "' was invalid!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto root_shader = [viewportID](const pair<path, path>& shaders) -> void
            {
                Log::Print(
                    "Failed to initialize shader because its "
                    "vertex shader '" + shaders.first.string() + "' and "
                    "frag shader '" + shaders.second.string() + "' "
                    "are already used in an existing root shader in viewport '" + to_string(viewportID) + "'!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };

        if (vertPath.empty())
        {
            empty_path("vertex");
            return nullptr;
        }
        if (!vertPath.has_extension()
            || vertPath.extension() != ".spv")
        {
            bad_ext("vertex");
            return nullptr;
        }
        if (!exists(vertPath))
        {
            invalid_path("vertex", vertPath.string());
            return nullptr;
        }

        if (fragPath.empty())
        {
            empty_path("fragment");
            return nullptr;
        }
        if (!fragPath.has_extension()
            || fragPath.extension() != ".spv")
        {
            bad_ext("fragment");
            return nullptr;
        }
        if (!exists(fragPath))
        {
            invalid_path("fragment", fragPath.string());
            return nullptr;
        }

        if (!geomPath.empty())
        {
            if (!geomPath.has_extension()
                || geomPath.extension() != ".spv")
            {
                bad_ext("geometry");
                return nullptr;
            }
            if (!exists(geomPath))
            {
                invalid_path("geometry", geomPath.string());
                return nullptr;
            }
        }

        vector<const RootShader*> rootShaders = vp->GetAllRootShaders();

        path absVert = absolute(vertPath);
        path absFrag = absolute(fragPath);

        for (const RootShader* rs : rootShaders)
        {
            //skip not-yet-initialized root shaders
            if (rs->shaderID == 0) continue;

            Shader* s{};
            string err = Shader::GetRegistry().GetContent(rs->shaderID, s);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    "Failed to initialize shader during root shader '" 
                    + to_string(rs->shaderID) + "' verification because it was invalid!");
            }
            
            /*
            Log::Print(
                "@@@@@\n"
                "this vert: " + absVert.string() + "\n"
                "this frag: " + absFrag.string() + "\n"
                "root vert: " + rs->vertShader.shaderPath.string() + "\n"
                "root frag: " + rs->fragShader.shaderPath.string());
            */

            if (absVert == rs->vertShader.shaderPath
                && absFrag == rs->fragShader.shaderPath)
            {
                root_shader({ vertPath, fragPath });

                return nullptr;
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

                return nullptr;
            }
        }

        auto create_shader_module = [&logicalDevice](
            string_view shaderType, 
            const path& shaderPath) -> ShaderModule
            {
                vector<u8> outData{};
                string errMsg = ReadBinaryDataFromFile(shaderPath, outData);

                if (!errMsg.empty())
                {
                    Log::Print(
                        "Failed to initialize shader because binary data "
                        "couldn't be read from shader type " + string(shaderType) + "! Reason : " + errMsg,
                        "KG_SHADER",
                        LogType::LOG_ERROR,
                        2);

                    return { false };
                }

                VkShaderModuleCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.codeSize = outData.size();
                createInfo.pCode = rcast<const u32*>(outData.data());

                VkShaderModule vkShaderModule{};
                VkResult vkResult = vkCreateShaderModule(
                    logicalDevice,
                    &createInfo,
                    nullptr,
                    &vkShaderModule);

                if (vkResult != VK_SUCCESS)
                {
                    string message = 
                        "Failed to initialize shader because module '" 
                        + string(shaderType) + "' creation failed! Reason: " 
                        + GraphicsContext::GetVkResultMessage(vkResult);

                    if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::SEVERITY_FATAL)
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

                    return {};
                }

                SpvReflectShaderModule* spvShaderModule = new SpvReflectShaderModule{};
                SpvReflectResult reflResult = spvReflectCreateShaderModule(
                    outData.size(),
                    outData.data(),
                    spvShaderModule);

                if (reflResult != SPV_REFLECT_RESULT_SUCCESS)
                {
                    Log::Print(
                        "Failed to initialize shader because module '" 
                        + string(shaderType) + "' failed! Reflect result error code: " 
                        + to_string(static_cast<int>(reflResult)),
                        "KG_SHADER",
                        LogType::LOG_ERROR,
                        2);

                    delete spvShaderModule;

                    return {};
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
        if (!module_vert.success) return nullptr;
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

            return nullptr;
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

                return nullptr;
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
                        + " when initializing shader!"
                        "SpvReflectResult: " + to_string(scast<int>(result)),
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
                    "Failed to initialize shader because descriptor set layout creation for set " + to_string(set)
                    + " failed! Reason: " + GraphicsContext::GetVkResultMessage(vkResult);

                if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::SEVERITY_FATAL)
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

                return nullptr;
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
        pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

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
                "Failed to initialize shader because pipeline layout creation failed! Reason: " 
                + GraphicsContext::GetVkResultMessage(vkResult);

            if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::SEVERITY_FATAL)
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

            return nullptr;
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

        VkFormat colorFormat = scast<VkFormat>(gctx->GetDefaultColorFormat());
        VkFormat depthFormat = scast<VkFormat>(gctx->GetDefaultDepthFormat());

        VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
        pipelineRenderingInfo.depthAttachmentFormat = depthFormat;
        pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        unique_ptr<PipelineInfo> newPipelineInfo = GetPipelineInfo(
            is2D,
            std::move(stages),
            newPipelineLayout,
            VK_NULL_HANDLE);

        newPipelineInfo->pipelineInfo.pNext = &pipelineRenderingInfo;
        newPipelineInfo->pipelineInfo.renderPass = VK_NULL_HANDLE;

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
                "Failed to initialize shader because pipeline creation failed! Reason: " 
                + GraphicsContext::GetVkResultMessage(vkResult);

            if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::SEVERITY_FATAL)
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

            return nullptr;
        }

        //
        // FINISH
        //

        unique_ptr<Shader> newShader = make_unique<Shader>();
        Shader* shaderPtr = newShader.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        shaderPtr->ID = newID;
        shaderPtr->is2D = is2D;
        shaderPtr->pipeline = newPipeline;
        shaderPtr->pipelineLayout = newPipelineLayout;
        shaderPtr->descriptorSetLayouts = std::move(newDescriptorSetLayouts);
        shaderPtr->shaderModuleData = std::move(newShaderModuleData);

        if (!is2D) vp->shader3DIDs.push_back(newID);
        else       vp->shader2DIDs.push_back(newID);

        shaderPtr->is2D = is2D;
        shaderPtr->viewportID = viewportID;

        err = registry.AddContent(newID, std::move(newShader));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics shader error",
				"Failed to initialize shader! Reason: " + err);
        }

        Log::Print(
			"Created new shader '" + to_string(newID) + "'!",
			"KG_SHADER",
			LogType::LOG_SUCCESS);

        return shaderPtr;
    }

    u32 Shader::GetID() const { return ID; }
    u32 Shader::GetViewportID() const { return viewportID; }
    const vector<u32>& Shader::GetMeshIDs() const { return meshIDs; }
    const vector<u32>& Shader::GetTextureIDs() const { return textureIDs; }
    const vector<u32>& Shader::GetCameraIDs() const { return cameraIDs; }

    bool Shader::Is2D() const { return is2D; }

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
        Viewport* vp{};
        string err = Viewport::GetRegistry().GetContent(viewportID, vp);

        if (isRootShader
            && !overrideRootDeletePermission
            && err.empty())
        {
            Log::Print(
                "Failed to delete shader '" + to_string(ID) 
                + "' because it is a root shader of viewport '" + to_string(viewportID) 
                + "' and it is required for normal operation of KalaGraphics!",
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (err.empty()
            && !isDestroyingViewport)
        {
            if (!is2D)
            {
                Camera* c{};
                string err = Camera::GetRegistry().GetContent(vp->primary3DCameraID, c);
                if (!err.empty())
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics shader error",
                        "Failed to destroy shader '" + to_string(ID) 
                        + "' because viewport '" + to_string(viewportID) 
                        + "' primary 3D camera was invalid! Reason: " + err);
                }

                if (c->shaderID == ID)
                {
                    Log::Print(
                        "Failed to delete shader '" + to_string(ID) 
                        + "' because it belongs to the primary 3D camera of viewport '" + to_string(viewportID) + "'!",
                        "KG_SHADER",
                        LogType::LOG_ERROR,
                        2);

                    return;
                }

                erase(
                    vp->shader3DIDs, 
                    ID);
            }
            else
            {
                Camera* c{};
                string err = Camera::GetRegistry().GetContent(vp->primary2DCameraID, c);
                if (!err.empty())
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics shader error",
                        "Failed to destroy shader '" + to_string(ID) 
                        + "' because viewport '" + to_string(viewportID) 
                        + "' primary 2D camera was invalid! Reason: " + err);
                }

                if (c->shaderID == ID)
                {
                    Log::Print(
                        "Failed to delete shader '" + to_string(ID) 
                        + "' because it belongs to the primary 2D camera of viewport '" + to_string(viewportID) + "'!",
                        "KG_SHADER",
                        LogType::LOG_ERROR,
                        2);

                    return;
                }

                erase(
                    vp->shader2DIDs, 
                    ID);
            }

            if (vp->lastBoundShaderID == ID) vp->lastBoundShaderID = 0;
        }

        for (u32 cID : cameraIDs)
        {
            Camera* c{};
            string err = Camera::GetRegistry().GetContent(cID, c);
            if (err.empty()) c->shaderID = 0;
        }
        cameraIDs.clear();

        for (u32 tID : textureIDs)
        {
            Texture* t{};
            string err = Texture::GetRegistry().GetContent(tID, t);
            if (err.empty()) t->shaderID = 0;
        }
        textureIDs.clear();

        for (u32 mID : meshIDs)
        {
            Mesh* m{};
            string err = Mesh::GetRegistry().GetContent(mID, m);
            if (err.empty()) m->shaderID = 0;
        }
        meshIDs.clear();

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to destroy shader '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Shader::~Shader()
    {
        Log::Print(
			"Destroying shader '" + to_string(ID) + "'.",
			"KG_SHADER",
			LogType::LOG_INFO);

        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE) 
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to clear shader '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        //drain the gpu before destroying this shader
        VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
        if (vkResult != VK_SUCCESS)
        {
            GraphicsContext::ForceClose(
                "KalaGraphics shader error",
                "Failed to destroy shader '" 
                + to_string(ID) + "' because vkDeviceWaitIdle did not succeed!",
                vkResult);
        }

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