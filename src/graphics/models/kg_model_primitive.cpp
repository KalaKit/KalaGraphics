//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"
#include "math_utils.hpp"
#include "import_kmd.hpp"

#include "graphics/models/kg_model_primitive.hpp"
#include "graphics/models/kg_model.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec3;
using KalaHeaders::KalaMath::MIN_POS3;
using KalaHeaders::KalaMath::MAX_POS3;
using KalaHeaders::KalaMath::MIN_SIZE3;
using KalaHeaders::KalaMath::MAX_SIZE3;

using KalaHeaders::KalaModelData::Vertex;

using KalaGraphics::Graphics::Model;
using KalaGraphics::Graphics::CubeDetails;
using KalaGraphics::Graphics::PyramidDetails;
using KalaGraphics::Graphics::SphereDetails;
using KalaGraphics::Graphics::TorusDetails;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;

using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::unique_ptr;
using std::make_unique;

enum class PrimitiveType
{
    PT_CUBE,
    PT_PYRAMID,
    PT_SPHERE,
    PT_TORUS
};

static string_view PrimitiveToString(PrimitiveType type)
{
    switch (type)
    {
        default:
        case PrimitiveType::PT_CUBE:    return "cube";
        case PrimitiveType::PT_PYRAMID: return "pyramid";
        case PrimitiveType::PT_SPHERE:  return "sphere";
        case PrimitiveType::PT_TORUS:   return "torus";
    }
}

static bool ContextExists(u32 contextID)
{
    return GraphicsContext::GetRegistry().createdContent.contains(contextID);
} 

static bool IsCorrectPos(const vec3& pos)
{
    return pos > MIN_POS3 && pos < MAX_POS3;
}
static bool IsCorrectSize(const vec3& size)
{
    return size > MIN_SIZE3 && size < MAX_SIZE3;
}

struct MeshData
{
	vector<Vertex> vertices{};
	vector<u32> indices{};
};

