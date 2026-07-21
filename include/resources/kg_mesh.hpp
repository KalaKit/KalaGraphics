//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Resources
{
    using KalaHeaders::KalaMath::Transform3D;
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::vec2;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;

    using u8 = uint8_t;
    using f32 = float;

    /*
    struct LIB_API Mesh_Cube
    {
        //ranges from 3 to 255,
        //used for top and bottom edges
        u8 edgeCount{};
    };

    struct LIB_API Mesh_Pyramid
    {
        //clamped from 0.01f to 10000.0f
        f32 bottomRadius = 1.0f;

        //clamped from 0.01f to 10000.0f
        f32 height = 1.0f;

        //ranges from 3 to 255,
        //used for bottom edges
        u8 edgeCount{};
    };

    enum class SphereType : u8
    {
        S_INVALID = 0u,

        S_UV = 1u,
        S_ICO = 2u,
        S_QUAD = 3u
    };
    struct LIB_API Mesh_Sphere
    {
        //clamped from 0.01f to 10000.0f
        f32 radius = 0.5f;
        //clamped from 1 to 255
        u8 detailLevel{};

        SphereType type{};
    };
    */

    struct LIB_API Transform
    {
        //X, Y, Z (Z is unused for 2D)
        vec3 pos{};
        //X, Y, Z (Y and Z are unused for 2D)
        vec3 rot{};
        //X, Y, Z (Z is unused for 2D)
        vec3 size{};
    };

    struct LIB_API Vertex
    {
        //X, Y, Z, (Z is unused for 2D)
        vec3 pos{};
        //U, V texture coordinates
        vec2 uv{};
    };

    enum class MeshType : u8
    {
        M_INVALID = 0u,

        M_2D = 1u,
        M_3D = 2u
    };

    class LIB_API Mesh
    {
    public:
        static KalaGraphicsRegistry<Mesh>& GetRegistry();

        static Mesh* Initialize(
            MeshType meshType,
            const Transform& transform,
            const vector<Vertex>& vertices);

        u32 GetID() const;

        void Destroy();

        ~Mesh();
    private:
        u32 ID{};

        MeshType meshType{};
        vector<Vertex> vertices{};

        Transform3D transform{};
    };
}