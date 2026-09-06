//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "export_glb.hpp"
#include "log_utils.hpp"

#include "export/kg_export_mesh.hpp"
#include "resources/kg_mesh.hpp"

using KalaHeaders::KalaMath::vec4;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaExportGLB::ExportMeshData;
using KalaHeaders::KalaExportGLB::ExportMaterialData;
using KalaHeaders::KalaExportGLB::ExportNodeData;

using KalaGraphics::Resources::Mesh;
using KalaGraphics::Resources::Vertex;

using std::string;
using std::to_string;
using std::vector;

static string GetNodeData(
    const vector<u32>& meshIDs,
    vector<ExportNodeData>& outData);

namespace KalaGraphics::Export
{
    void ExportMesh::ExportMeshes(
        const vector<u32>& meshIDs,
        const path& exportPath)
    {
        vector<ExportNodeData> exportNodeData{};

        string err = GetNodeData(
            meshIDs,
            exportNodeData);

        if (!err.empty())
        {
            Log::Print(
                "Failed to export meshes to '" + exportPath.string() + "'! Reason: " + err,
                "KG_EXPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        err = KalaHeaders::KalaExportGLB::ExportMeshes(
            std::move(exportNodeData),
            exportPath);

        if (!err.empty())
        {
            Log::Print(
                "Failed to export meshes to '" + exportPath.string() + "'! Reason: " + err,
                "KG_EXPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Log::Print(
            "Finished exporting meshes to '" + exportPath.string() + "'!",
            "KG_EXPORT_MESH",
            LogType::LOG_SUCCESS);
    }

    string ExportMesh::GetJsonData(const vector<u32> &meshIDs)
    {
        vector<ExportNodeData> exportNodeData{};

        string err = GetNodeData(
            meshIDs,
            exportNodeData);

        if (!err.empty())
        {
            Log::Print(
                "Failed to get json data from meshes! Reason: " + err,
                "KG_EXPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return "";
        }

        string result{};
        err = KalaHeaders::KalaExportGLB::GetJsonDataFromNodeData(
            std::move(exportNodeData),
            result);

        if (!err.empty())
        {
            Log::Print(
                "Failed to get json data from meshes! Reason: " + err,
                "KG_EXPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return "";
        }

        return result;
    }
}

string GetNodeData(
    const vector<u32>& meshIDs,
    vector<ExportNodeData>& outData)
{
    vector<ExportNodeData> exportNodeData{};

    for (u32 mID : meshIDs)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(mID, m);
        if (!err.empty())
        {
            Log::Print(
                "Mesh '" + to_string(mID) + "' was invalid! Reason: " + err,
                "KG_EXPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return "";
        }

        //temporarily flip face direction before exporting
        m->FlipFaceDirection();

        const vector<Vertex>& meshVertices = m->GetVertices();
        const vector<u32>& meshIndices = m->GetIndices();

        ExportMeshData meshData{};
        meshData.vertices.resize(meshVertices.size());
        meshData.indices.resize(meshIndices.size());

        for (size_t i = 0; i < meshVertices.size(); i++)
        {
            const auto& vert = meshVertices[i];

            meshData.vertices[i].pos[0] = vert.pos.x;
            meshData.vertices[i].pos[1] = vert.pos.y;
            meshData.vertices[i].pos[2] = vert.pos.z;

            meshData.vertices[i].normal[0] = vert.normal.x;
            meshData.vertices[i].normal[1] = vert.normal.y;
            meshData.vertices[i].normal[2] = vert.normal.z;

            meshData.vertices[i].uv[0] = vert.uv.x;
            meshData.vertices[i].uv[1] = vert.uv.y;

            meshData.vertices[i].color[0] = vert.color.x;
            meshData.vertices[i].color[1] = vert.color.y;
            meshData.vertices[i].color[2] = vert.color.z;
            meshData.vertices[i].color[3] = vert.color.w;
        }

        for (size_t i = 0; i < meshIndices.size(); i++)
        {
            const auto& ind = meshIndices[i];

            meshData.indices[i] = ind;
        }

        //revert back to original face direction
        m->FlipFaceDirection();

        ExportMaterialData matData{};
        const vec4& meshColor = m->GetColor();
        matData.baseColor[0] = meshColor.x;
        matData.baseColor[1] = meshColor.y;
        matData.baseColor[2] = meshColor.z;
        matData.baseColor[3] = meshColor.w;

        exportNodeData.push_back(
        { 
            .meshData = std::move(meshData),
            .matData = std::move(matData)
        });
    }

    outData = std::move(exportNodeData);

    return "";
}