static MeshData CreateCube(
    const vec3& pos,
    const vec3& size,
    CubeDetails cDet)
{
    MeshData mesh{};

    mesh.vertices =
    {
        //front face
        { {-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 1}, {1, 0, 0, 1} },
        { {-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1} },
        { { 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1} },
        { { 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 1}, {1, 0, 0, 1} },

        //back face
        { { 0.5f,  0.5f, -0.5f}, {0, 0, -1}, {0, 1}, {-1, 0, 0, 1} },
        { { 0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 0}, {-1, 0, 0, 1} },
        { {-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 0}, {-1, 0, 0, 1} },
        { {-0.5f,  0.5f, -0.5f}, {0, 0, -1}, {1, 1}, {-1, 0, 0, 1} },

        //left face
        { {-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 1}, {0, 0, 1, 1} },
        { {-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}, {0, 0, 1, 1} },
        { {-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 0}, {0, 0, 1, 1} },
        { {-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 1}, {0, 0, 1, 1} },

        //right face
        { { 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {0, 1}, {0, 0, -1, 1} },
        { { 0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 0}, {0, 0, -1, 1} },
        { { 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 0}, {0, 0, -1, 1} },
        { { 0.5f,  0.5f, -0.5f}, {1, 0, 0}, {1, 1}, {0, 0, -1, 1} },

        //top face
        { {-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 1}, {1, 0, 0, 1} },
        { {-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 0}, {1, 0, 0, 1} },
        { { 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 0}, {1, 0, 0, 1} },
        { { 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 1}, {1, 0, 0, 1} },

        //bottom face
        { {-0.5f, -0.5f,  0.5f}, {0, -1, 0}, {0, 1}, {1, 0, 0, 1} },
        { {-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 0}, {1, 0, 0, 1} },
        { { 0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0}, {1, 0, 0, 1} },
        { { 0.5f, -0.5f,  0.5f}, {0, -1, 0}, {1, 1}, {1, 0, 0, 1} },
    };

    mesh.indices =
    {
        0,  1,  2,   0,  2,  3,  //front
        4,  5,  6,   4,  6,  7,  //back
        8,  9, 10,   8, 10, 11,  //left
        12, 13, 14,  12, 14, 15,  //right
        16, 17, 18,  16, 18, 19,  //top
        20, 21, 22,  20, 22, 23,  //bottom
    };

    return mesh;
}
static MeshData CreatePyramid(
    const vec3& pos,
    const vec3& size,
    PyramidDetails pDet)
{
    return {};
}
static MeshData CreateSphere(
    const vec3& pos,
    const vec3& size,
    SphereDetails sDet)
{
    return {};
}
static MeshData CreateTorus(
    const vec3& pos,
    const vec3& size,
    TorusDetails tDet)
{
    return {};
}

static bool VerifyPrimitive(
    const vec3& pos,
    const vec3& rot,
    const vec3& size,
    PrimitiveType type,
    CubeDetails cDet,
    PyramidDetails pDet,
    SphereDetails sDet,
    TorusDetails tDet)
{
    if (!IsCorrectPos(pos))
    {
        Log::Print(
            "Failed to create primitive of type '" + string(PrimitiveToString(type)) + "' because it had an invalid position!",
            "KG_MODEL_PRIMITIVE",
            LogType::LOG_ERROR,
            2);
        
        return false;
    }
    if (!IsCorrectSize(size))
    {
        Log::Print(
            "Failed to create primitive of type '" + string(PrimitiveToString(type)) + "' because it had an invalid size!",
            "KG_MODEL_PRIMITIVE",
            LogType::LOG_ERROR,
            2);
        
        return false;
    }

    if ((type == PrimitiveType::PT_CUBE
        && cDet.edges < 3)
        || (type == PrimitiveType::PT_PYRAMID
        && pDet.edges < 3))
    {
        Log::Print(
            "Failed to create primitive of type '" + string(PrimitiveToString(type)) + "' because its edge count was too low!",
            "KG_MODEL_PRIMITIVE",
            LogType::LOG_ERROR,
            2);
        
        return false;
    }
    if (type == PrimitiveType::PT_SPHERE
        && sDet.sphereDetailLevel < 1)
    {
        Log::Print(
            "Failed to create primitive of type '" + string(PrimitiveToString(type)) + "' because its detail level was too low!",
            "KG_MODEL_PRIMITIVE",
            LogType::LOG_ERROR,
            2);
        
        return false;
    }
    if (type == PrimitiveType::PT_TORUS
        && tDet.innerRadius > tDet.outerRadius)
    {
        Log::Print(
            "Failed to create primitive of type '" + string(PrimitiveToString(type)) + "' because its inner radius was bigger than its outer radius!",
            "KG_MODEL_PRIMITIVE",
            LogType::LOG_ERROR,
            2);
        
        return false;
    }

    return true;
}

namespace KalaGraphics::Graphics
{
    Model_Primitive* Model_Primitive::CreatePrimitive(
        string_view modelName,
        u32 contextID,
        const vec3& pos,
        const vec3& rot,
        const vec3& size,
        CubeDetails cDet)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to create primitive of type 'cube' because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_MODEL_PRIMITIVE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (!VerifyPrimitive(
            pos,
            rot,
            size,
            PrimitiveType::PT_CUBE,
            cDet,
            {},
            {},
            {}))
        {
            return nullptr;
        }

        unique_ptr<Model_Primitive> newModel = make_unique<Model_Primitive>();
        Model_Primitive* modelPtr = newModel.get();

        modelPtr->SetName(modelName);

        MeshData md = CreateCube(pos, size, cDet);

        modelPtr->vertices = std::move(md.vertices);
        modelPtr->indices = std::move(md.indices);

        modelPtr->SetBackend(contextID);

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        modelPtr->ID = newID;

        Model::GetRegistry().AddContent(newID, std::move(newModel));

        Log::Print(
            "Created new model '" + string(modelName) + "' with ID '" + to_string(newID) + "'!",
            "KG_MODEL_PRIMITIVE",
            LogType::LOG_SUCCESS);

        return modelPtr;
    }

    //Create a new pyramid
    Model_Primitive* Model_Primitive::CreatePrimitive(
        string_view modelName,
        u32 contextID,
        const vec3& pos,
        const vec3& rot,
        const vec3& size,
        PyramidDetails pDet)
    {
        KalaGraphicsCore::ForceClose(
            "Not implemented",
            "Feature \"Create pyramid primitive\" is not yet implemented!",
            true);

        return nullptr;
    }

    //Create a new sphere
    Model_Primitive* Model_Primitive::CreatePrimitive(
        string_view modelName,
        u32 contextID,
        const vec3& pos,
        const vec3& rot,
        const vec3& size,
        SphereDetails sDet)
    {
        KalaGraphicsCore::ForceClose(
            "Not implemented",
            "Feature \"Create sphere primitive\" is not yet implemented!",
            true);

        return nullptr;
    }

    //Create a new torus
    Model_Primitive* Model_Primitive::CreatePrimitive(
        string_view modelName,
        u32 contextID,
        const vec3& pos,
        const vec3& rot,
        const vec3& size,
        TorusDetails tDet)
    {
        KalaGraphicsCore::ForceClose(
            "Not implemented",
            "Feature \"Create torus primitive\" is not yet implemented!",
            true);

        return nullptr;
    }

    void Model_Primitive::Update()
    {
        //update vulkan model here
    }

    Model_Primitive::~Model_Primitive()
    {
        Log::Print(
            "Destroying model '" + name + "' with ID '" + to_string(ID) + "'.",
            "KG_MODEL_PRIMITIVE",
            LogType::LOG_INFO);
    }
}