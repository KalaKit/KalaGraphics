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

struct VkDescriptorSet_T;
using VkDescriptorSet = VkDescriptorSet_T*;

namespace KalaGraphics::Core
{
    class HitTest;
}

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
    using std::default_delete;

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
        SPHERE_UV = 0,
        SPHERE_ICO = 1,
        SPHERE_QUAD = 2
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
        //X, Y, Z
        vec3 pos{};
        //X, Y, Z
        vec3 normal{};
        //U, V texture coordinates
        vec2 uv{};
        //RGBA color - default is white
        vec4 color = 1;
    };
    struct LIB_API Vertex2D
    {
        //X, Y
        vec2 pos{};
        //U, V texture coordinates
        vec2 uv{};
        //RGBA color - default is white
        vec4 color = 1;
    };

    //Output after generating a meshes data
    struct LIB_API Mesh_Generated_Data
    {
        vector<Vertex> vertices{};
        vector<u32> indices{};
    };

    //TODO: add instancing

    class LIB_API Mesh
    {
    friend class Shader;
    friend class Texture;
    friend class Camera;
    friend class KalaGraphics::Core::HitTest;
    friend struct default_delete<Mesh>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<Mesh>& GetRegistry();

        //Create a blank mesh,
        //all meshes require a shader even if that shader is also blank,
        //all meshes require a texture even if that texture is also a default 1x1 texture
        KNODISCARD
		static Mesh* Initialize(
            u32 shaderID,
            u32 textureID);

        //Import FBX, OBJ or GLTF model mesh data
        KNODISCARD
		static Mesh_Generated_Data GenerateMeshData(const path& meshPath);
        //Generate a cube or cylinder
        KNODISCARD
		static Mesh_Generated_Data GenerateMeshData(Mesh_Cube cubeData);
        //Generate a pyramid or cone
        KNODISCARD
		static Mesh_Generated_Data GenerateMeshData(Mesh_Pyramid pyramidData);
        //Generate a sphere
        KNODISCARD
		static Mesh_Generated_Data GenerateMeshData(
            SphereType sphereType,
            Mesh_Sphere sphereData);

        KNODISCARD
		u32 GetID() const;
        KNODISCARD
        u32 GetHitTestID() const;
        KNODISCARD
		u32 GetCameraID() const;

        KNODISCARD
        u32 GetShaderID() const;
        //Changing to a shader whose 2D state doesn't match the old shader 2D state
        //will recreate this mesh data and detach camera,
        //UpdateMeshData is called internally on success
        void SetShaderID(u32 newID);

        KNODISCARD
		u32 GetTextureID() const;
        void SetTextureID(u32 newID);

        KNODISCARD
		const Transform3D& GetTransform() const;
        void SetTransform(Transform3D&& newTransform);

        KNODISCARD
		bool Is2D() const;
        //Toggling 2D state resets mesh data and detaches attached camera,
        //UpdateMeshData is called internally on success
        void Set2DState(bool newState);

        KNODISCARD
		const vector<Vertex>& GetVertices() const;
        void SetVertices(vector<Vertex>&& newVertices);

        KNODISCARD
		const vector<Vertex2D>& GetVertices2D() const;
        void SetVertices2D(vector<Vertex2D>&& newVertices);

        KNODISCARD
		const vector<u32>& GetIndices() const;
        void SetIndices(vector<u32>&& newIndices);

        KNODISCARD
		const mat4& GetMatrix() const;

        //Should be called after updating any mesh data
        void UpdateMeshData();

        void Destroy();
    private:
        ~Mesh();

        void ClearAllData();

        void UpdateVertices();
        void UpdateIndices();

        bool isDestroyingCamera{};

        u32 ID{};
        u32 shaderID{};
        u32 hitTestID{};
        u32 cameraID{};
        u32 textureID{};

        bool is2D{};

        Transform3D transform{};

        //vertex data

        vector<Vertex> vertices{};
        vector<Vertex2D> vertices2D{};

        VkBuffer vkVertexBuffer{};
        u64 verticesSize{};
        VmaAllocation vmaVertexAllocation{};
        void* vertexMappedPtr{};

        //index data

        vector<u32> indices{};
        VkBuffer vkIndexBuffer{};
        u64 indicesSize{};
        VmaAllocation vmaIndexAllocation{};
        void* indexMappedPtr{};

        //mesh matrix data

        mat4 meshMatrix{};

        bool isDirty{};

        VkBuffer vkMeshUBOBuffer{};
        VmaAllocation vmaMeshUBOAllocation{};
        void* meshUBOMappedPtr{};

        VkDescriptorSet vkDescriptorSet{};
    };
}