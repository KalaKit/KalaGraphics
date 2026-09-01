//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/kg_hit_test.hpp"

#if defined(KWIN_ANY)
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif

#include <memory>

#include "log_utils.hpp"

#include "core/kg_context.hpp"
#include "core/kg_viewport.hpp"
#include "resources/kg_mesh.hpp"

using KalaHeaders::KalaCore::ToVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::RotTarget;

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
    u32 HitTest::GetMeshID() const { return meshID; }

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
                if (meshID != 0)
                {
                    Mesh* m{};
                    string err = Mesh::GetRegistry().GetContent(meshID, m);
                    if (err.empty()) m->hitTestID = 0;

                    meshID = 0;
                }
            };

        auto get_mouse_pos = [&gctx]() -> vec2
            {
#if defined(KWIN_ANY)
                POINT pt{};
                if (!GetCursorPos(&pt)) return -1;

                HWND hwnd = ToVar<HWND>(gctx->contextData.context_window);
                if (WindowFromPoint(pt) != hwnd) return -1;

                ScreenToClient(hwnd, &pt);
                return { scast<f32>(pt.x), scast<f32>(pt.y) };
#else
                Display* display = ToVar<Display*>(gctx->contextData.context_display);
                Window window = ToVar<Window>(gctx->contextData.context_window);

                Window xquery_root{}, xquery_child{};
                int xquery_x{}, xquery_y{}, xquery_width{}, xquery_height{};
                unsigned int xquery_mask_return{};

                if (!XQueryPointer(
                    display, 
                    window, 
                    &xquery_root, 
                    &xquery_child,
                    &xquery_x,
                    &xquery_y,
                    &xquery_width,
                    &xquery_height,
                    &xquery_mask_return))
                {
                    return -1;
                }

                return { scast<f32>(xquery_width), scast<f32>(xquery_height) };
#endif
            };

        vec2 mousePos = get_mouse_pos();

        //Log::Print("@@@@@ mouse pos: " + to_string(mousePos.x) + "x" + to_string(mousePos.y));

        clear_old_data();

        if (mousePos < 0) return;

        auto hit_viewport = [&mousePos](vec2 pos, vec2 size) -> bool
            {
                return 
                    mousePos.x >= pos.x 
                    && mousePos.x < pos.x + size.x
                    && mousePos.y >= pos.y
                    && mousePos.y < pos.y + size.y;
            };

        auto hit_2d_rect = [&mousePos](
            vec2 pos,
            vec2 size,
            Mesh* m) -> bool
            {
                //move mouse into rectangle-local space
                vec2 local = mousePos - pos;

                f32 rotation = m->GetTransform().getroteuler(RotTarget::ROT_WORLD).z;

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

        vector<u32> existingViewports{};
        existingViewports.push_back(gctx->rootViewportID);
        for (u32 vpID : gctx->extraViewportIDs)
        {
            existingViewports.push_back(vpID);
        }
        
        vector<Viewport*> collidedViewports{};
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
                collidedViewports.push_back(vp);
            }
        }

        if (collidedViewports.empty()) return;

        viewportID = collidedViewports.back()->ID;
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