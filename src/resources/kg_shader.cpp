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

    Shader* Shader::Initialize(
        u32 contextID,
        string&& shaderName,
        ShaderData&& shaderData,
        vector<DescriptorBinding>&& descriptorBindings)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to initialize shader because logical device was invalid!");
        }

        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (!gctx)
        {
            Log::Print(
                "Cannot initialize shader '" + shaderName + "' because the graphics context '" + to_string(contextID) + "' was not found!", 
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (shaderName.empty()
            || shaderName.size() > 50)
        {
            Log::Print(
                "Failed to create shader because its name is empty or too long!",
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        auto empty_path = [&shaderName](string_view shaderType) -> void
            {
                Log::Print(
                    "Cannot initialize shader '" + shaderName + "' because it did not contain a " 
                    + string(shaderType) + " shader file!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto bad_ext = [&shaderName](string_view shaderType) -> void
            {
                Log::Print(
                    "Cannot initialize shader '" + shaderName + "' because its " + string(shaderType) 
                    + " shader had a missing or incorrect extension!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto invalid_path = [&shaderName](
            string_view shaderType,
            string_view shaderPath) -> void
            {
                Log::Print(
                    "Cannot initialize shader '" + shaderName + "' because its " + string(shaderType) 
                    + " shader path '" + string(shaderPath) + "' was not found!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };

        if (shaderData.shader_vert.empty())
        {
            empty_path("vertex");
            return nullptr;
        }
        if (!shaderData.shader_vert.has_extension()
            || shaderData.shader_vert.extension() != ".spv")
        {
            bad_ext("vertex");
            return nullptr;
        }
        if (!exists(shaderData.shader_vert))
        {
            invalid_path("vertex", shaderData.shader_vert.string());
            return nullptr;
        }

        if (shaderData.shader_frag.empty())
        {
            empty_path("fragment");
            return nullptr;
        }
        if (!shaderData.shader_frag.has_extension()
            || shaderData.shader_frag.extension() != ".spv")
        {
            bad_ext("fragment");
            return nullptr;
        }
        if (!exists(shaderData.shader_frag))
        {
            invalid_path("fragment", shaderData.shader_frag.string());
            return nullptr;
        }

        if (!shaderData.shader_geom.empty())
        {
            if (!shaderData.shader_geom.has_extension()
                || shaderData.shader_geom.extension() != ".spv")
            {
                bad_ext("geometry");
                return nullptr;
            }
            if (!exists(shaderData.shader_geom))
            {
                invalid_path("geometry", shaderData.shader_geom.string());
                return nullptr;
            }
        }
        if (!shaderData.shader_tess_cont.empty())
        {
            if (!shaderData.shader_tess_cont.has_extension()
                || shaderData.shader_tess_cont.extension() != ".spv")
            {
                bad_ext("tesselation control");
                return nullptr;
            }
            if (!exists(shaderData.shader_tess_cont))
            {
                invalid_path("tesselation control", shaderData.shader_tess_cont.string());
                return nullptr;
            }
        }
        if (!shaderData.shader_tess_eval.empty())
        {
            if (!shaderData.shader_tess_eval.has_extension()
                || shaderData.shader_tess_eval.extension() != ".spv")
            {
                bad_ext("tesselation evaluation");
                return nullptr;
            }
            if (!exists(shaderData.shader_tess_eval))
            {
                invalid_path("tesselation evaluation", shaderData.shader_tess_eval.string());
                return nullptr;
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
                    "Cannot initialize shader '" + shaderName + "' because " + shaderStage + " was the same as " + it->second + "!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);

                return nullptr;
            }
        }

        struct ShaderModule
        {
            bool success{};
            VkShaderModule module{};
        };

        auto create_shader_module = [&shaderName, &logicalDevice](string_view shaderType, const path& shaderPath) -> ShaderModule
            {
                vector<u8> outData{};
                string errMsg = ReadBinaryDataFromFile(shaderPath, outData);

                if (!errMsg.empty())
                {
                    Log::Print(
                        "Failed to read binary data from shader " + shaderName + " type " + string(shaderType) + "'! Reason: " + errMsg,
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
                        "Failed to initialize shader '" + shaderName 
                        + "' because shader module creation failed! Reason: " 
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

        ShaderModuleData smData{};

        ShaderModule module_vert = create_shader_module("vertex", shaderData.shader_vert);
        if (!module_vert.success) return nullptr;
        else smData.module_vert = module_vert.module;

        ShaderModule module_frag = create_shader_module("fragment", shaderData.shader_frag);
        if (!module_frag.success)
        {
            destroy_shaders({ module_vert.module });

            return nullptr;
        }
        else smData.module_frag = module_frag.module;

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

                return nullptr;
            }
            else
            {
                smData.usingGeom = true;
                smData.module_geom = module_geom.module;
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

                return nullptr;
            }
            else
            {
                smData.usingTessCont = true;
                smData.module_tess_cont = module_tess_cont.module;
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

                return nullptr;
            }
            else
            {
                smData.usingTessEval = true;
                smData.module_tess_eval = module_tess_eval.module;
            }
        }

        //
        // PUSH CONSTANT LAYOUT
        //

        //TODO: use spirv reflection

        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pcRange.offset = 0;
        pcRange.size = sizeof(REPLACE_ME_TEST_SHADER_DATA);

        //
        // DESCRIPTOR SET LAYOUT
        //

        VkDescriptorSetLayoutBinding bindings[] = 
        {
            //32-bit uniform buffer
            { 
                0,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                1,
                VK_SHADER_STAGE_VERTEX_BIT
                | VK_SHADER_STAGE_FRAGMENT_BIT,
                nullptr
            },
            //16-bit color texture (R8G8B8A8)
            { 
                1,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                1,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                nullptr
            }
        };

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = 2;
        descriptorLayoutInfo.pBindings = bindings;

        VkDescriptorSetLayout descriptorSetLayout{};
        VkResult vkResult = vkCreateDescriptorSetLayout(
            logicalDevice,
            &descriptorLayoutInfo,
            nullptr,
            &descriptorSetLayout);

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
                "Failed to create descriptor set layout for shader '" + shaderName + "'! Reason: " 
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

            return nullptr;
        }

        //
        // PIPELINE LAYOUT
        //

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 0; //TODO: update dynamically for descriptor set count
        pipelineLayoutInfo.pSetLayouts            = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1; //TODO: update dynamically for push constant count
        pipelineLayoutInfo.pPushConstantRanges = &pcRange;

        VkPipelineLayout pipelineLayout{};
        vkResult = vkCreatePipelineLayout(
            logicalDevice,
            &pipelineLayoutInfo,
            nullptr,
            &pipelineLayout);

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
                descriptorSetLayout,
                nullptr);

            string message = 
                "Failed to create pipeline layout for shader '" + shaderName + "'! Reason: " 
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

            return nullptr;
        }

        //
        // PIPELINE
        //

        unique_ptr<ShaderPipelineRecreateData> rcData = make_unique<ShaderPipelineRecreateData>();

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

        add_stage(
            rcData->stages,
            VK_SHADER_STAGE_VERTEX_BIT,
            smData.module_vert);
        add_stage(
            rcData->stages,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            smData.module_frag);

        if (smData.usingGeom)
        {
            add_stage(
                rcData->stages,
                VK_SHADER_STAGE_GEOMETRY_BIT,
                smData.module_geom);
        }
        if (smData.usingTessCont)
        {
            add_stage(
                rcData->stages,
                VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                smData.module_tess_cont);
        }
        if (smData.usingTessEval)
        {
            add_stage(
                rcData->stages,
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                smData.module_tess_eval);
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
        rcData->rasterization.cullMode    = VK_CULL_MODE_NONE; //TODO: use correctly as VK_CULL_MODE_BACK_BIT once Y is flipped correctly;
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

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = scast<u32>(rcData->stages.size());
        pipelineInfo.pStages             = rcData->stages.data();
        pipelineInfo.pVertexInputState   = &rcData->vertexInput;
        pipelineInfo.pInputAssemblyState = &rcData->inputAssembly;
        pipelineInfo.pViewportState      = &rcData->viewportState;
        pipelineInfo.pRasterizationState = &rcData->rasterization;
        pipelineInfo.pMultisampleState   = &rcData->multisampling;
        pipelineInfo.pDepthStencilState  = &rcData->depthStencil;
        pipelineInfo.pColorBlendState    = &rcData->colorBlend;
        pipelineInfo.pDynamicState       = &rcData->dynamicState;
        pipelineInfo.layout              = pipelineLayout;
        pipelineInfo.renderPass          = gctx->GetRenderPass();
        pipelineInfo.subpass             = 0;

        VkPipeline pipeline{};
        vkResult = vkCreateGraphicsPipelines(
            logicalDevice,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &pipeline);

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
                descriptorSetLayout,
                nullptr);

            vkDestroyPipelineLayout(
                logicalDevice,
                pipelineLayout,
                nullptr);

            string message = 
                "Failed to create graphics pipeline for shader '" + shaderName + "'! Reason: " 
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

            return nullptr;
        }

        //
        // DESCRIPTOR SET
        //

        VkDescriptorSet dset{};
        if (!descriptorBindings.empty())
        {
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = descriptorBindings.size();
            //TODO: assign correctly
            //poolInfo.pPoolSizes = descriptorBindings.data();

            VkDescriptorPool descriptorPool;
            vkCreateDescriptorPool(
                logicalDevice,
                &poolInfo,
                nullptr,
                &descriptorPool);

            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &descriptorSetLayout;

            VkDescriptorSet descriptorSet;
            vkAllocateDescriptorSets(
                logicalDevice,
                &allocInfo,
                &descriptorSet);

            //TODO: bind data here...

            dset = descriptorSet;
        }

        //
        // FINISH
        //

        unique_ptr<Shader> newShader = make_unique<Shader>();
        Shader* shaderPtr = newShader.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        shaderPtr->ID = newID;
        shaderPtr->contextID = contextID;

        shaderPtr->name = std::move(shaderName);

        //graphics context references this shader
        gctx->shaderIDs.push_back(newID);

        shaderPtr->shaderModuleData = std::move(smData);

        shaderPtr->descriptorSetLayout = descriptorSetLayout;
        shaderPtr->pipelineLayout = pipelineLayout;
        shaderPtr->pipeline = pipeline;
        shaderPtr->descriptorSet = dset;

        shaderPtr->recreateData = std::move(rcData);

        registry.AddContent(newID, std::move(newShader));

        Log::Print(
			"Created new shader '" + shaderPtr->name + "' with ID '" + to_string(newID) + "'!",
			"KG_SHADER",
			LogType::LOG_SUCCESS);

        return shaderPtr;
    }

    u32 Shader::GetID() const { return ID; }

    u32 Shader::GetGraphicsContextID() const { return contextID; }
    void Shader::SetGraphicsContextID(
        u32 newValue,
        bool carryContentOver)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics shader error",
                "Failed to update shader graphics context ID because logical device was invalid!");
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

        GraphicsContext* oldGctx = GraphicsContext::GetRegistry().GetContent(contextID);
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(newValue);
        if (!gctx)
        {
            Log::Print("Failed to set shader '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it was not found!",
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        recreateData->pipelineInfo.renderPass = gctx->GetRenderPass();

        VkPipeline newPipeline{};
        VkResult result = vkCreateGraphicsPipelines(
            logicalDevice,
            VK_NULL_HANDLE,
            1,
            &recreateData->pipelineInfo,
            nullptr,
            &newPipeline);

        if (result != VK_SUCCESS)
        {
            Log::Print("Failed to set shader '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because pipeline recreation failed! Reason: " + GraphicsContext::GetVkResultMessage(result),
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vkDeviceWaitIdle(logicalDevice);
        vkDestroyPipeline(
            logicalDevice,
            pipeline,
            nullptr);

        erase(
            oldGctx->shaderIDs,
            ID);
        gctx->shaderIDs.push_back(ID);

        contextID = gctx->GetID();
        pipeline = newPipeline;

        //remove all meshes because they no longer match this shader gctx ID

        for (u32 mid : meshIDs)
        {
            Mesh* m = Mesh::GetRegistry().GetContent(mid);
            if (m)
            {
                if (!carryContentOver)
                {
                    m->shaderID = 0;

                    Log::Print("Removed shader '" + to_string(ID) 
                        + "' from mesh '" + to_string(mid) 
                        + "' because their graphic context IDs no longer match.",
                        "KG_SHADER",
                        LogType::LOG_WARNING);
                }
                else
                {
                    m->SetContextID(gctx->GetID());

                    Log::Print("Carried mesh '" + to_string(mid) 
                        + "' over with shader '" + to_string(ID) + "' to new graphics context " 
                        + to_string(gctx->GetID()) + " because shader requested graphics context ID swap.",
                        "KG_SHADER",
                        LogType::LOG_WARNING);
                }
            }
        }
        if (!carryContentOver) meshIDs.clear();

        Log::Print(
            "Set shader '" + to_string(ID) 
            + "' graphics context ID to '" + to_string(contextID) + "'!",
            "KG_SHADER",
            LogType::LOG_SUCCESS);
    }

    string_view Shader::GetName() const { return name; }

    VkShaderModule Shader::GetShaderModule(ShaderType type)
    {
        if (type == ShaderType::SHADER_INVALID)
        {
            Log::Print(
                "Cannot use shader type 'SHADER_INVALID' to return shader modules!",
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        switch (type)
        {
            case ShaderType::SHADER_VERT: return shaderModuleData.module_vert;
            case ShaderType::SHADER_FRAG: return shaderModuleData.module_frag;
            case ShaderType::SHADER_GEOM:
            {
                if (!shaderModuleData.usingGeom)
                {
                    Log::Print(
                        "Couldn't get geometry shader module from shader " + name + " because it was not assigned!",
                        "KG_SHADER",
                        LogType::LOG_WARNING);

                    return nullptr;
                }
                else return shaderModuleData.module_geom;
            }
            case ShaderType::SHADER_TESS_CONT:
            {
                if (!shaderModuleData.usingTessCont)
                {
                    Log::Print(
                        "Couldn't get tesselation control shader module from shader " + name + " because it was not assigned!",
                        "KG_SHADER",
                        LogType::LOG_WARNING);

                    return nullptr;
                }
                else return shaderModuleData.module_tess_cont;
            }
            case ShaderType::SHADER_TESS_EVAL:
            {
                if (!shaderModuleData.usingTessEval)
                {
                    Log::Print(
                        "Couldn't get tesselation evaluation shader module from shader " + name + " because it was not assigned!",
                        "KG_SHADER",
                        LogType::LOG_WARNING);

                    return nullptr;
                }
                else return shaderModuleData.module_tess_eval;
            }
            default: return nullptr;
        }

        return nullptr;
    }

    VkDescriptorSetLayout Shader::GetDescriptorSetLayout() { return descriptorSetLayout; }
    VkDescriptorSet Shader::GetDescriptorSet() { return descriptorSet; }
    VkPipelineLayout Shader::GetPipelineLayout() { return pipelineLayout; }
    VkPipeline Shader::GetPipeline() { return pipeline; }

    void Shader::Update(VkCommandBuffer cmdBuffer)
    {
        if (meshIDs.empty())
        {
            if (missingMeshWarningCount < 10)
            {
                Log::Print(
                    "Cannot render onto shader '" + to_string(ID) + "' "
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

        //TODO: use spriv-reflection
        
        REPLACE_ME_TEST_SHADER_DATA pc{};
        pc.color = { 1.0f, 0.8f, 0.6f, 1.0f };
        pc.debugMode = 1;

        vkCmdPushConstants(
            cmdBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(pc),
            &pc);

        if (descriptorSet != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(
                cmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0,
                1,
                &descriptorSet,
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
                    "Failed to update shader '" + to_string(ID) + "' because its mesh '" + to_string(meshID) + "' was nullptr!");
            }

            //mesh->SyncToGPU();

            VkDeviceSize offset{};
            vkCmdBindVertexBuffers(
                cmdBuffer,
                0,
                1,
                &mesh->vkVertexBuffer,
                &offset);

            if (mesh->indexBufferSize > 0)
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
            else
            {
                vkCmdDraw(
                    cmdBuffer,
                    mesh->vertices.size(),
                    1,
                    0,
                    0);
            }
        }
    }

    void Shader::Destroy()
    {
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
        VkDevice device = GraphicsContext::GetLogicalDevice();

        //drain the gpu before destroying this shader
        if (device != VK_NULL_HANDLE) 
        {
            VkResult vkResult = vkDeviceWaitIdle(device);
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
			"Destroying shader '" + name + "' with ID '" + to_string(ID) + "'.",
			"KG_SHADER",
			LogType::LOG_INFO);

        vkDestroyPipeline(
            device,
            pipeline,
            nullptr);
        vkDestroyPipelineLayout(
            device,
            pipelineLayout,
            nullptr);

        vkDestroyDescriptorSetLayout(
            device,
            descriptorSetLayout,
            nullptr);

        vkDestroyShaderModule(
            device,
            shaderModuleData.module_vert,
            nullptr);
        vkDestroyShaderModule(
            device,
            shaderModuleData.module_frag,
            nullptr);

        if (shaderModuleData.usingGeom)
        {
            vkDestroyShaderModule(
                device,
                shaderModuleData.module_geom,
                nullptr);
        }
        if (shaderModuleData.usingTessCont)
        {
            vkDestroyShaderModule(
                device,
                shaderModuleData.module_tess_cont,
                nullptr);
        }
        if (shaderModuleData.usingTessEval)
        {
            vkDestroyShaderModule(
                device,
                shaderModuleData.module_tess_eval,
                nullptr);
        }
    }
}