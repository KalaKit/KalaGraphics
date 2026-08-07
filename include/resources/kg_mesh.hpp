//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>
#include <filesystem>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"
#include "resources/kg_shader.hpp"

struct VkBuffer_T;
using VkBuffer = VkBuffer_T*;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

namespace KalaGraphics::Resources
{
    using KalaHeaders::KalaMath::Transform3D;
    using KalaHeaders::KalaMath::mat4;
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::vec2;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::string;
    using std::filesystem::path;

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

    //Output after generating a meshes data
    struct LIB_API Mesh_Generated_Data
    {
        vector<Vertex> vertices{};
        vector<u32> indices{};
    };

    class LIB_API Mesh
    {
    friend class Shader;
    friend class Camera;
    public:
        static KalaGraphicsRegistry<Mesh>& GetRegistry();

        //Create a blank mesh,
        //all meshes require a shader even if that shader is also blank
        static Mesh* Initialize(u32 shaderID);

        //Import FBX, OBJ or GLTF model mesh data
        static Mesh_Generated_Data GenerateMeshData(const path& meshPath);
        //Generate a cube or cylinder
        static Mesh_Generated_Data GenerateMeshData(Mesh_Cube cubeData);
        //Generate a pyramid or cone
        static Mesh_Generated_Data GenerateMeshData(Mesh_Pyramid pyramidData);
        //Generate a sphere
        static Mesh_Generated_Data GenerateMeshData(
            SphereType sphereType,
            Mesh_Sphere sphereData);

        u32 GetID() const;
        u32 GetCameraID() const;

        u32 GetShaderID() const;
        void SetShaderID(u32 newID);

        //Should be called after manually updating mesh shader data,
        //vertices, indices or attached camera data
        void UpdateMeshData();

        bool Is2D() const;
        void Set2DState(bool newState);

        Transform3D& GetTransform();

        const mat4& GetModelMatrix() const;

        vector<Vertex>& GetVertices();
        vector<u32>& GetIndices();

        VkBuffer GetBuffer(bool vertexBuffer);
        VmaAllocation GetAllocation(bool vertexAllocation);

        void Destroy();

        ~Mesh();
    private:
        void UpdateVertices();
        void UpdateIndices();

        bool isDestroyingCamera{};

        u32 ID{};
        u32 shaderID{};
        u32 cameraID{};

        bool is2D{};

        Transform3D transform{};

        vector<Vertex> vertices{};
        VkBuffer vkVertexBuffer{};
        VmaAllocation vmaVertexAllocation{};
        size_t vertexBufferSize{}; //required because vertices size may change
        void* vertexMappedPtr{};

        vector<u32> indices{};
        VkBuffer vkIndexBuffer{};
        VmaAllocation vmaIndexAllocation{};
        size_t indexBufferSize{}; //required because indices size may change
        void* indexMappedPtr{};

        REPLACE_ME_TEST_SHADER_DATA testShaderData{};
    };
}