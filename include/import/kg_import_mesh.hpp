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

    using u8 = uint8_t;
    using u32 = uint32_t;

    static constexpr string_view EXT_GLTF = ".gltf";
    static constexpr string_view EXT_KMOD = ".kmod";

    struct MeshData
    {

    };

    class LIB_API ImportMesh
    {
    public:
        static KalaGraphicsRegistry<ImportMesh>& GetRegistry();

        static ImportMesh* Initialize(path&& meshPath);

        u32 GetID() const;

        const path& GetMeshPath() const;
        const MeshData& GetMeshData() const;

        void Destroy();

        ~ImportMesh();
    private:
        static string Init_GLTF(
            vector<u8>&& binaryData,
            MeshData& outMeshData);
        static string Init_KMOD(
            vector<u8>&& binaryData,
            MeshData& outMeshData);

        u32 ID{};

        path meshPath{};
        MeshData meshData{};
    };
}