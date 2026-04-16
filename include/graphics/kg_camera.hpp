//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Graphics
{
    using KalaHeaders::KalaMath::Transform3D;
    using KalaHeaders::KalaMath::vec2;
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::mat4;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    enum class CameraType : u8
    {
        C_INVALID = 0u,

        C_PERSPECTIVE = 1u,
        C_ORTHOGRAPHIC = 2u
    };

    class LIB_API Camera
    {
    public:
        static KalaGraphicsRegistry<Camera>& GetRegistry();

        static Camera* Initialize(
            CameraType type,
            f32 fov,
            f32 aspect,
            vec2 drawDistance = { 0.001f, 1000.0f },
            const vec3& pos = {},
            const vec3& rot = {});

        u32 GetID() const;

        const vec3& GetPos() const;
        const vec3& GetRot() const;

        void SetPos(const vec3& pos);
        void AddPos(const vec3& pos);

        void SetRot(const vec3& rot);
        void AddRot(const vec3& rot);

        f32 GetFOV() const;
        f32 GetAspect() const;
        vec2 GetDrawDistance();

        void SetFOV(f32 newFOV);
        void SetAspect(f32 newAspect);
        void SetDrawDistance(vec2 newDraw);

        const mat4& GetView() const;
        const mat4& GetProjection() const;

        void Update(vec2 mouse);
    private: 
        u32 ID{};

        CameraType type{};

        Transform3D transform{};

        f32 fov{};
        f32 aspect{};
        vec2 drawDistance{};

        mat4 view{};
        mat4 projection{};
    };
}