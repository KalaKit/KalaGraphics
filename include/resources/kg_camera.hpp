//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

struct VkBuffer_T;
using VkBuffer = VkBuffer_T*;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

struct VkDescriptorSet_T;
using VkDescriptorSet = VkDescriptorSet_T*;

namespace KalaGraphics::Core
{
    class GraphicsContext;
}

namespace KalaGraphics::Resources
{
    constexpr f32 FOV_MIN = 30.0f;
    constexpr f32 FOV_MAX = 180.0f;

    constexpr f32 DRAW_DISTANCE_MIN = 0.001f;
    constexpr f32 DRAW_DISTANCE_MAX = 10000.0f;

    using KalaHeaders::KalaMath::Transform3D;
    using KalaHeaders::KalaMath::vec2;
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::mat4;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    enum class CameraType : u8
    {
        C_INVALID = 0u,

        C_ORTHOGRAPHIC = 1u,
        C_PERSPECTIVE = 2u
    };

    class LIB_API Camera
    {
    friend class KalaGraphics::Core::GraphicsContext;
    friend class Mesh;
    friend class Shader;
    public:
        static KalaGraphicsRegistry<Camera>& GetRegistry();

        static Camera* Initialize(
            u32 contextID,
            u32 shaderID);

        u32 GetID() const;

        u32 GetGraphicsContextID() const;
        void SetGraphicsContextID(u32 newValue);

        u32 GetShaderId() const;
        void SetShaderID(u32 newValue);

        u32 GetMeshID() const;
        void SetMeshID(u32 newValue);

        CameraType GetCameraType() const;
        void SetCameraType(CameraType type);

        //Pass mouse and keyboard input to this camera,
        //all values are internally clamped to 0, 1.
        //Calls UpdateCameraData internally, no need to call it separately.
        void Move(
            vec2 mouse,
            vec2 keyboard);

        //Call after updating camera transform manually to ensure camera UBO buffer is up to date
        void UpdateCameraData();

        Transform3D& GetTransform();

        f32 GetFOV() const;
        void SetFOV(f32 newFOV);

        vec2 GetDrawDistance() const;
        void SetDrawDistance(vec2 newDraw);

        const mat4& GetCameraMatrix() const;

        VkBuffer GetBuffer();
        VmaAllocation GetAllocation();
        VkDescriptorSet GetDescriptorSet();

        void Destroy();

        ~Camera();
    private: 
        //used only to prevent camera from removing its ID from
        //graphics context camera IDs list if the graphics context
        //destroy function called the destroy function of this camera 
        bool isDestroyingGraphicsContext{};

        u32 ID{};
        u32 contextID{};
        u32 shaderID{};
        u32 meshID{};

        CameraType type = CameraType::C_PERSPECTIVE;

        Transform3D transform{};

        f32 fov = 90.0f;
        f32 aspect{};
        vec2 drawDistance = { 0.001f, 1000.0f };

        //internal viewport size value that comes from graphics context
        //whenever the swapchain is recreated during resize
        vec2 viewport{};

        mat4 projectionMatrix{};
        mat4 orthographicMatrix{};

        bool reassign{};
        VkBuffer vkCameraUBOBuffer{};
        VmaAllocation vmaCameraUBOAllocation{};
        void* cameraUBOMappedPtr{};

        VkDescriptorSet vkCameraDescriptorSet{};
    };
}