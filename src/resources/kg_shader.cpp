//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>
#include <vector>
#include <unordered_map>

#include "vulkan/vulkan_core.h"

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "resources/kg_shader.hpp"
#include "resources/kg_mesh.hpp"
#include "resources/kg_camera.hpp"
#include "core/kg_context.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::Severity;

using std::unique_ptr;
using std::make_unique;
using std::to_string;
using std::vector;
using std::unordered_map;
using std::string;
using std::string_view;
using std::array;

using u8 = uint8_t;

struct ShaderModule
{
    bool success{};
    VkShaderModule module{};
};

namespace KalaGraphics::Resources
{
    struct ShaderPipelineRecreateData
    {
        //the template - render pass is patched when recreating
        VkGraphicsPipelineCreateInfo pipelineInfo{};

        //owned arrays/structs that pipelineInfo pointers reference

        vector<VkPipelineShaderStageCreateInfo> stages{};
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        VkPipelineViewportStateCreateInfo viewportState{};
        VkPipelineRasterizationStateCreateInfo rasterization{};
        VkPipelineMultisampleStateCreateInfo multisampling{};
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo colorBlend{};

        //backing storage for pointer members within the above structs

        vector<VkDynamicState> dynamicStates{};
        VkVertexInputBindingDescription bindingDescription{};
        array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
    };

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

        recreateData->pipelineInfo.renderPass = newContext->GetRenderPass();

        VkGraphicsPipelineCreateInfo newPipelineInfo{};
        newPipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        newPipelineInfo.stageCount          = scast<u32>(recreateData->stages.size());
        newPipelineInfo.pStages             = recreateData->stages.data();
        newPipelineInfo.pVertexInputState   = &recreateData->vertexInput;
        newPipelineInfo.pInputAssemblyState = &recreateData->inputAssembly;
        newPipelineInfo.pViewportState      = &recreateData->viewportState;
        newPipelineInfo.pRasterizationState = &recreateData->rasterization;
        newPipelineInfo.pMultisampleState   = &recreateData->multisampling;
        newPipelineInfo.pDepthStencilState  = &recreateData->depthStencil;
        newPipelineInfo.pColorBlendState    = &recreateData->colorBlend;
        newPipelineInfo.pDynamicState       = &recreateData->dynamicState;
        newPipelineInfo.layout              = pipelineLayout;
        newPipelineInfo.renderPass          = newContext->GetRenderPass();
        newPipelineInfo.subpass             = 0;

