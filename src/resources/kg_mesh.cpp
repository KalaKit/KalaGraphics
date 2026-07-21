//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "resources/kg_mesh.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Mesh> registry{};

    KalaGraphicsRegistry<Mesh>& Mesh::GetRegistry() { return registry; }

    Mesh* Mesh::Initialize(
        MeshType meshType,
        const Transform& transform,
        const vector<Vertex>& vertices)
    {
        /*TODO: fill*/

        return nullptr;
    }

    u32 Mesh::GetID() const { return ID; }

    void Mesh::Destroy()
    {
        /*TODO: fill*/
    }

    Mesh::~Mesh()
    {
        Log::Print(
            "Destroying mesh '" + to_string(ID) + "'.",
            "KG_MESH",
            LogType::LOG_INFO);

        /*TODO: fill*/
    }
}