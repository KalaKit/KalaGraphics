//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <vector>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Import
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::filesystem::path;
    using std::vector;
    using std::string;
    using std::string_view;
    using std::default_delete;

    static constexpr string_view EXT_GLTF = ".gltf";
    static constexpr string_view EXT_KMOD = ".kmod";

    struct MeshData
    {

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
		const MeshData& GetMeshData() const;

        void Destroy();
    private:
        ~ImportMesh();

        KNODISCARD
		static string Init_GLTF(
            vector<u8>&& binaryData,
            MeshData& outMeshData);
        KNODISCARD
		static string Init_KMOD(
            vector<u8>&& binaryData,
            MeshData& outMeshData);

        u32 ID{};

        path meshPath{};
        MeshData meshData{};
    };
}