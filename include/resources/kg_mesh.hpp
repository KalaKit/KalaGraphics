//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

struct VkBuffer_T;
using VkBuffer = VkBuffer_T*;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

namespace KalaGraphics::Resources
{
    using KalaHeaders::KalaMath::Transform3D;
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::vec2;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;

    using u8 = uint8_t;
    using f32 = float;

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
        //X, Y, Z (Z is unused for 2D)
        vec3 normal{};
        //U, V texture coordinates
        vec2 uv{};
    };

    class LIB_API Mesh
    {
    friend class Shader;
    public:
        static KalaGraphicsRegistry<Mesh>& GetRegistry();

        //The default importer, set use2D to true if you
        //intend to use this mesh only for UI, this cannot be changed later
        static Mesh* Initialize(
            bool use2D,
            u32 shaderID,
            Transform&& transform,
            vector<Vertex>&& vertices,
            vector<u32>&& indices);

        //Create a simple cube or cylinder
        static Mesh* Initialize(
            u32 shaderID,
            Transform&& transform,
            Mesh_Cube&& cubeData);

        //Create a simple pyramid or cone
        static Mesh* Initialize(
            u32 shaderID,
            Transform&& transform,
            Mesh_Pyramid&& pyramidData);

        //Create a simple sphere
        static Mesh* Initialize(
            u32 shaderID,
            Transform&& transform,
            Mesh_Sphere&& sphereData);

        u32 GetID() const;
        u32 GetShaderID() const;

        bool Is2D() const;

        VkBuffer& GetVkBuffer(bool vertex);

        void Destroy();

        ~Mesh();
    private:
        bool InitVertices();
        bool InitIndices();

        void SyncToGPU();

        u32 ID{};

        u32 shaderID{};
        vector<u32> textureIDs{};

        bool is2D{};

        vector<Vertex> vertices{};
        VkBuffer vkVertexBuffer{};
        VmaAllocation vmaVertexAllocation{};
        size_t vertexBufferSize{};
        void* vertexMappedPtr{};

        vector<u32> indices{};
        VkBuffer vkIndexBuffer{};
        VmaAllocation vmaIndexAllocation{};
        size_t indexBufferSize{};
        void* indexMappedPtr{};

        Transform3D transform{};
    };
}