//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "import/kg_import_mesh.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaGraphics::Core::KalaGraphicsCore;

using std::to_string;
using std::unique_ptr;
using std::make_unique;
using std::filesystem::is_regular_file;

namespace KalaGraphics::Import
{
    static KalaGraphicsRegistry<ImportMesh> registry{};

    KalaGraphicsRegistry<ImportMesh>& ImportMesh::GetRegistry() { return registry; }

    ImportMesh* ImportMesh::Initialize(path&& meshPath)
    {
        if (!is_regular_file(meshPath))
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "' because it is not a regular file!",
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        string ext = meshPath.extension().string();
        if (ext != EXT_GLTF
            && ext != EXT_KMOD)
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "' because its extension is not supported!",
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        vector<u8> outData{};
        string errMsg = ReadBinaryDataFromFile(
            meshPath,
            outData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        MeshData meshData{};
        if (ext == ".png")
        {
            errMsg = Init_GLTF(
                std::move(outData),
                meshData);
        }
        else
        {
            errMsg = Init_KMOD(
                std::move(outData),
                meshData);
        }

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<ImportMesh> newMesh = make_unique<ImportMesh>();
        ImportMesh* meshPtr = newMesh.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        meshPtr->ID = newID;
        meshPtr->meshPath = std::move(meshPath);
        meshPtr->meshData = std::move(meshData);

        string err = registry.AddContent(newID, std::move(newMesh));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics import mesh error",
				"Failed to initialize import mesh! Reason: " + err);
        }

        Log::Print(
			"Created new import mesh '" + to_string(newID) + "'!",
			"KG_IMPORT_MESH",
			LogType::LOG_SUCCESS);

        return meshPtr;
    }

    u32 ImportMesh::GetID() const { return ID; }

    const path& ImportMesh::GetMeshPath() const { return meshPath; }
    const MeshData& ImportMesh::GetMeshData() const { return meshData; }

    string ImportMesh::Init_GLTF(
        vector<u8>&& binaryData,
        MeshData& outMeshData)
    {
        return "init gltf";
    }
    string ImportMesh::Init_KMOD(
        vector<u8>&& binaryData,
        MeshData& outMeshData)
    {
        return "init kmod";
    }

    void ImportMesh::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics import mesh error",
                "Failed to destroy import mesh '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    ImportMesh::~ImportMesh()
    {
        Log::Print(
            "Destroying import mesh data '" + to_string(ID) + "'.",
            "KG_IMPORT_MESH",
            LogType::LOG_INFO);
    }
}