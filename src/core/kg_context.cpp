//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include <string>
#include <unordered_map>
#include <vector>

#include "log_utils.hpp"
#include "core_utils.hpp"
#include "string_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_context.hpp"
#include "core/kg_core.hpp"
#include "core/kg_registry.hpp"
#include "graphics/kg_vulkan.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;

using KalaHeaders::KalaString::BoolValue;

using KalaHeaders::KalaMath::vec2;

using KalaGraphics::Core::ViewportSize;
using KalaGraphics::Graphics::VulkanContext;

using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;

//4:3

constexpr string_view vp_640_480 = "640x480";
constexpr string_view vp_800_600 = "800x600";
constexpr string_view vp_1024_768 = "1024x768";
constexpr string_view vp_1600_1200 = "1600x1200";

//16:9

constexpr string_view vp_1280_720 = "1280x720";
constexpr string_view vp_1600_900 = "1600x900";
constexpr string_view vp_1920_1080 = "1920x1080";
constexpr string_view vp_2560_1440 = "2560x1440";
constexpr string_view vp_3840_2160 = "3840x2160";

//16:10

constexpr string_view vp_1280_800 = "640x480";
constexpr string_view vp_1680_1050 = "640x480";
constexpr string_view vp_1920_1200 = "640x480";
constexpr string_view vp_2560_1600 = "2560x1600";

//21:9

constexpr string_view vp_2560_1080 = "2560x1080";
constexpr string_view vp_3440_1440 = "3440x1440";
constexpr string_view vp_5120_2160 = "5120x2160";

//32:9

constexpr string_view vp_3840_1080 = "3840x1080";
constexpr string_view vp_5120_1440 = "5120x1440";

static unordered_map<ViewportSize, string_view, EnumHash<ViewportSize>> vpNames =
{
    { ViewportSize::VP_640_480,   vp_640_480 },
    { ViewportSize::VP_800_600,   vp_800_600 },
    { ViewportSize::VP_1024_768,  vp_1024_768 },
    { ViewportSize::VP_1600_1200, vp_1600_1200 },

    { ViewportSize::VP_1280_720,  vp_1280_720 },
    { ViewportSize::VP_1600_900,  vp_1600_900 },
    { ViewportSize::VP_1920_1080, vp_1920_1080 },
    { ViewportSize::VP_2560_1440, vp_2560_1440 },
    { ViewportSize::VP_3840_2160, vp_3840_2160 },

    { ViewportSize::VP_1280_800,  vp_1280_800 },
    { ViewportSize::VP_1680_1050, vp_1680_1050 },
    { ViewportSize::VP_1920_1200, vp_1920_1200 },
    { ViewportSize::VP_2560_1600, vp_2560_1600 },

    { ViewportSize::VP_2560_1080, vp_2560_1080 },
    { ViewportSize::VP_3440_1440, vp_3440_1440 },
    { ViewportSize::VP_5120_2160, vp_5120_2160 },

    { ViewportSize::VP_3840_1080, vp_3840_1080 },
    { ViewportSize::VP_5120_1440, vp_5120_1440 }
};

static unordered_map<ViewportSize, vec2, EnumHash<ViewportSize>> vpSizes =
{
    { ViewportSize::VP_640_480,   vec2(640, 480) },
    { ViewportSize::VP_800_600,   vec2(800, 600) },
    { ViewportSize::VP_1024_768,  vec2(1024, 768) },
    { ViewportSize::VP_1600_1200, vec2(1600, 1200) },

    { ViewportSize::VP_1280_720,  vec2(1280, 720) },
    { ViewportSize::VP_1600_900,  vec2(1600, 900) },
    { ViewportSize::VP_1920_1080, vec2(1920, 1080) },
    { ViewportSize::VP_2560_1440, vec2(2560, 1440) },
    { ViewportSize::VP_3840_2160, vec2(3840, 2160) },

    { ViewportSize::VP_1280_800,  vec2(1280, 800) },
    { ViewportSize::VP_1680_1050, vec2(1680, 1050) },
    { ViewportSize::VP_1920_1200, vec2(1920, 1200) },
    { ViewportSize::VP_2560_1600, vec2(2560, 1600) },

    { ViewportSize::VP_2560_1080, vec2(2560, 1080) },
    { ViewportSize::VP_3440_1440, vec2(3440, 1440) },
    { ViewportSize::VP_5120_2160, vec2(5120, 2160) },

    { ViewportSize::VP_3840_1080, vec2(3840, 1080) },
    { ViewportSize::VP_5120_1440, vec2(5120, 1440) }
};

