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
#include "import_kmd.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Graphics
{
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::vec4;
    using KalaHeaders::KalaMath::Transform3D;

    using KalaHeaders::KalaModelData::Vertex;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::string;
    using std::string_view;
    using std::filesystem::path;

    using u32 = uint32_t;

    struct LIB_API Model_Cube
    {
        //ranges from 3 to 255,
        //used for top and bottom edges
        u8 edgeCount{};
    };

    struct LIB_API Model_Pyramid
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
    struct LIB_API Model_Sphere
    {
        //clamped from 0.01f to 10000.0f
        f32 radius = 0.5f;
        //clamped from 1 to 255
        u8 detailLevel{};

        SphereType type{};
    };

    class LIB_API Model
    {
    public:
        static KalaGraphicsRegistry<Model>& GetRegistry();

        //Initialize a model from a path
        static Model* Initialize(
            string_view modelName,
            const path& modelPath,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            u32 contextID,
            u32 vulkanID,
            u32 shaderID);

        //Initialize a raw model with basic data
        static Model* Initialize(
            string_view modelName,
            vector<Vertex> vertices,
		    vector<u32> indices,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            u32 contextID,
            u32 vulkanID,
            u32 shaderID);

        //Initialize a cube or cylinder with predefined model data
        static Model* Initialize(
            string_view modelName,
            Model_Cube cubeData,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            u32 contextID,
            u32 vulkanID,
            u32 shaderID);

        //Initialize a pyramid or cone with predefined model data
        static Model* Initialize(
            string_view modelName,
            Model_Pyramid pyramidData,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            u32 contextID,
            u32 vulkanID,
            u32 shaderID);

        //Initialize a sphere with predefined model data
        static Model* Initialize(
            string_view modelName,
            Model_Sphere sphereData,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            u32 contextID,
            u32 vulkanID,
            u32 shaderID);

        void SetName(string_view newName);
        const string& GetName() const;

        vector<Vertex>& GetVertices();
        vector<u32>& GetIndices();

        u32 GetID() const;
        u32 GetGraphicsContextID() const;
        u32 GetVulkanContextID() const;
        u32 GetShaderID() const;

        void Update();

        ~Model();
    private:
        static Model* InitializeInternal(
            string_view modelName,
            vector<Vertex> vertices,
		    vector<u32> indices,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            u32 contextID,
            u32 vulkanID,
            u32 shaderID);

        string name{};

        Transform3D transform{};

        u32 ID{};

        u32 contextID{};
        u32 vulkanID{};
        u32 shaderID{};

		vector<Vertex> vertices{};
		vector<u32> indices{};
    };
}