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

using u8 = uint8_t;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Shader> registry{};

    KalaGraphicsRegistry<Shader>& Shader::GetRegistry() { return registry; }

    Shader* Shader::Initialize(
        u32 graphicsContextID,
        string&& shaderName,
        ShaderData&& shaderData)
    {
        auto empty_path = [&shaderName](string_view shaderType) -> void
            {
                Log::Print(
                    "Cannot initialize shader '" + shaderName + "' because it did not contain a " + string(shaderType) + " shader file!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto bad_ext = [&shaderName](string_view shaderType) -> void
            {
                Log::Print(
                    "Cannot initialize shader '" + shaderName + "' because its " + string(shaderType) + " shader had a missing or incorrect extension!", 
                    "KG_SHADER",
                    LogType::LOG_ERROR,
                    2);
            };
        auto invalid_path = [&shaderName](
            string_view shaderType,
            string_view shaderPath) -> void
            {
                Log::Print(
                    "Cannot initialize shader '" + shaderName + "' because its " + string(shaderType) + " shader path '" + string(shaderPath) + "' does not exist!", 
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

        GraphicsContext* graphicsContextPtr = GraphicsContext::GetRegistry().GetContent(graphicsContextID);
        if (!graphicsContextPtr)
        {
            Log::Print(
                "Cannot initialize shader '" + shaderName + "' because the graphics context '" + to_string(graphicsContextID) + "' was not found!", 
                "KG_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
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

        auto create_shader_module = [&shaderName](string_view shaderType, const path& shaderPath) -> ShaderModule
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
                    GraphicsContext::GetLogicalDevice(),
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
                            "Vulkan shader error",
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

        auto destroy_shaders = [](vector<VkShaderModule> modules) -> void
            {
                for (const auto& m : modules)
                {
                    vkDestroyShaderModule(
                        GraphicsContext::GetLogicalDevice(),
                        m,
                        nullptr);
                }
            };

        //
        // MODULES
        //

        unique_ptr<Shader> newShader = make_unique<Shader>();
        Shader* shaderPtr = newShader.get();

        shaderPtr->graphicsContextID = graphicsContextPtr->GetID();

        ShaderModule module_vert = create_shader_module("vertex", shaderData.shader_vert);
        if (!module_vert.success) return nullptr;
        else shaderPtr->shaderModuleData.module_vert = module_vert.module;

        ShaderModule module_frag = create_shader_module("fragment", shaderData.shader_frag);
        if (!module_frag.success)
        {
            destroy_shaders({ module_vert.module });

            return nullptr;
        }
        else shaderPtr->shaderModuleData.module_frag = module_frag.module;

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
                shaderPtr->shaderModuleData.usingGeom = true;
                shaderPtr->shaderModuleData.module_geom = module_geom.module;
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
                shaderPtr->shaderModuleData.usingTessCont = true;
                shaderPtr->shaderModuleData.module_tess_cont = module_tess_cont.module;
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
                shaderPtr->shaderModuleData.usingTessEval = true;
                shaderPtr->shaderModuleData.module_tess_eval = module_tess_eval.module;
            }
        }

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
            GraphicsContext::GetLogicalDevice(),
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
                "Failed to create descriptor set layout for shader '" + shaderPtr->name + "'! Reason: " 
                + GraphicsContext::GetVkResultMessage(vkResult);

            if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
            {
                KalaGraphicsCore::ForceClose(
                    "Vulkan shader error",
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

        shaderPtr->descriptorSetLayout = descriptorSetLayout;

        //
        // PIPELINE LAYOUT
        //

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        VkPipelineLayout pipelineLayout{};
        vkResult = vkCreatePipelineLayout(
            GraphicsContext::GetLogicalDevice(),
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
                GraphicsContext::GetLogicalDevice(),
                descriptorSetLayout,
                nullptr);

            string message = 
                "Failed to create pipeline layout for shader '" + shaderPtr->name + "'! Reason: " 
                + GraphicsContext::GetVkResultMessage(vkResult);

            if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
            {
                KalaGraphicsCore::ForceClose(
                    "Vulkan shader error",
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

        shaderPtr->pipelineLayout = pipelineLayout;

        //
        // PIPELINE
        //

        vector<VkPipelineShaderStageCreateInfo> stages{};

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
            stages,
            VK_SHADER_STAGE_VERTEX_BIT,
            shaderPtr->shaderModuleData.module_vert);
        add_stage(
            stages,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            shaderPtr->shaderModuleData.module_frag);

        if (shaderPtr->shaderModuleData.usingGeom)
        {
            add_stage(
                stages,
                VK_SHADER_STAGE_GEOMETRY_BIT,
                shaderPtr->shaderModuleData.module_geom);
        }
        if (shaderPtr->shaderModuleData.usingTessCont)
        {
            add_stage(
                stages,
                VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                shaderPtr->shaderModuleData.module_tess_cont);
        }
        if (shaderPtr->shaderModuleData.usingTessEval)
        {
            add_stage(
                stages,
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                shaderPtr->shaderModuleData.module_tess_eval);
        }

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof(f32) * 8;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attributeDescriptions[] =
        {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
            { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(f32) * 3 },
            { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(f32) * 5 }
        };

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount = 3;
        vertexInput.pVertexAttributeDescriptions    = attributeDescriptions;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        vector<VkDynamicState> dynamicStates = 
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = scast<u32>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments    = &colorBlendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = scast<u32>(stages.size());
        pipelineInfo.pStages             = stages.data();
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlend;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = pipelineLayout;
        pipelineInfo.renderPass          = graphicsContextPtr->GetRenderPass();
        pipelineInfo.subpass             = 0;

        VkPipeline pipeline{};
        vkResult = vkCreateGraphicsPipelines(
            GraphicsContext::GetLogicalDevice(),
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
                GraphicsContext::GetLogicalDevice(),
                descriptorSetLayout,
                nullptr);

            vkDestroyPipelineLayout(
                GraphicsContext::GetLogicalDevice(),
                pipelineLayout,
                nullptr);

            string message = 
                "Failed to create graphics pipeline for shader '" + shaderPtr->name + "'! Reason: " 
                + GraphicsContext::GetVkResultMessage(vkResult);

            if (GraphicsContext::GetVkResultSeverity(vkResult) == Severity::S_FATAL)
            {
                KalaGraphicsCore::ForceClose(
                    "Vulkan shader error",
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

        shaderPtr->pipeline = pipeline;

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);
        shaderPtr->ID = newID;

        registry.AddContent(newID, std::move(newShader));

        Log::Print(
			"Created new shader " + shaderPtr->name + " with ID '" + to_string(newID) + "'!",
			"KG_SHADER",
			LogType::LOG_SUCCESS);

        return shaderPtr;
    }

    u32 Shader::GetID() const { return ID; }
    u32 Shader::GetGraphicsContextID() const { return graphicsContextID; }
    const vector<u32>& Shader::GetMeshIDs() const { return meshIDs; }

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

    VkPipelineLayout Shader::GetPipelineLayout() { return pipelineLayout; }
    VkPipeline Shader::GetPipeline() { return pipeline; }

    void Shader::Update(VkCommandBuffer cmdBuffer)
    {
        vkCmdBindPipeline(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline);

        vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &descriptorSet,
            0,
            nullptr);

        for (u32 meshID : meshIDs)
        {
            Mesh* mesh = Mesh::GetRegistry().GetContent(meshID);
            if (!mesh)
            {
                KalaGraphicsCore::ForceClose(
                    "Shader update error",
                    "Failed to update shader '" + to_string(ID) + "' because its mesh '" + to_string(meshID) + "' was nullptr!");
            }

            mesh->SyncToGPU();

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

    void Shader::Destroy() { registry.RemoveContent(ID); }

    Shader::~Shader()
    {
        VkDevice device = GraphicsContext::GetLogicalDevice();

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