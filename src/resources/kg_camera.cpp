//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "resources/kg_camera.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::PosTarget::POS_WORLD;
using KalaHeaders::KalaMath::RotTarget::ROT_WORLD;
using KalaHeaders::KalaMath::setpos3d;
using KalaHeaders::KalaMath::setroteuler;
using KalaHeaders::KalaMath::view;
using KalaHeaders::KalaMath::ortho;
using KalaHeaders::KalaMath::perspective;
using KalaHeaders::KalaMath::createumodel;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;

using std::unique_ptr;
using std::make_unique;
using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Camera> registry{};

    KalaGraphicsRegistry<Camera>& Camera::GetRegistry() { return registry; }

    Camera* Camera::Initialize(
        u32 contextID, 
        CameraType type,
        vec3&& pos,
        vec3&& rot,
        f32 fov,
        vec2 drawDistance)
    {
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (!gctx)
        {
            Log::Print(
                "Failed to create camera because the graphics context '" + to_string(contextID) + "' was invalid!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (type == CameraType::C_INVALID)
        {
            Log::Print(
                "Failed to create camera because its type was unassigned!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (fov < FOV_MIN
            || fov > FOV_MAX)
        {
            Log::Print(
                "Failed to create camera because its fov was out of range!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (drawDistance.x < DRAW_DISTANCE_MIN
            || drawDistance.y > DRAW_DISTANCE_MAX)
        {
            Log::Print(
                "Failed to create camera because its draw distance was out of range!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (drawDistance.x >= drawDistance.y)
        {
            Log::Print(
                "Failed to create camera because its min draw distance"
                "was equal to or bigger than its max draw distance!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Camera> newCamera = make_unique<Camera>();
        Camera* cameraPtr = newCamera.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        cameraPtr->ID = newID;
        cameraPtr->contextID = contextID;

        cameraPtr->type = type;

        setpos3d(
            cameraPtr->transform,
            {},
            POS_WORLD,
            std::move(pos));
        setroteuler(
            cameraPtr->transform,
            {},
            ROT_WORLD,
            std::move(rot));

        cameraPtr->fov = fov;
        cameraPtr->drawDistance = drawDistance;
        cameraPtr->viewport = gctx->GetExtent();

        //graphics context references this camera
        gctx->cameraIDs.push_back(newID);

        registry.AddContent(newID, std::move(newCamera));

        Log::Print(
			"Created new camera '" + to_string(newID) 
            + "' for graphics context '" + to_string(contextID) + "'!",
			"KG_CAMERA",
			LogType::LOG_SUCCESS);

        return cameraPtr;
    }

    u32 Camera::GetID() const { return ID; }

    u32 Camera::GetGraphicsContextID() const { return contextID; }
    void Camera::SetGraphicsContextID(u32 newValue)
    {
        if (contextID == newValue)
        {
            Log::Print("Failed to set camera '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        GraphicsContext* oldGctx = GraphicsContext::GetRegistry().GetContent(contextID);
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(newValue);
        if (!gctx)
        {
            Log::Print("Failed to set camera '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it was invalid!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        contextID = newValue;

        erase(
            oldGctx->cameraIDs,
            ID);
        gctx->cameraIDs.push_back(ID);

        Log::Print(
            "Set camera '" + to_string(ID) 
            + "' graphics context ID to '" + to_string(contextID) + "'!",
            "KG_CAMERA",
            LogType::LOG_SUCCESS);
    }

    void Camera::Destroy()
    {
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (gctx
            && !isDestroyingGraphicsContext)
        {
            erase(
                gctx->cameraIDs, 
                ID);
        }

        registry.RemoveContent(ID);
    }

    Camera::~Camera()
    {
        Log::Print(
            "Destroying camera '" + to_string(ID) + "'.",
            "KG_CAMERA",
            LogType::LOG_INFO);
    }
}