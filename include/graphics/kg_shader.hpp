//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <string>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

struct VkPipelineLayout_T;
using VkPipelineLayout = VkPipelineLayout_T*;

struct VkPipeline_T;
using VkPipeline = VkPipeline_T*;

struct VkDescriptorSetLayout_T;
using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;

struct VkShaderModule_T;
using VkShaderModule = VkShaderModule_T*;

namespace KalaGraphics::Graphics
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::filesystem::path;
    using std::string;
    using std::string_view;

    using u8 = uint8_t;
    using u32 = uint32_t;

    enum class ShaderType : u8
    {
        SHADER_INVALID = 0u,

        //transforms vertices
        SHADER_VERT = 1u,
        //outputs pixel colors
        SHADER_FRAG = 2u,
        //generates/discards primitives
        SHADER_GEOM = 3u,
        //controls tesselation patches
        SHADER_TESS_CONT = 4u,
        //generates/discards primitives
        SHADER_TESS_EVAL = 5u
    };

    struct ShaderData
    {
        //transforms vertices, required
        path shader_vert{};
        //outputs pixel colors, required
        path shader_frag{};
        //generates/discards primitives
        path shader_geom{};
        //controls tesselation patches
        path shader_tess_cont{};
        //generates/discards primitives
        path shader_tess_eval{};
    };

    struct ShaderModuleData
    {
        VkShaderModule module_vert{};
        VkShaderModule module_frag{};

        bool usingGeom{};
        VkShaderModule module_geom{};

        bool usingTessCont{};
        VkShaderModule module_tess_cont{};

        bool usingTessEval{};
        VkShaderModule module_tess_eval{};
    };

    class LIB_API Shader
    {
    public:
        static KalaGraphicsRegistry<Shader>& GetRegistry();

        static Shader* Initialize(
            u32 windowContextID,
            string_view shaderName,
            const ShaderData& shaderData);

        u32 GetID() const;
        u32 GetSlot() const;

        string_view GetName() const;

        VkShaderModule GetShaderModule(ShaderType type);

        VkDescriptorSetLayout GetDescriptorSetLayout();

        VkPipelineLayout GetPipelineLayout();
        VkPipeline GetPipeline();

        //Bind the shader with the given data before this function is called
        bool Bind();

        //Destroy this shader
        void Shutdown();
    private:
        string name;

        u32 ID{};

        ShaderData shaderData{};
        ShaderModuleData shaderModuleData{};

        VkDescriptorSetLayout descriptorSetLayout{};

        VkPipelineLayout pipelineLayout{};
        VkPipeline pipeline{};
    };
}