        VkPipeline newPipeline{};
        VkResult vkResult = vkCreateGraphicsPipelines(
            logicalDevice,
            VK_NULL_HANDLE,
            1,
            &newPipelineInfo,
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

        //
        // FINISH
        //

        vkDestroyPipeline(
            logicalDevice,
            pipeline,
            nullptr);

        pipeline = newPipeline;

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
    const vector<u32>& Shader::GetCameraIDs() const { return cameraIDs; }

    void Shader::SetShaderData(
        ShaderData&& shaderData,
        vector<DescriptorBinding>&& descriptorBindings)
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

        if (shaderData.shader_vert.empty())
        {
            empty_path("vertex");
            return;
        }
        if (!shaderData.shader_vert.has_extension()
            || shaderData.shader_vert.extension() != ".spv")
        {
            bad_ext("vertex");
            return;
        }
        if (!exists(shaderData.shader_vert))
        {
            invalid_path("vertex", shaderData.shader_vert.string());
            return;
        }

        if (shaderData.shader_frag.empty())
        {
            empty_path("fragment");
            return;
        }
        if (!shaderData.shader_frag.has_extension()
            || shaderData.shader_frag.extension() != ".spv")
        {
            bad_ext("fragment");
            return;
        }
        if (!exists(shaderData.shader_frag))
        {
            invalid_path("fragment", shaderData.shader_frag.string());
            return;
        }

        if (!shaderData.shader_geom.empty())
        {
            if (!shaderData.shader_geom.has_extension()
                || shaderData.shader_geom.extension() != ".spv")
            {
                bad_ext("geometry");
                return;
            }
            if (!exists(shaderData.shader_geom))
            {
                invalid_path("geometry", shaderData.shader_geom.string());
                return;
            }
        }
        if (!shaderData.shader_tess_cont.empty())
        {
            if (!shaderData.shader_tess_cont.has_extension()
                || shaderData.shader_tess_cont.extension() != ".spv")
            {
                bad_ext("tesselation control");
                return;
            }
            if (!exists(shaderData.shader_tess_cont))
            {
                invalid_path("tesselation control", shaderData.shader_tess_cont.string());
                return;
            }
        }
        if (!shaderData.shader_tess_eval.empty())
        {
            if (!shaderData.shader_tess_eval.has_extension()
                || shaderData.shader_tess_eval.extension() != ".spv")
            {
                bad_ext("tesselation evaluation");
                return;
            }
            if (!exists(shaderData.shader_tess_eval))
            {
                invalid_path("tesselation evaluation", shaderData.shader_tess_eval.string());
                return;
            }
        }

        unordered_map<string, string> shaderPaths =
        {
            { "vertex",                 shaderData.shader_vert.string() },
            { "fragment",               shaderData.shader_frag.string() },
            { "geometry",               shaderData.shader_geom.string() },
            { "tesselation control",    shaderData.shader_tess_cont.string() },
            { "tesselation evaulation", shaderData.shader_tess_eval.string() }
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

                VkShaderModule shaderModule{};
                VkResult vkResult = vkCreateShaderModule(
                    logicalDevice,
                    &createInfo,
                    nullptr,
                    &shaderModule);

                if (vkResult != VK_SUCCESS)
                {
                    string message = 
                        "Failed to create shader '" + string(shaderType) 
                        + "' when assigning new shader data for shader '" + to_string(ID) 
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

                    return { false };
                }

                return { true, shaderModule};
            };

        auto destroy_shaders = [&logicalDevice](vector<VkShaderModule> modules) -> void
            {
                for (const auto& m : modules)
                {
                    vkDestroyShaderModule(
                        logicalDevice,
                        m,
                        nullptr);
                }
            };

        //
        // MODULES
        //

        ShaderModuleData newShaderModuleData{};

        ShaderModule module_vert = create_shader_module("vertex", shaderData.shader_vert);
        if (!module_vert.success) return;
        else newShaderModuleData.module_vert = module_vert.module;

        ShaderModule module_frag = create_shader_module("fragment", shaderData.shader_frag);
        if (!module_frag.success)
        {
            destroy_shaders({ module_vert.module });

            return;
        }
        else newShaderModuleData.module_frag = module_frag.module;

        ShaderModule module_geom{};
        if (!shaderData.shader_geom.empty())
        {
            module_geom = create_shader_module("geometry", shaderData.shader_geom);
            if (!module_geom.success)
            {
                destroy_shaders(
                    { 
                        module_vert.module,
                        module_frag.module
                    });

                return;
            }
            else
            {
                newShaderModuleData.usingGeom = true;
                newShaderModuleData.module_geom = module_geom.module;
            }
        }
        ShaderModule module_tess_cont{};
        if (!shaderData.shader_tess_cont.empty())
        {
            module_tess_cont = create_shader_module("tesselation control", shaderData.shader_tess_cont);
            if (!module_tess_cont.success)
            {
                vector<VkShaderModule> badShaders = 
                    {
                        module_vert.module,
                        module_frag.module
                    };

                if (!shaderData.shader_geom.empty()) badShaders.push_back(module_geom.module);

                destroy_shaders(badShaders);

                return;
            }
            else
            {
                newShaderModuleData.usingTessCont = true;
                newShaderModuleData.module_tess_cont = module_tess_cont.module;
            }
        }
        ShaderModule module_tess_eval{};
        if (!shaderData.shader_tess_eval.empty())
        {
            module_tess_eval = create_shader_module("tesselation evaluation", shaderData.shader_tess_eval);
            if (!module_tess_eval.success)
            {
                vector<VkShaderModule> badShaders = 
                    {
                        module_vert.module,
                        module_frag.module
                    };

                if (!shaderData.shader_geom.empty())      badShaders.push_back(module_geom.module);
                if (!shaderData.shader_tess_cont.empty()) badShaders.push_back(module_tess_cont.module);

                destroy_shaders(badShaders);

                return;
            }
            else
            {
                newShaderModuleData.usingTessEval = true;
                newShaderModuleData.module_tess_eval = module_tess_eval.module;
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

        VkDescriptorSetLayout newDescriptorSetLayout{};
        VkPipelineLayout newPipelineLayout{};
        VkGraphicsPipelineCreateInfo newPipelineInfo{};

        //
        // RECREATE DATA DOESNT EXIST, START FROM SCRATCH
        //

        bool recreate = !recreateData;
        if (recreate)
        {
            //
            // PUSH CONSTANT LAYOUT
            //

            //TODO: use spirv reflection

            VkPushConstantRange pushConstantRanges[1]{};

            pushConstantRanges[0].stageFlags = 
                VK_SHADER_STAGE_VERTEX_BIT 
                | VK_SHADER_STAGE_FRAGMENT_BIT;
            pushConstantRanges[0].offset = 0;
            pushConstantRanges[0].size = 
                sizeof(REPLACE_ME_TEST_SHADER_DATA);

            //
            // DESCRIPTOR SET LAYOUT
            //

            VkDescriptorSetLayoutBinding bindings[1]{};

            //camera UBO
            bindings[0].binding = 0;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            bindings[0].pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
            descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorLayoutInfo.bindingCount = 1;
            descriptorLayoutInfo.pBindings = bindings;

            VkResult vkResult = vkCreateDescriptorSetLayout(
                logicalDevice,
                &descriptorLayoutInfo,
                nullptr,
                &newDescriptorSetLayout);

            if (vkResult != VK_SUCCESS)
            {
                vector<VkShaderModule> badShaders = 
                    {
                        module_vert.module,
                        module_frag.module
                    };

                if (!shaderData.shader_geom.empty())      badShaders.push_back(module_geom.module);
                if (!shaderData.shader_tess_cont.empty()) badShaders.push_back(module_tess_cont.module);
                if (!shaderData.shader_tess_eval.empty()) badShaders.push_back(module_tess_eval.module);

                destroy_shaders(badShaders);

                string message = 
                    "Failed to create descriptor set layout when assigning new shader data for shader '" + to_string(ID) 
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
            // PIPELINE LAYOUT
            //

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount         = 1; //TODO: update dynamically for descriptor set count
            pipelineLayoutInfo.pSetLayouts            = &newDescriptorSetLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1; //TODO: update dynamically for push constant count
            pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

            vkResult = vkCreatePipelineLayout(
                logicalDevice,
                &pipelineLayoutInfo,
                nullptr,
                &newPipelineLayout);

            if (vkResult != VK_SUCCESS)
            {
                vector<VkShaderModule> badShaders = 
                    {
                        module_vert.module,
                        module_frag.module
                    };

                if (!shaderData.shader_geom.empty())      badShaders.push_back(module_geom.module);
                if (!shaderData.shader_tess_cont.empty()) badShaders.push_back(module_tess_cont.module);
                if (!shaderData.shader_tess_eval.empty()) badShaders.push_back(module_tess_eval.module);

                destroy_shaders(badShaders);

                vkDestroyDescriptorSetLayout(
                    logicalDevice,
                    newDescriptorSetLayout,
                    nullptr);

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

            unique_ptr<ShaderPipelineRecreateData> rcData = make_unique<ShaderPipelineRecreateData>();

            add_stage(
                rcData->stages,
                VK_SHADER_STAGE_VERTEX_BIT,
                newShaderModuleData.module_vert);
            add_stage(
                rcData->stages,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                newShaderModuleData.module_frag);

            if (newShaderModuleData.usingGeom)
            {
                add_stage(
                    rcData->stages,
                    VK_SHADER_STAGE_GEOMETRY_BIT,
                    newShaderModuleData.module_geom);
            }
            if (newShaderModuleData.usingTessCont)
            {
                add_stage(
                    rcData->stages,
                    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                    newShaderModuleData.module_tess_cont);
            }
            if (newShaderModuleData.usingTessEval)
            {
                add_stage(
                    rcData->stages,
                    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                    newShaderModuleData.module_tess_eval);
            }

            rcData->bindingDescription.binding   = 0;
            rcData->bindingDescription.stride    = sizeof(Vertex);
            rcData->bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            rcData->attributeDescriptions = array<VkVertexInputAttributeDescription, 3>{
            {
                { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
                { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) },
                { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) }
            }};

            rcData->vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            rcData->vertexInput.vertexBindingDescriptionCount   = 1;
            rcData->vertexInput.pVertexBindingDescriptions      = &rcData->bindingDescription;
            rcData->vertexInput.vertexAttributeDescriptionCount = 3;
            rcData->vertexInput.pVertexAttributeDescriptions    = rcData->attributeDescriptions.data();

            rcData->inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            rcData->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            rcData->dynamicStates = 
            {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };
            rcData->dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            rcData->dynamicState.dynamicStateCount = scast<u32>(rcData->dynamicStates.size());
            rcData->dynamicState.pDynamicStates    = rcData->dynamicStates.data();

            rcData->viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            rcData->viewportState.viewportCount = 1;
            rcData->viewportState.scissorCount  = 1;

            rcData->rasterization.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rcData->rasterization.polygonMode = VK_POLYGON_MODE_FILL;
            rcData->rasterization.cullMode    = VK_CULL_MODE_BACK_BIT;
            rcData->rasterization.frontFace   = VK_FRONT_FACE_CLOCKWISE;
            rcData->rasterization.lineWidth   = 1.0f;

            rcData->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            rcData->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            rcData->depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            rcData->depthStencil.depthTestEnable  = VK_TRUE;
            rcData->depthStencil.depthWriteEnable = VK_TRUE;
            rcData->depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

            rcData->colorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT;
            rcData->colorBlendAttachment.blendEnable = VK_FALSE;

            rcData->colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            rcData->colorBlend.attachmentCount = 1;
            rcData->colorBlend.pAttachments    = &rcData->colorBlendAttachment;

            newPipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            newPipelineInfo.stageCount          = scast<u32>(rcData->stages.size());
            newPipelineInfo.pStages             = rcData->stages.data();
            newPipelineInfo.pVertexInputState   = &rcData->vertexInput;
            newPipelineInfo.pInputAssemblyState = &rcData->inputAssembly;
            newPipelineInfo.pViewportState      = &rcData->viewportState;
            newPipelineInfo.pRasterizationState = &rcData->rasterization;
            newPipelineInfo.pMultisampleState   = &rcData->multisampling;
            newPipelineInfo.pDepthStencilState  = &rcData->depthStencil;
            newPipelineInfo.pColorBlendState    = &rcData->colorBlend;
            newPipelineInfo.pDynamicState       = &rcData->dynamicState;
            newPipelineInfo.layout              = newPipelineLayout;
            newPipelineInfo.renderPass          = gctx->GetRenderPass();
            newPipelineInfo.subpass             = 0;

            recreateData = std::move(rcData);
        }

        //
        // RECREATE DATA EXISTS, DONT RECREATE EVERYTHING
        //

        else
        {
            //clear old data before assigning new stages data
            recreateData->stages.clear();

            add_stage(
                recreateData->stages,
                VK_SHADER_STAGE_VERTEX_BIT,
                newShaderModuleData.module_vert);
            add_stage(
                recreateData->stages,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                newShaderModuleData.module_frag);

            if (newShaderModuleData.usingGeom)
            {
                add_stage(
                    recreateData->stages,
                    VK_SHADER_STAGE_GEOMETRY_BIT,
                    newShaderModuleData.module_geom);
            }
            if (newShaderModuleData.usingTessCont)
            {
                add_stage(
                    recreateData->stages,
                    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                    newShaderModuleData.module_tess_cont);
            }
            if (newShaderModuleData.usingTessEval)
            {
                add_stage(
                    recreateData->stages,
                    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                    newShaderModuleData.module_tess_eval);
            }

            newPipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            newPipelineInfo.stageCount          = scast<u32>(recreateData->stages.size()); // < new data from this re-import
            newPipelineInfo.pStages             = recreateData->stages.data();             // < new data from this re-import
            newPipelineInfo.pVertexInputState   = &recreateData->vertexInput;
            newPipelineInfo.pInputAssemblyState = &recreateData->inputAssembly;
            newPipelineInfo.pViewportState      = &recreateData->viewportState;
            newPipelineInfo.pRasterizationState = &recreateData->rasterization;
            newPipelineInfo.pMultisampleState   = &recreateData->multisampling;
            newPipelineInfo.pDepthStencilState  = &recreateData->depthStencil;
            newPipelineInfo.pColorBlendState    = &recreateData->colorBlend;
            newPipelineInfo.pDynamicState       = &recreateData->dynamicState;
            newPipelineInfo.layout              = newPipelineLayout;
            newPipelineInfo.renderPass          = gctx->GetRenderPass();
            newPipelineInfo.subpass             = 0;
        }

        if (recreate)
        {
            descriptorSetLayout = newDescriptorSetLayout;
            pipelineLayout = newPipelineLayout;
        }

        VkPipeline newPipeline{};
        VkResult vkResult = vkCreateGraphicsPipelines(
            logicalDevice,
            VK_NULL_HANDLE,
            1,
            &newPipelineInfo,
            nullptr,
            &newPipeline);

        if (vkResult != VK_SUCCESS)
        {
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
        // FINISH
        //

        //delete old pipeline if it exists
        if (pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(
                logicalDevice,
                pipeline,
                nullptr);
        }

        pipeline = newPipeline;
        shaderModuleData = std::move(newShaderModuleData);

        Log::Print(
            "Assigned new shader data for shader '" + to_string(ID) 
            + "' under graphics context '" + to_string(contextID) + "'!",
            "KG_SHADER",
            LogType::LOG_SUCCESS);
    }

    VkDescriptorSetLayout Shader::GetDescriptorSetLayout() { return descriptorSetLayout; }
    VkPipelineLayout Shader::GetPipelineLayout() { return pipelineLayout; }
    VkPipeline Shader::GetPipeline() { return pipeline; }

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

            vkCmdBindDescriptorSets(
                cmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0,
                1,
                &camera->vkCameraDescriptorSet,
                0,
                nullptr);
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
                if (mesh->vertexBufferSize == 0) continue;
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
            else if (mesh->vertexBufferSize == 0)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics shader error",
                    "Failed to render mesh '" + to_string(meshID) 
                    + "' on shader '" + to_string(ID) 
                    + "' because its vertex buffer is valid but it "
                    "doesn't have vertex buffer data!");
            }

            //TODO: use spriv-reflection

            vkCmdPushConstants(
                cmdBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(REPLACE_ME_TEST_SHADER_DATA),
                &mesh->testShaderData);

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
                if (mesh->indexBufferSize == 0)
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

    void Shader::Destroy()
    {
        for (u32 cID : cameraIDs)
        {
            Camera* c = Camera::GetRegistry().GetContent(cID);
            if (c) c->shaderID = 0;
        }
        cameraIDs.clear();

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

        vkDestroyDescriptorSetLayout(
            logicalDevice,
            descriptorSetLayout,
            nullptr);

        vkDestroyShaderModule(
            logicalDevice,
            shaderModuleData.module_vert,
            nullptr);
        vkDestroyShaderModule(
            logicalDevice,
            shaderModuleData.module_frag,
            nullptr);

        if (shaderModuleData.usingGeom)
        {
            vkDestroyShaderModule(
                logicalDevice,
                shaderModuleData.module_geom,
                nullptr);
        }
        if (shaderModuleData.usingTessCont)
        {
            vkDestroyShaderModule(
                logicalDevice,
                shaderModuleData.module_tess_cont,
                nullptr);
        }
        if (shaderModuleData.usingTessEval)
        {
            vkDestroyShaderModule(
                logicalDevice,
                shaderModuleData.module_tess_eval,
                nullptr);
        }
    }
}