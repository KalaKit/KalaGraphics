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
    class Viewport;
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
    friend class Mesh;
    friend class Texture;
    friend class Camera;
    friend class KalaGraphics::Core::GraphicsContext;
    friend class KalaGraphics::Core::Viewport;
    friend default_delete<Shader>;
    public:
        static KalaGraphicsRegistry<Shader>& GetRegistry();

        //Create a blank shader.
        //This shader has no shader data and
        //must be given shaders via SetShaderData
        static Shader* Initialize(u32 viewportID);

        u32 GetID() const;

        u32 GetViewportID() const;
        void SetViewportID(u32 newValue);

        bool Is2D() const;

        const vector<u32>& GetMeshIDs() const;
        const vector<u32>& GetTextureIDs() const;
        const vector<u32>& GetCameraIDs() const;

        //First time init or hot-reload shaders
        void UpdateShaderData(
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
        //viewport shader IDs list if the viewport
        //destroy function called the destroy function of this shader 
        bool isDestroyingViewport{};

        u32 ID{};
        u32 viewportID{};

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