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
    using KalaHeaders::KalaMath::vec4;
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::vec2;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::string;
    using std::filesystem::path;
    using std::default_delete;

    enum class AnchorPosition : u8
    {
        P_DEFAULT = 0,

        P_BOTTOM_LEFT = 1,
        P_BOTTOM_RIGHT = 2,

        P_TOP_LEFT = 3,
        P_TOP_RIGHT = 4,
        
        P_CENTER = 5
    };

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
        bool IsVisible() const;
        void SetVisibleState(bool newValue);

        KNODISCARD
		bool Is2D() const;

        KNODISCARD
        u16 GetDrawOrderIndex() const;
        //Set the mesh draw order, set sortNow to true
        //if you want this call to sort all meshes, 
        //otherwise the next global update will sort all meshes,
        //not used for 3D meshes
        void SetDrawOrderIndex(
            u16 newValue,
            bool sortNow = false);

        KNODISCARD
		Transform3D& GetTransform();

        AnchorPosition GetLocalAnchorPosition() const;
        //Automatically always updates this mesh transform position relative to local anchor,
        //not used for 3D meshes
        void SetLocalAnchorPosition(AnchorPosition pos);

        AnchorPosition GetViewportAnchorPosition() const;
        //Automatically always updates this mesh transform position relative to viewport anchor,
        //not used for 3D meshes
        void SetViewportAnchorPosition(AnchorPosition pos);

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

        void Destroy();
    private:
        ~Mesh();

        void ClearAllData();

        void UpdateMeshData();

        void UpdateVertices();
        void UpdateIndices();

        u32 ID{};
        u32 shaderID{};
        u32 hitTestID{};
        u32 cameraID{};
        u32 textureID{};

        u16 drawOrderIndex{};

        bool isBufferDataDirty{};
        bool isVertexDataDirty{};
        bool isIndexDataDirty{};

        bool isVisible = true;

        bool isDestroyingCamera{};

        bool is2D{};

        Transform3D transform{};

        AnchorPosition localAnchor{};
        AnchorPosition viewportAnchor{};

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

        VkBuffer vkMeshUBOBuffer{};
        VmaAllocation vmaMeshUBOAllocation{};
        void* meshUBOMappedPtr{};

        VkDescriptorSet vkDescriptorSet{};
    };
}