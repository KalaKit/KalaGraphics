//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/kg_hit_test.hpp"

#include <memory>

#include "log_utils.hpp"

#include "core/kg_context.hpp"
#include "core/kg_viewport.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_mesh.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::radians;
using KalaHeaders::KalaMath::PosTarget;
using KalaHeaders::KalaMath::RotTarget;
using KalaHeaders::KalaMath::SizeTarget;
using KalaHeaders::KalaMath::Transform2D;

using KalaGraphics::Resources::Shader;
using KalaGraphics::Resources::Mesh;

using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Core
{
    static KalaGraphicsRegistry<HitTest> registry{};

    KalaGraphicsRegistry<HitTest>& HitTest::GetRegistry() { return registry; }

    HitTest* HitTest::Initialize(u32 contextID)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);

        unique_ptr<HitTest> newHT = make_unique<HitTest>();
        HitTest* htPtr = newHT.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        htPtr->ID = newID;

        htPtr->contextID = contextID;
        gctx->hitTestID = newID;

        err = registry.AddContent(newID, std::move(newHT));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics hit test error",
				"Failed to initialize hit test! Reason: " + err);
        }

        Log::Print(
            "Created new hit test '" + to_string(newID) + "'!",
            "KG_HIT_TEST",
            LogType::LOG_SUCCESS);

        return htPtr;
    }

    u32 HitTest::GetID() const { return ID; }
    u32 HitTest::GetContextID() const { return contextID; }

    u32 HitTest::GetViewportID() const { return viewportID; }
    u32 HitTest::Get3DMeshID() const { return mesh3DID; }
    u32 HitTest::Get2DMeshID() const { return mesh2DID; }

    void HitTest::Update()
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics hit test error", 
                "Failed to update hit test '" + to_string(ID) 
                + "' because its graphics context was invalid! Reason: " + err);
        }

        auto clear_old_data = [&]() -> void
            {
                if (viewportID != 0)
                {
                    Viewport* vp{};
                    string err = Viewport::GetRegistry().GetContent(viewportID, vp);
                    if (err.empty()) vp->hitTestID = 0;

                    viewportID = 0;
                }
                if (mesh3DID != 0)
                {
                    Mesh* m{};
                    string err = Mesh::GetRegistry().GetContent(mesh3DID, m);
                    if (err.empty()) m->hitTestID = 0;

                    mesh3DID = 0;
                }
                if (mesh2DID != 0)
                {
                    Mesh* m{};
                    string err = Mesh::GetRegistry().GetContent(mesh2DID, m);
                    if (err.empty()) m->hitTestID = 0;

                    mesh2DID = 0;
                }
            };

        /*
        Log::Print(
            "@@@@@\n"
            "mouse pos: " + to_string(mousePos.x) + "x" + to_string(mousePos.y)
            + "reverse pos: " + to_string(reverseMousePos.x) + "x" + to_string(reverseMousePos.y));
        */

        clear_old_data();

        if (gctx->mousePos < 0
            || gctx->mousePosYReversed < 0)
        {
            return;
        }

        auto hit_viewport = [gctx](vec2 pos, vec2 size) -> bool
            {
                return 
                    gctx->mousePos.x >= pos.x 
                    && gctx->mousePos.x < pos.x + size.x
                    && gctx->mousePos.y >= pos.y
                    && gctx->mousePos.y < pos.y + size.y;
            };

        auto hit_2d_mesh = [gctx](
            vec2 pos,
            vec2 size,
            Mesh* m) -> bool
            {
                //move mouse into rectangle-local space
                vec2 local = gctx->mousePosYReversed - pos;

                f32 rotation = radians(scast<Transform2D&>(m->GetTransform())
                    .getrot(RotTarget::ROT_WORLD));

                //inverse-rotate mouse around rectangle center
                const f32 c = cos(-rotation);
                const f32 s = sin(-rotation);

                vec2 rotated
                {
                    local.x * c - local.y * s,
                    local.x * s + local.y * c
                };

                //rectangle is now axis-aligned
                const vec2 halfSize = size * 0.5f;

                return 
                    rotated.x >= -halfSize.x 
                    && rotated.x < halfSize.x
                    && rotated.y >= -halfSize.y
                    && rotated.y < halfSize.y;
            };

        auto get_hit_viewports = [&]() -> vector<Viewport*>
            {
                vector<u32> existingViewports{};
                existingViewports.push_back(gctx->rootViewportID);
                for (u32 vpID : gctx->extraViewportIDs)
                {
                    existingViewports.push_back(vpID);
                }
                
                vector<Viewport*> hitViewports{};
                for (u32 vpID : existingViewports)
                {
                    Viewport* vp{};
                    string err = Viewport::GetRegistry().GetContent(vpID, vp);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics hit test error", 
                            "Failed to update hit test '" + to_string(ID) 
                            + "' because its graphics context '" + to_string(contextID) 
                            + "' viewport was invalid! Reason: " + err);
                    }

                    //ignore hidden viewports
                    if (!vp->isVisible) continue;

                    if (hit_viewport(vp->viewportOffset, vp->viewportDynamicSize))
                    {
                        hitViewports.push_back(vp);
                    }
                }

                return hitViewports;
            };

        auto get_2d_hit_meshes = [&](Viewport* vp) -> vector<Mesh*>
            {
                vector<u32> existingMeshes{};

                Shader* primary2DShader{};
                string err = Shader::GetRegistry().GetContent(vp->primary2DShaderID, primary2DShader);
                if (!err.empty())
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics hit test error", 
                        "Failed to update hit test '" + to_string(ID) 
                        + "' because viewport '" + to_string(vp->ID) 
                        + "' primary 2D shader was invalid! Reason: " + err);
                }

                existingMeshes = primary2DShader->meshIDs;

                for (u32 extra2DShaderID : vp->extra2DShaderIDs)
                {
                    Shader* extra2DShader{};
                    string err = Shader::GetRegistry().GetContent(extra2DShaderID, extra2DShader);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics hit test error", 
                            "Failed to update hit test '" + to_string(ID) 
                            + "' because viewport '" + to_string(vp->ID) 
                            + "' extra 2D shader was invalid! Reason: " + err);
                    }

                    for (u32 ID : extra2DShader->meshIDs)
                    {
                        existingMeshes.push_back(ID);
                    }
                }

                //Log::Print("@@@@@ 2d existing meshes count '" + to_string(existingMeshes.size()) + "'...");

                vector<Mesh*> hitMeshes{};
                for (u32 mID : existingMeshes)
                {
                    Mesh* m{};
                    string err = Mesh::GetRegistry().GetContent(mID, m);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics hit test error", 
                            "Failed to update hit test '" + to_string(ID) 
                            + "' because its mesh was invalid! Reason: " + err);
                    }

                    //ignore hidden meshes
                    if (!m->isVisible) continue;

                    Transform2D& t = m->GetTransform();
                    vec2 pos = t.getpos(PosTarget::POS_WORLD);

                    string mpos = 
                        to_string(pos.x) + "x" 
                        + to_string(pos.y);
                    string mfinalpos = 
                        to_string(m->finalAnchorPos.x) + "x" 
                        + to_string(m->finalAnchorPos.y);

                    /*
                    Log::Print(
                        "@@@@@\n"
                        "mesh transform pos: " + mpos 
                        + "\nmesh anchor pos: " + mfinalpos);
                    */

                    if (hit_2d_mesh(
                        m->finalAnchorPos, //mesh resolved anchor position for correct hit testing
                        t.getsize(SizeTarget::SIZE_WORLD), 
                        m))
                    {
                        hitMeshes.push_back(m);
                    }
                }

                //Log::Print("@@@@@ 2d hit meshes count '" + to_string(hitMeshes.size()) + "'...");

                return hitMeshes;
            };

        vector<Viewport*> hitViewports = get_hit_viewports();
        if (hitViewports.empty())
        {
            lastViewportID = 0;
            return;
        }

        Viewport* hitVP = hitViewports.back();
        viewportID = hitVP->ID;

        hitVP->hitTestID = ID;

        if (hitVP->hoverCallback) hitVP->hoverCallback();
        if (lastViewportID != hitVP->ID
            && hitVP->onHoverStartCallback)
        {
            hitVP->onHoverStartCallback();
        }

        if (lastViewportID != 0
            && lastViewportID != viewportID)
        {
            Viewport* lastVP{};
            string err = Viewport::GetRegistry().GetContent(lastViewportID, lastVP);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics hit test error", 
                    "Failed to update hit test '" + to_string(ID) 
                    + "' because its last viewport was invalid! Reason: " + err);
            }

            if (lastVP->onHoverExitCallback) lastVP->onHoverExitCallback();
        }

        lastViewportID = hitVP->GetID();

        vector<Mesh*> hit2DMeshes = get_2d_hit_meshes(hitVP);
        if (hit2DMeshes.empty())
        {
            if (lastMesh2DID != 0)
            {
                Mesh* lastMesh{};
                string err = Mesh::GetRegistry().GetContent(lastMesh2DID, lastMesh);
                if (!err.empty())
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics hit test error", 
                        "Failed to update hit test '" + to_string(ID) 
                        + "' because its last 2D mesh was invalid! Reason: " + err);
                }

                if (lastMesh->onHoverExitCallback) lastMesh->onHoverExitCallback();
            }

            lastMesh2DID = 0;
        }
        else
        {
            Mesh* hitMesh = hit2DMeshes.back();
            mesh2DID = hitMesh->ID;
            hitMesh->hitTestID = ID;

            if (hitMesh->hoverCallback) hitMesh->hoverCallback();
            if (lastMesh2DID != hitMesh->GetID()
                && hitMesh->onHoverStartCallback)
            {
                hitMesh->onHoverStartCallback();
            }

            if (lastMesh2DID != 0
                && lastMesh2DID != mesh2DID)
            {
                Mesh* lastMesh{};
                string err = Mesh::GetRegistry().GetContent(lastMesh2DID, lastMesh);
                if (!err.empty())
                {
                    KalaGraphicsCore::ForceClose(
                        "KalaGraphics hit test error", 
                        "Failed to update hit test '" + to_string(ID) 
                        + "' because its last 2D mesh was invalid! Reason: " + err);
                }

                if (lastMesh->onHoverExitCallback) lastMesh->onHoverExitCallback();
            }

            lastMesh2DID = hitMesh->GetID();
        }
    }

    void HitTest::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics hit test error",
                "Failed to destroy hit test '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    HitTest::~HitTest()
    {
		Log::Print(
			"Destroying hit test '" + to_string(ID) + "'.",
			"KG_HIT_TEST",
			LogType::LOG_INFO);
    }
}