namespace KalaGraphics::Core
{
    static VkInstance vk_instance{};

    static KalaGraphicsRegistry<GraphicsContext> registry{};

    KalaGraphicsRegistry<GraphicsContext>& GraphicsContext::GetRegistry() { return registry; }

    void GraphicsContext::SetVKInstance(VkInstance in_vk_instance)
    {
        if (!in_vk_instance)
        {
            Log::Print(
                "Cannot set instance to an empty one!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);
        }

        vk_instance = in_vk_instance;
    }
    VkInstance GraphicsContext::GetVKInstance()
    {
        if (!vk_instance)
        {
            Log::Print(
                "Cannot get Vulkan instance because it is not assigned!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return vk_instance;
    }

    string_view GraphicsContext::GetStaticViewportName(ViewportSize vpSize)
    {   
        string_view out{};
        if (!EnumToString(vpSize, vpNames, out))
        {
            Log::Print(
                "Failed to get viewport name because the passed enum was invalid!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return out;
    }
    vec2 GraphicsContext::GetStaticViewportSizeValue(ViewportSize vpSize)
    {
		auto it = vpSizes.find(vpSize);
		if (it == vpSizes.end())
        {
            Log::Print(
                "Failed to get viewport value because the passed enum was invalid!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

		return it->second;
    }

    GraphicsContext* GraphicsContext::Initialize(const GraphicsContextData& in_context)
    {
        unique_ptr<GraphicsContext> newContext = make_unique<GraphicsContext>();
        GraphicsContext* contextPtr = newContext.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        contextPtr->ID = newID;

        contextPtr->contextData = in_context;

        if (contextPtr->contextData.windowID == 0)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context because it had no window ID!");

            return nullptr;
        }

        string idStr = to_string(newID);

        if (registry.createdContent.contains(contextPtr->contextData.windowID))
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because its ID was added more than once!");

            return nullptr;
        }

#ifdef _WIN32
        if (!contextPtr->contextData.context_window)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because it was missing its window!");

            return nullptr;
        }

        HWND hwnd = ToVar<HWND>(contextPtr->contextData.context_window);
        if (!IsWindow(hwnd))
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because it did not contain a real window!");

            return nullptr;
        }
#else
        if (!contextPtr->contextData.context_display
            || !contextPtr->contextData.context_window)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because it was missing its display or window!");

            return nullptr;
        }

        Display* display = ToVar<Display*>(contextPtr->contextData.context_display);
        Window window = ToVar<Window>(contextPtr->contextData.context_window);

        XWindowAttributes attr{};
        if (!XGetWindowAttributes(display, window, &attr))
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because it did not contain a real display or window!");

            return nullptr;
        }
#endif

        if (!vk_instance)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because no Vulkan instance was passed!");

            return nullptr;
        }

        registry.AddContent(newID, std::move(newContext));

        string isFBDynamic = string(BoolValue(contextPtr->vpData.isDynamicViewport));
        string fbVal = string(GetStaticViewportName(contextPtr->vpData.vpSize));

        Log::Print(
            "Created new graphics context with ID '" + idStr + "'!",
            "KG_CONTEXT",
            LogType::LOG_SUCCESS);

        if (!VulkanContext::IsInitialized()) VulkanContext::Initialize();
        VulkanContext* vulkanContextPtr = VulkanContext::InitializeContext(contextPtr->ID);
        contextPtr->vulkanContextID = vulkanContextPtr->GetID();

        return contextPtr;
    }

