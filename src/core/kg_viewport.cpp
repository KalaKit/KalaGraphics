//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <unordered_map>

#include "vulkan/vulkan_core.h"

#include "log_utils.hpp"

#include "core/kg_viewport.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"
#include "core/kg_hit_test.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_camera.hpp"

using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec2;
using KalaHeaders::KalaMath::isnear;

using KalaGraphics::Core::ViewportStaticSize;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::Viewport;
using KalaGraphics::Resources::Shader;
using KalaGraphics::Resources::Camera;

using std::string_view;
using std::to_string;
using std::unordered_map;
using std::vector;
using std::min;

//
// 4:3
//

static constexpr string_view vp_640_480   = "640x480";
static constexpr string_view vp_800_600   = "800x600";
static constexpr string_view vp_1024_768  = "1024x768";
static constexpr string_view vp_1600_1200 = "1600x1200";

//
// 16:9
//

static constexpr string_view vp_1280_720 = "1280x720";
static constexpr string_view vp_1600_900 = "1600x900";
static constexpr string_view vp_1920_1080 = "1920x1080";
static constexpr string_view vp_2560_1440 = "2560x1440";
static constexpr string_view vp_3840_2160 = "3840x2160";

//16:10

static constexpr string_view vp_1280_800 = "640x480";
static constexpr string_view vp_1680_1050 = "640x480";
static constexpr string_view vp_1920_1200 = "640x480";
static constexpr string_view vp_2560_1600 = "2560x1600";

//21:9

static constexpr string_view vp_2560_1080 = "2560x1080";
static constexpr string_view vp_3440_1440 = "3440x1440";
static constexpr string_view vp_5120_2160 = "5120x2160";

//32:9

static constexpr string_view vp_3840_1080 = "3840x1080";
static constexpr string_view vp_5120_1440 = "5120x1440";

static unordered_map<ViewportStaticSize, string_view, EnumHash<ViewportStaticSize>> vpNames =
{
    { ViewportStaticSize::VP_640_480,   vp_640_480 },
    { ViewportStaticSize::VP_800_600,   vp_800_600 },
    { ViewportStaticSize::VP_1024_768,  vp_1024_768 },
    { ViewportStaticSize::VP_1600_1200, vp_1600_1200 },

    { ViewportStaticSize::VP_1280_720,  vp_1280_720 },
    { ViewportStaticSize::VP_1600_900,  vp_1600_900 },
    { ViewportStaticSize::VP_1920_1080, vp_1920_1080 },
    { ViewportStaticSize::VP_2560_1440, vp_2560_1440 },
    { ViewportStaticSize::VP_3840_2160, vp_3840_2160 },

    { ViewportStaticSize::VP_1280_800,  vp_1280_800 },
    { ViewportStaticSize::VP_1680_1050, vp_1680_1050 },
    { ViewportStaticSize::VP_1920_1200, vp_1920_1200 },
    { ViewportStaticSize::VP_2560_1600, vp_2560_1600 },

    { ViewportStaticSize::VP_2560_1080, vp_2560_1080 },
    { ViewportStaticSize::VP_3440_1440, vp_3440_1440 },
    { ViewportStaticSize::VP_5120_2160, vp_5120_2160 },

    { ViewportStaticSize::VP_3840_1080, vp_3840_1080 },
    { ViewportStaticSize::VP_5120_1440, vp_5120_1440 }
};

static unordered_map<ViewportStaticSize, vec2, EnumHash<ViewportStaticSize>> vpSizes =
{
    { ViewportStaticSize::VP_640_480,   vec2(640, 480) },
    { ViewportStaticSize::VP_800_600,   vec2(800, 600) },
    { ViewportStaticSize::VP_1024_768,  vec2(1024, 768) },
    { ViewportStaticSize::VP_1600_1200, vec2(1600, 1200) },

    { ViewportStaticSize::VP_1280_720,  vec2(1280, 720) },
    { ViewportStaticSize::VP_1600_900,  vec2(1600, 900) },
    { ViewportStaticSize::VP_1920_1080, vec2(1920, 1080) },
    { ViewportStaticSize::VP_2560_1440, vec2(2560, 1440) },
    { ViewportStaticSize::VP_3840_2160, vec2(3840, 2160) },

    { ViewportStaticSize::VP_1280_800,  vec2(1280, 800) },
    { ViewportStaticSize::VP_1680_1050, vec2(1680, 1050) },
    { ViewportStaticSize::VP_1920_1200, vec2(1920, 1200) },
    { ViewportStaticSize::VP_2560_1600, vec2(2560, 1600) },

    { ViewportStaticSize::VP_2560_1080, vec2(2560, 1080) },
    { ViewportStaticSize::VP_3440_1440, vec2(3440, 1440) },
    { ViewportStaticSize::VP_5120_2160, vec2(5120, 2160) },

    { ViewportStaticSize::VP_3840_1080, vec2(3840, 1080) },
    { ViewportStaticSize::VP_5120_1440, vec2(5120, 1440) }
};

