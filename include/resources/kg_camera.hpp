//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

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
    public:
        static KalaGraphicsRegistry<Camera>& GetRegistry();

        static Camera* Initialize(
            u32 contextID, 
            CameraType type,
            vec3&& pos = {},
            vec3&& rot = { 0.0f, -90.0f, 0.0f },
            f32 fov = 90.0f,
            vec2 drawDistance = { 0.001f, 1000.0f });

        u32 GetID() const;

        u32 GetGraphicsContextID() const;
        void SetGraphicsContextID(u32 newValue);

        Transform3D& GetTransform();

        f32 GetFOV() const;
        void SetFOV(f32 newFOV);

        vec2 GetDrawDistance();
        void SetDrawDistance(vec2 newDraw);

        const mat4& GetView() const;
        const mat4& GetProjection() const;
        const mat4& GetOrtho() const;
        const mat4& GetUModel() const;

        //Allow camera to be moved by mouse
        void Update(vec2 mouse);

        void Destroy();

        ~Camera();
    private: 
        //used only to prevent camera from removing its ID from
        //graphics context camera IDs list if the graphics context
        //destroy function called the destroy function of this camera 
        bool isDestroyingGraphicsContext{};

        u32 ID{};
        u32 contextID{};

        CameraType type{};

        Transform3D transform{};

        f32 fov{};
        f32 aspect{};
        vec2 drawDistance{};

        //internal viewport size value that comes from graphics context
        //whenever the swapchain is recreated during resize
        vec2 viewport{};

        mat4 view{};
        mat4 projection{};
        mat4 ortho{};
        mat4 umodel{};
    };
}