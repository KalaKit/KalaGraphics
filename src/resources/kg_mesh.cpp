//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "resources/kg_mesh.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaMath::toquat;

using KalaGraphics::Core::KalaGraphicsCore;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Mesh> registry{};

    KalaGraphicsRegistry<Mesh>& Mesh::GetRegistry() { return registry; }

    Mesh* Mesh::Initialize(
        MeshType meshType,
        Transform&& transform,
        vector<Vertex>&& vertices)
    {
        if (meshType == MeshType::M_INVALID)
        {

        }
        else if (meshType == MeshType::M_2D)
        {

        }
        else
        {

        }

        unique_ptr<Mesh> newMesh = make_unique<Mesh>();
        Mesh* meshPtr = newMesh.get();

        meshPtr->meshType = meshType;
        meshPtr->vertices = std::move(vertices);
        meshPtr->transform.pos_world = transform.pos;
        meshPtr->transform.rot_world = toquat(transform.rot);
        meshPtr->transform.size_world = transform.size;

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);
        meshPtr->ID = newID;

        registry.AddContent(newID, std::move(newMesh));

        Log::Print(
			"Created new mesh '" + to_string(newID) + "'!",
			"KG_MESH",
			LogType::LOG_SUCCESS);

        return meshPtr;
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