//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <vector>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

#include "resources/kg_mesh.hpp"

namespace KalaGraphics::Import
{
    using KalaHeaders::KalaMath::vec4;
    using KalaHeaders::KalaMath::Transform3D;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using KalaGraphics::Resources::Vertex;

    using std::filesystem::path;
    using std::vector;
    using std::string;
    using std::default_delete;

    struct ImportMeshData
    {
        vector<Vertex> vertices{};
        vector<u32> indices{};
    };
    struct ImportMaterialData
    {
        string materialName{};
        //RGBA color, defaults to white opaque
        vec4 baseColor{ 1.0f };

        //TODO: add texture data and material slots...
    };

    struct ImportPrimitiveData
    {
        ImportMeshData meshData{};
        ImportMaterialData matData{};

        //TODO: add import anim, bone data etc...
    };

    struct ImportNodeData
    {
        string nodeName{};
        Transform3D transform{};
        
        vector<ImportPrimitiveData> primitiveData{};
    };

    class LIB_API ImportMesh
    {
    friend struct default_delete<ImportMesh>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<ImportMesh>& GetRegistry();

        KNODISCARD
		static ImportMesh* Initialize(path&& meshPath);

        KNODISCARD
		u32 GetID() const;

        KNODISCARD
		const path& GetMeshPath() const;
        KNODISCARD
		const vector<ImportNodeData>& GetMeshData() const;

        void Destroy();
    private:
        ~ImportMesh();

        KNODISCARD
		static string Init_GLTF_GLB(
            const path& meshPath,
            vector<u8>&& binaryData,
            vector<ImportNodeData>& outNodeData);
        KNODISCARD
		static string Init_KMOD(
            vector<u8>&& binaryData,
            vector<ImportNodeData>& outNodeData);

        u32 ID{};

        path meshPath{};
        vector<ImportNodeData> nodeData{};
    };
}