//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <vector>

#include "core_utils.hpp"

namespace KalaGraphics::Export
{
    using std::filesystem::path;
    using std::vector;

    class LIB_API ExportMesh
    {
    public:
        //Exports a mesh to a new .glb file
        static void ExportSingle(
            u32 meshID,
            const path& targetPath);

        //Exports multiple meshes to a new .glb file
        static void ExportMultiple(
            const vector<u32>& meshIDs,
            const path& targetPath);
    };
}