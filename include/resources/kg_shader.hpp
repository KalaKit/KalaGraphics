//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <memory>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;

struct VkShaderModule_T;
using VkShaderModule = VkShaderModule_T*;

struct VkDescriptorSetLayout_T;
using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;

struct VkDescriptorSet_T;
using VkDescriptorSet = VkDescriptorSet_T*;

struct VkPipelineLayout_T;
using VkPipelineLayout = VkPipelineLayout_T*;

struct VkPipeline_T;
using VkPipeline = VkPipeline_T*;

namespace KalaGraphics::Core
{
    class GraphicsContext;
}

namespace KalaGraphics::Resources
{
    using KalaHeaders::KalaMath::vec4;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::filesystem::path;
    using std::string;
    using std::string_view;
    using std::vector;
    using std::unique_ptr;

    using u8 = uint8_t;
    using u32 = uint32_t;

    enum class DescriptorBindingType : u8
    {
        D_INVALID = 0u,

        //read-only structured data (material params, camera matrices, lighting constants).
        //small, frequently updated, cached aggressively by GPU, most common binding type
        D_UNIFORM_BUFFER = 1u,
        //texture + sampler fused into one binding. shader samples it directly.
        //used for albedo, normal, roughness maps, the most common texture binding type
        D_COMBINED_IMAGE_SAMPLER = 2u,
        //read/write buffer for larger or compute-style data. less cache-friendly than UBOs
        //but supports arbitrary sizes and write access. used for particle data, bone matrices, SSBO-based materials
        D_STORAGE_BUFFER = 3u,
        //image view without an embedded sampler. sampler is bound separately via D_SAMPLER.
        //useful when multiple textures share the same sampler state, reducing descriptor count
        D_SAMPLED_IMAGE = 4u,
        //standalone sampler object, paired with D_SAMPLER_IMAGE when you want to
        //decouple texture content from filtering/wrapping state.
        D_SAMPLER = 5u
    };

    struct DescriptorBinding
    {
        u8 slot{};
        DescriptorBindingType type{};
    };

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
        //transforms and vertices, required
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

    //forward declaration, defined in kg_shader.cpp
    struct ShaderPipelineRecreateData;

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

    //TODO: replace with spirv-reflection logic later
    struct REPLACE_ME_TEST_SHADER_DATA
    {
        vec4 color{};    //offset 0
        u32 debugMode{}; //offset 16, value should be 0 or 1
    };

    class LIB_API Shader
    {
    friend class KalaGraphics::Core::GraphicsContext;
    friend class Mesh;
    public:
        static KalaGraphicsRegistry<Shader>& GetRegistry();

        static Shader* Initialize(
            u32 graphicsContextID,
            ShaderData&& shaderData,
            vector<DescriptorBinding>&& bindings = {});

        u32 GetID() const;

        u32 GetGraphicsContextID() const;
        //Assign a new graphics context ID,
        //can choose to carry content over to new graphics context ID,
        //otherwise if left false then they will be detached from this shader
        void SetGraphicsContextID(
            u32 newValue,
            bool carryContentOver = false);

        VkShaderModule GetShaderModule(ShaderType type);

        VkDescriptorSetLayout GetDescriptorSetLayout();
        VkDescriptorSet GetDescriptorSet();

        VkPipelineLayout GetPipelineLayout();
        VkPipeline GetPipeline();

        void Destroy();

        ~Shader();
    private:
        void Update(VkCommandBuffer buffer);

        //used only to prevent shader from removing its ID from
        //graphics context camera IDs list if the graphics context
        //destroy function called the destroy function of this shader 
        bool isDestroyingGraphicsContext{};

        u32 ID{};
        u32 contextID{};

        vector<u32> meshIDs{};

        u8 missingMeshWarningCount{};

        ShaderData shaderData{};
        ShaderModuleData shaderModuleData{};

        unique_ptr<ShaderPipelineRecreateData> recreateData;

        VkDescriptorSetLayout descriptorSetLayout{};
        VkDescriptorSet descriptorSet{};

        VkPipelineLayout pipelineLayout{};
        VkPipeline pipeline{};
    };
}