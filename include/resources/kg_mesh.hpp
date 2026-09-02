//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <functional>

#include "core_utils.hpp"
#include "math_utils.hpp"
#include "key_standards.hpp"

#include "core/kg_core.hpp"
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
    using KalaGraphics::Core::KalaGraphicsCore;
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using KalaHeaders::KalaMath::Transform3D;
    using KalaHeaders::KalaMath::Transform2D;
    using KalaHeaders::KalaMath::mat4;
    using KalaHeaders::KalaMath::vec4;
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::vec2;

    using KalaHeaders::KalaKeyStandards::KeyboardButton;
    using KalaHeaders::KalaKeyStandards::MouseButton;

    using std::vector;
    using std::unordered_map;
    using std::pair;
    using std::string;
    using std::to_string;
    using std::filesystem::path;
    using std::function;
    using std::same_as;
    using std::default_delete;

    static constexpr u8 MIN_CUBE_EDGE_COUNT = 3;
    static constexpr u8 MAX_CUBE_EDGE_COUNT = 32;

    static constexpr u8 MIN_PYRAMID_EDGE_COUNT = 3;
    static constexpr u8 MAX_PYRAMID_EDGE_COUNT = 32;

    static constexpr u8 MIN_SPHERE_DETAIL_LEVEL = 1;
    static constexpr u8 MAX_SPHERE_DETAIL_LEVEL = 8;

    enum class FaceDirection : u8
    {
        //faces and normals point outwards
        F_OUT = 0,
        //faces and normals point inwards
        F_IN = 1
    };

    enum class NormalType : u8
    {
        //one normal per face, often requiring duplicated vertices
        N_FLAT = 0,
        //one normal per shared vertex, with interpolation between them
        N_SMOOTH = 1
    };

    struct LIB_API Mesh_Cube
    {
        //clamped from 3 to 32,
        //used for top and bottom edges
        u8 edgeCount = 3;

        FaceDirection faceDir{};
        NormalType normalType{};
    };

    struct LIB_API Mesh_Pyramid
    {
        //clamped from 3 to 32,
        //used for bottom edges
        u8 edgeCount = 3;

        FaceDirection faceDir{};
        NormalType normalType{};
    };

    struct LIB_API Mesh_Sphere
    {
        //clamped from 1 to 8
        u8 detailLevel = 1;

        FaceDirection faceDir{};
        NormalType normalType = NormalType::N_SMOOTH;
    };

    enum class AnchorPosition : u8
    {
        P_DEFAULT = 0,

        P_BOTTOM_LEFT = 1,
        P_BOTTOM_RIGHT = 2,

        P_TOP_LEFT = 3,
        P_TOP_RIGHT = 4,
        
        P_CENTER = 5
    };

    struct LIB_API TransformData
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
    };
    struct LIB_API Vertex2D
    {
        //X, Y
        vec2 pos{};
        //U, V texture coordinates
        vec2 uv{};
    };

    //Output after generating a meshes data
    struct LIB_API MeshData
    {
        vector<Vertex> vertices{};
        vector<u32> indices{};
    };

    //TODO: add instancing

    template<typename T>
    concept TransformType =
        same_as<T, Transform2D>
        || same_as<T, Transform3D>;

    struct LIB_API Transform
    {
        template<TransformType T>
        constexpr operator T&()
        {
            if constexpr (same_as<T, Transform2D>)
            {
                if (!is2D)
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics mesh error",
                        "Failed to get transform because 3D mesh '" + to_string(ID) + "' "
                        "does not allow to return its 2D transform!");
                }

                return transform2D;
            }
            else
            {
                if (is2D)
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics mesh error",
                        "Failed to get transform because 2D mesh '" + to_string(ID) + "' "
                        "does not allow to return its 3D transform!");
                }

                return transform3D;
            }
        }
    private:
        friend class Mesh;

        Transform(
            Transform3D& transform3D,
            Transform2D& transform2D,
            bool is2D,
            u32 ID) :
            transform3D(transform3D),
            transform2D(transform2D),
            is2D(is2D),
            ID(ID) {}

        Transform3D& transform3D;
        Transform2D& transform2D;
        bool is2D{};
        u32 ID{};
    };

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

        //Create a new mesh, mesh type is derived from shader,
        //2D mesh creates its own canonical data during initialization, 
        //3D mesh stays empty and must be updated via SetMeshData, 
        //all meshes require a shader even if that shader is also blank,
        //all meshes require a texture even if that texture is also a default 1x1 texture
        KNODISCARD
		static Mesh* Initialize(
            u32 shaderID,
            u32 textureID);

        //Generate a 3D cube or 3D cylinder
        KNODISCARD
		static MeshData GenerateMeshData(Mesh_Cube cubeData);
        //Generate a 3D pyramid or 3D cone
        KNODISCARD
		static MeshData GenerateMeshData(Mesh_Pyramid pyramidData);
        //Generate a 3D sphere
        KNODISCARD
		static MeshData GenerateMeshData(Mesh_Sphere sphereData);

        KNODISCARD
		u32 GetID() const;
        KNODISCARD
		u32 GetCameraID() const;

        KNODISCARD
        u32 GetShaderID() const;
        //Swap mesh shader at runtime, not allowed to switch to a
        //3D shader if mesh is 2D and vice versa
        void SetShaderID(u32 newID);

        KNODISCARD
		u32 GetTextureID() const;
        void SetTextureID(u32 newID);

        //Returns true if this 2D or 3D mesh is
        //currently being detected by the Hit Test logic
        KNODISCARD
        bool IsHovered() const;

        KNODISCARD
        bool IsVisible() const;
        void SetVisibleState(bool newValue);

        KNODISCARD
		bool Is2D() const;

        KNODISCARD
        u16 GetDrawOrderIndex() const;
        //Set the mesh draw order, not used for 3D meshes,
        //the next global update will sort all meshes
        void SetDrawOrderIndex(u16 newValue);

        KNODISCARD
        Transform GetTransform();

        AnchorPosition GetLocalAnchorPosition() const;
        //Automatically always updates this mesh transform position relative to local anchor,
        //not used for 3D meshes
        void SetLocalAnchorPosition(AnchorPosition pos);

        AnchorPosition GetViewportAnchorPosition() const;
        //Automatically always updates this mesh transform position relative to viewport anchor,
        //not used for 3D meshes
        void SetViewportAnchorPosition(AnchorPosition pos);

        KNODISCARD
        const vec4& GetColor() const;
        void SetColor(vec4&& newValue);

        KNODISCARD
        bool IsTransparent() const;
        //If true, then this mesh is filtered separately from opaque models
        //and allows to use .w in color and textures
        void SetTransparentState(bool newValue);

        KNODISCARD
		const vector<Vertex>& GetVertices() const;
        KNODISCARD
		const vector<Vertex2D>& GetVertices2D() const;
        KNODISCARD
		const vector<u32>& GetIndices() const;

        //Only for 3D meshes, allows runtime data modification
        void SetMeshData(MeshData&& meshData);

        KNODISCARD
		const mat4& GetMatrix() const;

        //Called while hovered 
        void SetHoverCallback(function<void()>&& newValue);
        //Called once when hover starts
        void SetOnHoverStartCallback(function<void()>&& newValue);
        //Called once when hover exits
        void SetOnHoverExitCallback(function<void()>&& newValue);

        void SetKeyHeldCallback(
            KeyboardButton btn, 
            function<void()>&& newValue,
            bool requireHover = true);
        void SetKeyPressedCallback(
            KeyboardButton btn, 
            function<void()>&& newValue,
            bool requireHover = true);
        void SetKeyReleasedCallback(
            KeyboardButton btn, 
            function<void()>&& newValue,
            bool requireHover = true);

        void SetMouseButtonHeldCallback(
            MouseButton btn, 
            function<void()>&& newValue,
            bool requireHover = true);
        void SetMouseButtonPressedCallback(
            MouseButton btn, 
            function<void()>&& newValue,
            bool requireHover = true);
        void SetMouseButtonReleasedCallback(
            MouseButton btn, 
            function<void()>&& newValue,
            bool requireHover = true);
        void SetMouseButtonDoubleClickedCallback(
            MouseButton btn, 
            function<void()>&& newValue,
            bool requireHover = true);
        void SetMouseButtonDraggingCallback(
            MouseButton btn, 
            function<void(vec2)>&& newValue,
            bool requireHover = true);

        void SetScrollUpCallback(
            function<void(f32)>&& newValue,
            bool requireHover = true);
        void SetScrollDownCallback(
            function<void(f32)>&& newValue,
            bool requireHover = true);

        void Destroy();
    private:
        ~Mesh();

        void ClearAllData();

        void UpdateMeshData();

        u32 ID{};
        u32 shaderID{};
        u32 hitTestID{};
        u32 cameraID{};
        u32 textureID{};

        u16 drawOrderIndex{};

        bool isBufferDataDirty{};
        bool isMeshDataDirty{};

        bool isVisible = true;

        bool isDestroyingCamera{};

        bool is2D{};

        vec2 finalAnchorPos{};

        Transform3D transform3D{};
        Transform2D transform2D{};
        vec3 lastPos{}; //used for detecting if mesh has moved

        AnchorPosition localAnchor{};
        AnchorPosition viewportAnchor{};

        //RGBA color - default is white
        vec4 color = 1;
        //shader stores as u32 instead of bool
        u32 isTransparent{};

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

        //callbacks

        function<void()> hoverCallback{};
        function<void()> onHoverStartCallback{};
        function<void()> onHoverExitCallback{};

        unordered_map<KeyboardButton, pair<bool, function<void()>>> keyHeldCallbacks{};
        unordered_map<KeyboardButton, pair<bool, function<void()>>> keyPressedCallbacks{};
        unordered_map<KeyboardButton, pair<bool, function<void()>>> keyReleasedCallbacks{};

        unordered_map<MouseButton, pair<bool, function<void()>>> mouseButtonHeldCallbacks{};
        unordered_map<MouseButton, pair<bool, function<void()>>> mouseButtonPressedCallbacks{};
        unordered_map<MouseButton, pair<bool, function<void()>>> mouseButtonReleasedCallbacks{};
        unordered_map<MouseButton, pair<bool, function<void()>>> mouseButtonDoubleClickedCallbacks{};
        unordered_map<MouseButton, pair<bool, function<void(vec2)>>> mouseButtonDraggingCallbacks{};

        pair<bool, function<void(f32)>> scrollUpCallback{};
        pair<bool, function<void(f32)>> scrollDownCallback{};
    };
}