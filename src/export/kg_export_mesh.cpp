//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "export_glb.hpp"
#include "log_utils.hpp"

#include "export/kg_export_mesh.hpp"
#include "resources/kg_mesh.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaExportGLB::ExportSingleMesh;

using KalaGraphics::Resources::Mesh;
using KalaGraphics::Resources::Vertex;

using std::string;
using std::to_string;

namespace KalaGraphics::Export
{
    void ExportMesh::ExportSingle(
        u32 meshID,
        const path& targetPath)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            Log::Print(
                "Failed to export mesh '" + to_string(meshID) 
                + "' because it was invalid! Reason: " + err,
                "KG_EXPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        err = ExportSingleMesh();
    }

    void ExportMesh::ExportMultiple(
        const vector<u32>& meshIDs,
        const path& targetPath)
    {

    }
}