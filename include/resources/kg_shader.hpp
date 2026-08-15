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
    using KalaHeaders::KalaMath::mat4;
    using KalaHeaders::KalaMath::vec4;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::filesystem::path;
    using std::string;
    using std::string_view;
    using std::vector;
    using std::unique_ptr;
    using std::default_delete;

    using u8 = uint8_t;
    using u32 = uint32_t;

    struct ShaderModuleData
    {
        VkShaderModule vkModule_vert{};
        uintptr_t spvModule_vert{};

        VkShaderModule vkModule_frag{};
        uintptr_t spvModule_frag{};

        bool usingGeom{};
        VkShaderModule vkModule_geom{};
        uintptr_t spvModule_geom{};
    };

    class LIB_API Shader
    {
    friend class KalaGraphics::Core::GraphicsContext;
    friend class Mesh;
    friend class Texture;
    friend class Camera;
    friend default_delete<Shader>;
    public:
        static KalaGraphicsRegistry<Shader>& GetRegistry();

        //Create a blank shader.
        //This shader has no shader data and
        //must be given shaders via SetShaderData
        static Shader* Initialize(u32 graphicsContextID);

        u32 GetID() const;

        u32 GetGraphicsContextID() const;
        void SetGraphicsContextID(u32 newValue);

        bool Is2D() const;

        const vector<u32>& GetMeshIDs() const;
        const vector<u32>& GetTextureIDs() const;
        const vector<u32>& GetCameraIDs() const;

        //First time init or hot-reload shaders
        void SetShaderData(
            bool is2D,
            path&& vertPath,
            path&& fragPath,
            path&& geomPath = {});

        const vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts();

        void Destroy();
    private:
        ~Shader();

        static void DestroyVkShaderModules(vector<VkShaderModule> modules);

        void Update(VkCommandBuffer buffer);

        //used only to prevent shader from removing its ID from
        //graphics context shader IDs list if the graphics context
        //destroy function called the destroy function of this shader 
        bool isDestroyingGraphicsContext{};

        u32 ID{};
        u32 contextID{};

        vector<u32> meshIDs{};
        vector<u32> textureIDs{};
        vector<u32> cameraIDs{};

        bool is2D{};

        u8 missingPipelineWarningCount{};
        u8 missingMeshWarningCount{};

        ShaderModuleData shaderModuleData{};
        vector<VkDescriptorSetLayout> descriptorSetLayouts{};
        VkPipelineLayout pipelineLayout{};
        VkPipeline pipeline{};
    };
}