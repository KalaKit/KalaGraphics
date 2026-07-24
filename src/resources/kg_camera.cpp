//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "resources/kg_camera.hpp"

using KalaHeaders::KalaMath::toeuler3;

using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Camera> registry{};

    KalaGraphicsRegistry<Camera>& Camera::GetRegistry() { return registry; }

    Camera* Camera::Initialize(
            CameraType type,
            f32 fov,
            f32 aspect,
            vec2 drawDistance,
            vec3&& pos,
            vec3&& rot)
    {

    }

    u32 Camera::GetID() const { return ID; }

    const vec3& Camera::GetPos() const { return transform.pos_world; }
    const vec3& Camera::GetRot() const { return toeuler3(transform.rot_world); }
}