namespace KalaGraphics::Core
{
    static KalaGraphicsRegistry<Viewport> registry{};

    KalaGraphicsRegistry<Viewport>& Viewport::GetRegistry() { return registry; }

    string_view Viewport::GetStaticName(ViewportStaticSize vpSize)
    {   
        string_view out{};
        string err = EnumToString(vpSize, vpNames, out);
        if (!err.empty())
        {
            Log::Print(
                "Failed to get viewport name! Reason: " + err, 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return out;
    }
    vec2 Viewport::GetStaticValue(ViewportStaticSize vpSize)
    {
		auto it = vpSizes.find(vpSize);
		if (it == vpSizes.end())
        {
            Log::Print(
                "Failed to get viewport value because the passed enum was invalid!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

		return it->second;
    }

    Viewport* Viewport::Initialize(
        u32 contextID,
        ViewportType type,
        u32 targetViewport)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize viewport because its graphics context was invalid! Reason: " + err,
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Viewport> newVP = make_unique<Viewport>();
        Viewport* vpPtr = newVP.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        vpPtr->ID = newID;

        vpPtr->viewportType = type;
        vpPtr->targetViewportID = targetViewport;

        vpPtr->contextID = contextID;

        if (gctx->rootViewportID == 0)
        {
            gctx->rootViewportID = newID;
            vpPtr->isRootViewport = true;
        }
        else gctx->extraViewportIDs.push_back(newID);

        err = registry.AddContent(newID, std::move(newVP));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics viewport error",
				"Failed to initialize viewport! Reason: " + err);
        }

        Log::Print(
            "Created new viewport '" + to_string(newID) + "'!",
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);

        return vpPtr;
    }

    u32 Viewport::GetID() const { return ID; }
    u32 Viewport::GetContextID() const { return contextID; }

    u32 Viewport::GetTargetViewportID() const { return targetViewportID; }

    u32 Viewport::GetPrimary3DCameraID() const { return primary3DCameraID; }
    u32 Viewport::GetPrimary2DCameraID() const { return primary2DCameraID; }

    const vector<u32>& Viewport::GetExtra3DCameraIDs() const { return extra3DCameraIDs; }
    const vector<u32>& Viewport::GetExtra2DCameraIDs() const { return extra2DCameraIDs; }

    u32 Viewport::GetPrimary3DShaderID() const { return primary3DShaderID; }
    u32 Viewport::GetPrimary2DShaderID() const { return primary2DShaderID; }

    const vector<u32>& Viewport::GetExtra3DShaderIDs() const { return extra3DShaderIDs; }
    const vector<u32>& Viewport::GetExtra2DShaderIDs() const { return extra2DShaderIDs; }

    bool Viewport::IsHovered() const { return hitTestID != 0; }

    bool Viewport::IsVisible() const { return isVisible; }
    void Viewport::SetVisibleState(bool newValue)
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' visible state because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        isVisible = newValue;

        string val = isVisible ? "true" : "false";

        Log::Print(
            "Set viewport '" + to_string(ID) + "' "
            "visible state to " + val + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    bool Viewport::IsRootViewport() const { return isRootViewport; }

    bool Viewport::IsOffscreenViewport() const { return isOffscreenViewport; }

    bool Viewport::IsDynamicResizeEnabled() const { return isDynamicResizeEnabled; }
    void Viewport::SetDynamicResizeState(bool newValue)
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' dynamic resize state because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        isDynamicResizeEnabled = newValue;

        string val = isDynamicResizeEnabled ? "true" : "false";

        Log::Print(
            "Set viewport '" + to_string(contextID) + "' "
            "dynamic resize state to " + val + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    ViewportType Viewport::GetType() const { return viewportType; }
    void Viewport::SetType(ViewportType newType)
    {
        string typeStr{};

        switch (newType)
        {
        case ViewportType::VP_FILL:
            typeStr = "fill";
            viewportType = ViewportType::VP_FILL;
            break;
        case ViewportType::VP_FIT:
            typeStr = "fit";
            viewportType = ViewportType::VP_FIT;
            break;
        case ViewportType::VP_CENTER:
            typeStr = "center";
            viewportType = ViewportType::VP_CENTER;
            break;
        case ViewportType::VP_CUSTOM:
            typeStr = "custom";
            viewportType = ViewportType::VP_CUSTOM;
            break;
        };

        UpdateViewportSize();

        Log::Print(
            "Set viewport '" + to_string(ID) + "' type to '" + typeStr + "'!",
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    u8 Viewport::GetDrawOrderIndex() const { return drawOrderIndex; }
    void Viewport::SetDrawOrderIndex(
        u8 newValue,
        bool sortNow)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' draw order index because its graphics context was invalid! Reason: " + err);
        }

        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' draw order index because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        else if (!isRootViewport
                 && newValue == 0)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' draw order index because 0 can only be applied to root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        drawOrderIndex = newValue;

        if (!sortNow) gctx->isViewportSortDirty = true;
        else gctx->SortViewports();

        Log::Print(
            "Set viewport '" + to_string(ID) + "' draw order index to '" + to_string(drawOrderIndex) + "'.",
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    const vec4& Viewport::GetBackgroundColor() const { return viewportBackgroundColor; }
    void Viewport::SetBackgroundColor(vec4&& newColor)
    {
        viewportBackgroundColor = std::move(kclamp(newColor, 0, 1));

        Log::Print(
            "Set background color to '" 
            + to_string(viewportBackgroundColor.x) + ", "
            + to_string(viewportBackgroundColor.y) + ", "
            + to_string(viewportBackgroundColor.z) + ", "
            + to_string(viewportBackgroundColor.w) + "'!",
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    const vec4& Viewport::GetLetterboxColor() const { return viewportLetterboxColor; }
    void Viewport::SetLetterboxColor(vec4&& newColor)
    {
        viewportLetterboxColor = std::move(kclamp(newColor, 0, 1));

        Log::Print(
            "Set letterbox color to '" 
            + to_string(viewportLetterboxColor.x) + ", "
            + to_string(viewportLetterboxColor.y) + ", "
            + to_string(viewportLetterboxColor.z) + ", "
            + to_string(viewportLetterboxColor.w) + "'!",
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetAnchorPosition(ViewportAnchorPosition pos) const 
    { 
        switch (pos)
        {
        default:
        case ViewportAnchorPosition::P_DEFAULT:
            return 0;
        case ViewportAnchorPosition::P_BOTTOM_LEFT:
            return posBottomLeft;
        case ViewportAnchorPosition::P_BOTTOM_RIGHT:
            return posBottomRight;
        case ViewportAnchorPosition::P_TOP_LEFT:
            return posTopLeft;
        case ViewportAnchorPosition::P_TOP_RIGHT:
            return posTopRight;
        case ViewportAnchorPosition::P_CENTER:
            return posCenter;
        }
    }

    vec2 Viewport::GetSize() const { return viewportDynamicSize; }
    void Viewport::SetSize(ViewportStaticSize vpSize)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' static size because its graphics context was invalid! Reason: " + err);
        }

        vec2 realSize = GetStaticValue(vpSize);
        if (realSize.x + viewportOffset.x > gctx->renderSize.x
            || realSize.y + viewportOffset.y > gctx->renderSize.y)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' static size because it is bigger than the window size!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        viewportStaticSize = vpSize;

        UpdateViewportSize();

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' static size to '" + string(GetStaticName(viewportStaticSize)) + "'!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }
    void Viewport::SetSize(vec2 newValue)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' dynamic size because its graphics context was invalid! Reason: " + err);
        }

        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' dynamic size because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (isOffscreenViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' dynamic size because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < 0
            || newValue.y < 0)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it was below 0!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        if (newValue.x + viewportOffset.x > gctx->renderSize.x
            || newValue.y + viewportOffset.y > gctx->renderSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it extended outside of render area!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        viewportDynamicSize = kclamp(
            newValue,
            0,
            gctx->renderSize - viewportOffset);

        if (viewportType == ViewportType::VP_FILL)
        {
            scissorSize = viewportDynamicSize;
        }

        UpdateViewportSize();

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' dynamic size to " 
            + to_string(viewportDynamicSize.x) + "x" 
            + to_string(viewportDynamicSize.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetOffset() const { return viewportOffset; }
    void Viewport::SetOffset(vec2 newValue)
    {
        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' offset because its graphics context was invalid! Reason: " + err);
        }

        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' offset because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (isOffscreenViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' offset because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < 0.0f
            || newValue.y < 0.0f)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it was below 0!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        if (newValue.x + viewportDynamicSize.x > gctx->renderSize.x
            || newValue.y + viewportDynamicSize.y > gctx->renderSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' offset because it was extended outside of render area!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        viewportOffset = kclamp(
            newValue,
            0,
            gctx->renderSize - viewportDynamicSize);

        UpdateViewportSize();

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' offset to " 
            + to_string(viewportOffset.x) + "x" 
            + to_string(viewportOffset.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetScissorSize() const { return scissorSize; }
    void Viewport::SetScissorSize(vec2 newValue)
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor size because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (isOffscreenViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor size because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (viewportType != ViewportType::VP_CUSTOM)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor size because it is not a custom type!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < 0
            || newValue.y < 0)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor size because it was below 0!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        if (scissorOffset.x + newValue.x > viewportOffset.x + viewportDynamicSize.x
            || scissorOffset.y + newValue.y > viewportOffset.y + viewportDynamicSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor size because it extended outside of viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }
        
        scissorSize = kclamp(
            newValue, 
            0, 
            (viewportOffset + viewportDynamicSize) - scissorOffset);

        UpdateViewportSize();

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' scissor size to " 
            + to_string(scissorSize.x) + "x" 
            + to_string(scissorSize.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetScissorOffset() const { return scissorOffset; }
    void Viewport::SetScissorOffset(vec2 newValue)
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it is a root viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (isOffscreenViewport)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it is an offscreen viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (viewportType != ViewportType::VP_CUSTOM)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it is not a custom type!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x < viewportOffset.x
            || newValue.y < viewportOffset.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor offset because it was less than viewport offset!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }
        if (newValue.x + scissorSize.x > viewportOffset.x + viewportDynamicSize.x
            || newValue.y + scissorSize.y > viewportOffset.y + viewportDynamicSize.y)
        {
            Log::Print(
                "Clamped viewport '" + to_string(ID) 
                + "' scissor offset because it extended outside of viewport!", 
                "KG_VIEWPORT",
                LogType::LOG_WARNING);
        }

        scissorOffset = kclamp(
            newValue, 
            viewportOffset, 
            (viewportOffset + viewportDynamicSize) - scissorSize);

        UpdateViewportSize();

        Log::Print(
            "Set viewport '" + to_string(ID) 
            + "' scissor offset to " 
            + to_string(scissorOffset.x) + "x" 
            + to_string(scissorOffset.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    void Viewport::SetHoverCallback(function<void()>&& newValue)
    { 
        hoverCallback = std::move(newValue);
    }
    void Viewport::SetOnHoverStartCallback(function<void()>&& newValue)
    { 
        onHoverStartCallback = std::move(newValue);
    }
    void Viewport::SetOnHoverExitCallback(function<void()>&& newValue)
    { 
        onHoverExitCallback = std::move(newValue);
    }

    void Viewport::SetKeyHeldCallback(
        KeyboardButton btn, 
        function<void()>&& newValue)
    {
        keyHeldCallbacks[btn] = std::move(newValue);
    }
    void Viewport::SetKeyPressedCallback(
        KeyboardButton btn, 
        function<void()>&& newValue)
    {
        keyPressedCallbacks[btn] = std::move(newValue);
    }
    void Viewport::SetKeyReleasedCallback(
        KeyboardButton btn, 
        function<void()>&& newValue)
    {
        keyReleasedCallbacks[btn] = std::move(newValue);
    }

    void Viewport::SetMouseButtonHeldCallback(
        MouseButton btn, 
        function<void()>&& newValue)
    {
        mouseButtonHeldCallbacks[btn] = std::move(newValue);
    }
    void Viewport::SetMouseButtonPressedCallback(
        MouseButton btn, 
        function<void()>&& newValue)
    {
        mouseButtonPressedCallbacks[btn] = std::move(newValue);
    }
    void Viewport::SetMouseButtonReleasedCallback(
        MouseButton btn, 
        function<void()>&& newValue)
    {
        mouseButtonReleasedCallbacks[btn] = std::move(newValue);
    }
    void Viewport::SetMouseButtonDoubleClickedCallback(
        MouseButton btn, 
        function<void()>&& newValue)
    {
        mouseButtonDoubleClickedCallbacks[btn] = std::move(newValue);
    }
    void Viewport::SetMouseButtonDraggingCallback(
        MouseButton btn, 
        function<void(vec2)>&& newValue)
    {
        mouseButtonDraggingCallbacks[btn] = std::move(newValue);
    }

    void Viewport::SetScrollUpCallback(function<void(f32)>&& newValue)
    { 
        scrollUpCallback = std::move(newValue);
    }
    void Viewport::SetScrollDownCallback(function<void(f32)>&& newValue)
    { 
        scrollDownCallback = std::move(newValue);
    }

    void Viewport::Update(u32 imageIndex)
    {
        //don't draw hidden viewports
        if (!isVisible) return;

        GraphicsContext* gctx{};
        (void)GraphicsContext::GetRegistry().GetContent(contextID, gctx);

        if (IsHovered())
        {
            //keyboard button callbacks

            for (KeyboardButton key : gctx->GetHeldKeys())
            {
                auto it = keyHeldCallbacks.find(key);
                if (it != keyHeldCallbacks.end()
                    && it->second)
                {
                    it->second();
                }
            }
            for (KeyboardButton key : gctx->GetPressedKeys())
            {
                auto it = keyPressedCallbacks.find(key);
                if (it != keyPressedCallbacks.end()
                    && it->second)
                {
                    it->second();
                }
            }
            for (KeyboardButton key : gctx->GetReleasedKeys())
            {
                auto it = keyReleasedCallbacks.find(key);
                if (it != keyReleasedCallbacks.end()
                    && it->second)
                {
                    it->second();
                }
            }

            //mouse button callbacks

            for (MouseButton mb : gctx->GetHeldMouseButtons())
            {
                auto it = mouseButtonHeldCallbacks.find(mb);
                if (it != mouseButtonHeldCallbacks.end()
                    && it->second)
                {
                    it->second();
                }
            }
            for (MouseButton mb : gctx->GetPressedMouseButtons())
            {
                auto it = mouseButtonPressedCallbacks.find(mb);
                if (it != mouseButtonPressedCallbacks.end()
                    && it->second)
                {
                    it->second();
                }
            }
            for (MouseButton mb : gctx->GetReleasedMouseButtons())
            {
                auto it = mouseButtonReleasedCallbacks.find(mb);
                if (it != mouseButtonReleasedCallbacks.end()
                    && it->second)
                {
                    it->second();
                }
            }
            for (MouseButton mb : gctx->GetDoubleClickedMouseButtons())
            {
                auto it = mouseButtonDoubleClickedCallbacks.find(mb);
                if (it != mouseButtonDoubleClickedCallbacks.end()
                    && it->second)
                {
                    it->second();
                }
            }

            for (MouseButton mb : gctx->GetDraggingMouseButtons())
            {
                auto it = mouseButtonDraggingCallbacks.find(mb);
                if (it != mouseButtonDraggingCallbacks.end()
                    && it->second)
                {
                    it->second(gctx->mousePos);
                }
            }

            //scrollwheel callbacks

            f32 scrollWheelDelta = gctx->GetScrollWheelDelta();

            if (scrollWheelDelta > 0
                && scrollUpCallback)
            {
                scrollUpCallback(scrollWheelDelta);
            }
            if (scrollWheelDelta < 0
                && scrollDownCallback)
            {
                scrollDownCallback(scrollWheelDelta);
            }
        }

        //
        // BLACK BORDERS FOR STATIC MODE
        //

        if (viewportType != ViewportType::VP_FILL)
        {
            /*
            Log::Print(
                "@@@@@\n"
                "STATIC VIEWPORT DEBUG\n"
                "viewportDynamicSize: " + to_string(viewportDynamicSize.x) + "x" + to_string(viewportDynamicSize.y) + "\n"
                "viewportOffset: " + to_string(viewportOffset.x) + "x" + to_string(viewportOffset.y) + "\n"
                "scissorSize: " + to_string(scissorSize.x) + "x" + to_string(scissorSize.y) + "\n"
                "scissorOffset: " + to_string(scissorOffset.x) + "x" + to_string(scissorOffset.y) + "\n"
                "renderSize: " + to_string(gctx->renderSize.x) + "x" + to_string(gctx->renderSize.y));
            */

            VkRenderingAttachmentInfo barClear{};
            barClear.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            barClear.imageView = gctx->swapchainImageViews[imageIndex];
            barClear.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barClear.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            barClear.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            barClear.clearValue.color = 
            { 
                { 
                    viewportLetterboxColor.x, 
                    viewportLetterboxColor.y, 
                    viewportLetterboxColor.z, 
                    viewportLetterboxColor.w 
                } 
            };

            vec2 barSize = 
            {
                min(viewportDynamicSize.x,
                    gctx->renderSize.x - viewportOffset.x),
                min(viewportDynamicSize.y, 
                    gctx->renderSize.y - viewportOffset.y)
            };
            vec2 barOffset =
            {
                min(viewportOffset.x,
                    gctx->renderSize.x),
                min(viewportOffset.y,
                    gctx->renderSize.y)
            };

            VkRenderingInfo barInfo{};
            barInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            barInfo.renderArea.offset = 
            {
                scast<i32>(barOffset.x),
                scast<i32>(barOffset.y)
            };
            barInfo.renderArea.extent = 
            {
                scast<u32>(barSize.x),
                scast<u32>(barSize.y)
            };
            barInfo.layerCount = 1;
            barInfo.colorAttachmentCount = 1;
            barInfo.pColorAttachments = &barClear;
            barInfo.pDepthAttachment = nullptr;

            vkCmdBeginRendering(
                gctx->commandBuffers[gctx->currentFrame],
                &barInfo);
            vkCmdEndRendering(gctx->commandBuffers[gctx->currentFrame]);
        }

        //
        // RESOLVE DRAW AREA
        //

        vec2 drawSize{};
        vec2 drawOffset{};

        if (viewportType == ViewportType::VP_FILL)
        {
            drawSize = viewportDynamicSize;
            drawOffset = viewportOffset;
        }
        else if (viewportType == ViewportType::VP_CUSTOM)
        {
            drawSize = scissorSize;
            drawOffset = scissorOffset;
        }
        else
        {
            drawSize = scissorSize;
            drawOffset = vec2{ 
                viewportOffset.x + scissorOffset.x,
                viewportOffset.y + scissorOffset.y };
        }

        drawSize.x = min(
            drawSize.x,
            gctx->renderSize.x - drawOffset.x);
        drawSize.y = min(
            drawSize.y,
            gctx->renderSize.y - drawOffset.y);
        drawOffset.x = min(
            drawOffset.x,
            gctx->renderSize.x);
        drawOffset.y = min(
            drawOffset.y,
            gctx->renderSize.y);

        //
        // ACTUAL DRAW FUNCTION
        //

        auto draw = [&](bool is2D) -> void
            {
                VkViewport viewport{};
                viewport.width = viewportDynamicSize.x;
                viewport.height = viewportDynamicSize.y;

                viewport.x = viewportOffset.x;
                viewport.y = viewportOffset.y;

                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;

                VkRect2D scissor{};
                scissor.extent = 
                { 
                    scast<u32>(drawSize.x), 
                    scast<u32>(drawSize.y) 
                };
                scissor.offset = 
                { 
                    scast<int>(drawOffset.x), 
                    scast<int>(drawOffset.y) 
                };

                /*
                Log::Print(
                    "@@@@@\n"
                    "viewport ID: " + to_string(ID) + "\n"
                    "viewport size: " + to_string(viewport.width) + "x" + to_string(viewport.height) + "\n"
                    "viewport offset: " + to_string(viewport.x) + "x" + to_string(viewport.y) + "\n"
                    "scissor size: " + to_string(scissor.extent.width) + "x" + to_string(scissor.extent.height) + "\n"
                    "scissor offset: " + to_string(scissor.offset.x) + "x" + to_string(scissor.offset.y) + "\n");
                */

                vkCmdSetViewport(
                    gctx->commandBuffers[gctx->currentFrame],
                    0,
                    1,
                    &viewport);
                    
                vkCmdSetScissor(
                    gctx->commandBuffers[gctx->currentFrame],
                    0,
                    1,
                    &scissor);

                if (!is2D)
                {
                    Shader* primary3DShader{};
                    string err = Shader::GetRegistry().GetContent(primary3DShaderID, primary3DShader);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics context error",
                            "Failed to update viewport '" + to_string(ID) 
                            + "' because the primary 3D shader was invalid! Reason: " + err);
                    }

                    primary3DShader->Update(gctx->commandBuffers[gctx->currentFrame]);

                    for (u32 extra3DShaderID : extra3DShaderIDs)
                    {
                        Shader* extra3DShader{};
                        err = Shader::GetRegistry().GetContent(extra3DShaderID, extra3DShader);
                        if (!err.empty())
                        {
                            KalaGraphicsCore::ForceClose(
                                "KalaGraphics context error",
                                "Failed to update viewport '" + to_string(ID) 
                                + "' because extra 3D shader was invalid! Reason: " + err);
                        }

                        extra3DShader->Update(gctx->commandBuffers[gctx->currentFrame]);
                    }
                }
                else
                {
                    Shader* primary2DShader{};
                    string err = Shader::GetRegistry().GetContent(primary2DShaderID, primary2DShader);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics context error",
                            "Failed to update viewport '" + to_string(ID) 
                            + "' because the primary 2D shader was invalid! Reason: " + err);
                    }

                    primary2DShader->Update(gctx->commandBuffers[gctx->currentFrame]);

                    for (u32 extra2DShaderID : extra2DShaderIDs)
                    {
                        Shader* extra2DShader{};
                        err = Shader::GetRegistry().GetContent(extra2DShaderID, extra2DShader);
                        if (!err.empty())
                        {
                            KalaGraphicsCore::ForceClose(
                                "KalaGraphics context error",
                                "Failed to update viewport '" + to_string(ID) 
                                + "' because extra 2D shader was invalid! Reason: " + err);
                        }

                        extra2DShader->Update(gctx->commandBuffers[gctx->currentFrame]);
                    }
                }
            };

        //
        // 3D STAGE
        //

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = gctx->swapchainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = 
        { 
            { 
                viewportBackgroundColor.x, 
                viewportBackgroundColor.y, 
                viewportBackgroundColor.z, 
                viewportBackgroundColor.w
            } 
        };

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = gctx->depthImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = 
        { 
            scast<i32>(drawOffset.x), 
            scast<i32>(drawOffset.y) 
        };
        renderingInfo.renderArea.extent = 
        { 
            scast<u32>(drawSize.x), 
            scast<u32>(drawSize.y) 
        };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(
            gctx->commandBuffers[gctx->currentFrame],
            &renderingInfo);

        draw(false);

        vkCmdEndRendering(gctx->commandBuffers[gctx->currentFrame]);

        //
        // 2D STAGE
        //

        VkRenderingAttachmentInfo colorAttachment2D{};
        colorAttachment2D.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment2D.imageView = gctx->swapchainImageViews[imageIndex];
        colorAttachment2D.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment2D.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment2D.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderingInfo2D{};
        renderingInfo2D.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo2D.renderArea.offset = 
        { 
            scast<i32>(drawOffset.x), 
            scast<i32>(drawOffset.y) 
        };
        renderingInfo2D.renderArea.extent = 
        { 
            scast<u32>(drawSize.x), 
            scast<u32>(drawSize.y) 
        };
        renderingInfo2D.layerCount = 1;
        renderingInfo2D.colorAttachmentCount = 1;
        renderingInfo2D.pColorAttachments = &colorAttachment2D;
        renderingInfo2D.pDepthAttachment = nullptr;

        vkCmdBeginRendering(
            gctx->commandBuffers[gctx->currentFrame],
            &renderingInfo2D);

        draw(true);

        vkCmdEndRendering(gctx->commandBuffers[gctx->currentFrame]);
    }

    void Viewport::UpdateViewportSize()
    {
        if (!isVisible) return;

        GraphicsContext* gctx{};
        string err = GraphicsContext::GetRegistry().GetContent(contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to resize viewport '" + to_string(ID) 
                + "' because its graphics context was invalid! Reason: " + err);
        }

        if (isDynamicResizeEnabled)
        {
            vec2 scaleFactor =
            {
                gctx->renderSize.x / gctx->oldRenderSize.x,
                gctx->renderSize.y / gctx->oldRenderSize.y
            };

            viewportDynamicSize.x *= scaleFactor.x;
            viewportDynamicSize.y *= scaleFactor.y;

            viewportOffset.x *= scaleFactor.x;
            viewportOffset.y *= scaleFactor.y;
        }

        viewportDynamicSize.x = min(
            viewportDynamicSize.x,
            gctx->renderSize.x 
            - viewportOffset.x);

        viewportDynamicSize.y = min(
            viewportDynamicSize.y,
            gctx->renderSize.y 
            - viewportOffset.y);

        viewportOffset = kclamp(
            viewportOffset, 
            0.0f, 
            gctx->renderSize);
            
        vec2 viewportRealStaticSize = GetStaticValue(viewportStaticSize);

        if (viewportType == ViewportType::VP_FIT
            || viewportType == ViewportType::VP_CENTER)
        {
            const f32 targetAspect = 
                scast<f32>(viewportRealStaticSize.x) /
                scast<f32>(viewportRealStaticSize.y);

            const f32 viewportAspect = 
                viewportDynamicSize.x /
                viewportDynamicSize.y;

            vec2 fittedSize{};

            //viewport is wider than target aspect - pillarbox
            if (viewportAspect > targetAspect)
            {
                fittedSize.y = viewportDynamicSize.y;
                fittedSize.x = floorf(viewportDynamicSize.y * targetAspect);
            }
            //viewport is taller than target aspect - letterbox
            else
            {
                fittedSize.x = viewportDynamicSize.x;
                fittedSize.y = floorf(viewportDynamicSize.x / targetAspect);
            }

            if (viewportType == ViewportType::VP_CENTER)
            {
                //clamp to max static size
                fittedSize.x = min(
                    fittedSize.x, 
                    viewportRealStaticSize.x);
                fittedSize.y = min(
                    fittedSize.y,
                    viewportRealStaticSize.y);
            }

            //center within the current viewports coordinate space
            scissorSize = fittedSize;
            scissorOffset = 
            {
                floorf((viewportDynamicSize.x - fittedSize.x) * 0.5f),
                floorf((viewportDynamicSize.y - fittedSize.y) * 0.5f)
            };
        }
        else if (viewportType == ViewportType::VP_FILL)
        {
            scissorSize = viewportDynamicSize;
            scissorOffset = {};
        }

        posTopLeft =
        {
            posBottomLeft.x,
            posBottomLeft.y + scissorSize.y
        };
        posTopRight =
        {
            posBottomLeft.x + scissorSize.x,
            posBottomLeft.y + scissorSize.y
        };
        posBottomLeft =
        {
            viewportOffset.x + scissorOffset.x,
            gctx->renderSize.y
                - viewportOffset.y
                - scissorOffset.y
                - scissorSize.y
        };
        posBottomRight =
        {
            posBottomLeft.x + scissorSize.x,
            posBottomLeft.y
        };
        posCenter =
        {
            posBottomLeft.x + scissorSize.x * 0.5f,
            posBottomLeft.y + scissorSize.y * 0.5f
        };

        if (isnear(posTopLeft.x)) posTopLeft.x = 0;
        if (isnear(posTopLeft.y)) posTopLeft.y = 0;

        if (isnear(posTopRight.x)) posTopRight.x = 0;
        if (isnear(posTopRight.y)) posTopRight.y = 0;

        if (isnear(posBottomLeft.x)) posBottomLeft.x = 0;
        if (isnear(posBottomLeft.y)) posBottomLeft.y = 0;

        if (isnear(posBottomRight.x)) posBottomRight.x = 0;
        if (isnear(posBottomRight.y)) posBottomRight.y = 0;

        if (isnear(posCenter.x)) posCenter.x = 0;
        if (isnear(posCenter.y)) posCenter.y = 0;

        /*
        Log::Print("@@@@@\n"
            "gctx '" + to_string(contextID) + "' render size: "
            + to_string(gctx->renderSize.x) + "x" 
            + to_string(gctx->renderSize.y)
            + "\nvp '" + to_string(ID) + "' dynamic size: "
            + to_string(viewportDynamicSize.x) + "x" 
            + to_string(viewportDynamicSize.y)
            + "\nvp static size: "
            + to_string(viewportRealStaticSize.x) + "x" 
            + to_string(viewportRealStaticSize.y)
            + "\nvp top left: " 
            + to_string(posTopLeft.x) + "x" 
            + to_string(posTopLeft.y)
            + "\nvp top right: " 
            + to_string(posTopRight.x) + "x" 
            + to_string(posTopRight.y)
            + "\nvp bottom left: " 
            + to_string(posBottomLeft.x) + "x" 
            + to_string(posBottomLeft.y)
            + "\nvp bottom right: " 
            + to_string(posBottomRight.x) + "x" 
            + to_string(posBottomRight.y)
            + "\nvp center: " 
            + to_string(posCenter.x) + "x" 
            + to_string(posCenter.y));
        */

        Camera* c3d{};
        err = Camera::GetRegistry().GetContent(primary3DCameraID, c3d);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to resize viewport '" + to_string(ID) 
                + "' because its primary 3D camera was invalid! Reason: " + err);
        }
        c3d->Move({}, {});

        for (u32 cID : extra3DCameraIDs)
        {
            Camera* ec3d{};
            err = Camera::GetRegistry().GetContent(cID, ec3d);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics viewport error",
                    "Failed to resize viewport '" + to_string(ID) 
                    + "' because its extra 3D camera was invalid! Reason: " + err);
            }
            ec3d->Move({}, {});
        }

        Camera* c2d{};
        err = Camera::GetRegistry().GetContent(primary2DCameraID, c2d);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to resize viewport '" + to_string(ID) 
                + "' because its primary 2D camera was invalid! Reason: " + err);
        }
        c2d->Move({}, {});

        for (u32 cID : extra2DCameraIDs)
        {
            Camera* ec2d{};
            err = Camera::GetRegistry().GetContent(cID, ec2d);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics viewport error",
                    "Failed to resize viewport '" + to_string(ID) 
                    + "' because its extra 2D camera was invalid! Reason: " + err);
            }
            ec2d->Move({}, {});
        }
    }

    void Viewport::Destroy()
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to destroy viewport '" + to_string(ID) 
                + "' because it is a root viewport of graphics context '" + to_string(contextID) + "'!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        _Destroy();
    }

    void Viewport::_Destroy()
    {
        if (isOffscreenViewport)
        {
            Viewport* target{};
            string err = registry.GetContent(targetViewportID, target);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics viewport error",
                    "Failed to destroy viewport '" + to_string(ID) 
                    + "' because its target viewport was invalid! Reason: " + err);
            }
        }

        HitTest* hitTest{};
        string err = HitTest::GetRegistry().GetContent(hitTestID, hitTest);
        if (err.empty()) hitTest->viewportID = 0;

        Shader* p3d{};
        err = Shader::GetRegistry().GetContent(primary3DShaderID, p3d);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to destroy viewport '" + to_string(ID) + "' because "
                "its primary 3D shader was invalid! Reason: " + err);
        }

        p3d->isDestroyingViewport = true;
        p3d->Destroy();

        Shader* p2d{};
        err = Shader::GetRegistry().GetContent(primary2DShaderID, p2d);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to destroy viewport '" + to_string(ID) + "' because "
                "its primary 2D shader was invalid! Reason: " + err);
        }

        p2d->isDestroyingViewport = true;
        p2d->Destroy();

        for (u32 sID : extra3DShaderIDs)
        {
            Shader* s{};
            err = Shader::GetRegistry().GetContent(sID, s);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 3D shader was invalid! Reason: " + err);
            }

            s->isDestroyingViewport = true;
            s->Destroy();
        }

        for (u32 sID : extra2DShaderIDs)
        {
            Shader* s{};
            err = Shader::GetRegistry().GetContent(sID, s);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 2D shader was invalid! Reason: " + err);
            }

            s->isDestroyingViewport = true;
            s->Destroy();
        }

        Camera* pc3{};
        err = Camera::GetRegistry().GetContent(primary3DCameraID, pc3);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to destroy viewport '" + to_string(ID) 
                + "' because its 3D camera was invalid! Reason: " + err);
        }

        pc3->Destroy();

        Camera* pc2{};
        err = Camera::GetRegistry().GetContent(primary2DCameraID, pc2);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to destroy viewport '" + to_string(ID) 
                + "' because its 2D camera was invalid! Reason: " + err);
        }

        pc2->Destroy();

        for (u32 cID : extra3DCameraIDs)
        {
            Camera* c{};
            err = Camera::GetRegistry().GetContent(cID, c);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 3D camera was invalid! Reason: " + err);
            }

            c->Destroy();
        }

        for (u32 cID : extra2DCameraIDs)
        {
            Camera* c{};
            err = Camera::GetRegistry().GetContent(cID, c);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its extra 2D camera was invalid! Reason: " + err);
            }

            c->Destroy();
        }

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to destroy viewport '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Viewport::~Viewport()
    {
		Log::Print(
			"Destroying viewport '" + to_string(ID) + "'.",
			"KG_VIEWPORT",
			LogType::LOG_INFO);
    }
}