    u32 GraphicsContext::GetID() const { return ID; }
    u32 GraphicsContext::GetVulkanContextID() const { return vulkanContextID; }

    VSyncState GraphicsContext::GetVSyncState() const { return vsyncState; }
    void GraphicsContext::SetVSyncState(VSyncState newState)
    {
        if (newState == VSyncState::VSYNC_INVALID)
        {
            Log::Print(
                "Cannot set vsync state to VSYNC_INVALID!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (newState == vsyncState)
        {
            Log::Print(
                "Cannot set vsync state to same value!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        VSyncState old = vsyncState;
        vsyncState = newState;

        VulkanContext* ctx = VulkanContext::GetRegistry().GetContent(vulkanContextID);
        if (!ctx->SetVSyncState()) vsyncState = old;
    }

    vec2 GraphicsContext::GetStaticViewportSize() const
    {
        return vpSizes[vpData.vpSize];
    }
    void GraphicsContext::SetStaticViewportSize(ViewportSize vpSize)
    {
        vpData.vpSize = vpSize;

        Log::Print(
            "Set static viewport size to " + string(GetStaticViewportName(vpSize)) + " for window '" + to_string(contextData.windowID) + "'.", 
            "KG_CONTEXT",
            LogType::LOG_INFO);
    }

    bool GraphicsContext::IsDynamicViewport() const { return vpData.isDynamicViewport; }
    void GraphicsContext::SetDynamicViewportState(bool newValue)
    {
        vpData.isDynamicViewport = newValue;

        Log::Print(
            "Set dynamic viewport state to " + string(BoolValue(newValue)) + " for window '" + to_string(contextData.windowID) + "'!", 
            "KG_CONTEXT",
            LogType::LOG_INFO);
    }

    vec2 GraphicsContext::GetDepth() const { return vpData.depth; }
    void GraphicsContext::SetDepth(vec2 newValue) { vpData.depth = newValue; }

    vec2 GraphicsContext::GetViewportOffset() const { return vpData.viewportOffset; }
    void GraphicsContext::SetViewportOffset(vec2 newValue) { vpData.viewportOffset = newValue; }

    vec2 GraphicsContext::GetScissorSize() const { return vpData.scissorSize; }
    void GraphicsContext::SetScissorSize(vec2 newValue) { vpData.scissorSize = newValue; }

    const GraphicsContextData& GraphicsContext::GetGraphicsContextData() const { return contextData; }

    void GraphicsContext::Update()
    {
        VulkanContext* ctx = VulkanContext::GetRegistry().GetContent(vulkanContextID);

        if (!ctx)
        {
            Log::Print(
                "Failed to update graphics context '" + to_string(ID) + "' because its vulkan context ID '" + to_string(vulkanContextID) + "' was not found!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        ctx->Update();
    }

    void GraphicsContext::ResizeUpdate()
    {
        VulkanContext* ctx = VulkanContext::GetRegistry().GetContent(vulkanContextID);

        if (!ctx)
        {
            KalaGraphicsCore::ForceClose(
                "Graphics context update error",
                "Failed to run graphics context resize update because the Vulkan context '" + to_string(ID) + "' was not found!");
        }

        ctx->ResizeUpdate();
    }

    void GraphicsContext::Destroy()
    {
        VulkanContext* vkctx = VulkanContext::GetRegistry().GetContent(vulkanContextID);
        if (!vkctx)
        {
            Log::Print(
                "Failed to destroy vulkan context because its ID '" + to_string(vulkanContextID) + "' was not found!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vkctx->Destroy();

        registry.RemoveContent(ID);
    }

    GraphicsContext::~GraphicsContext()
    {
		Log::Print(
			"Destroying graphics context '" + to_string(ID) + "'.",
			"KG_CONTEXT",
			LogType::LOG_INFO);
    }
}
