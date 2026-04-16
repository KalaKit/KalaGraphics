//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#include "glcorearb.h"
#else
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan_core.h"

#include "log_utils.hpp"
#include "core_utils.hpp"
#include "string_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_context.hpp"
#include "core/kg_core.hpp"
#include "core/kg_registry.hpp"
#include "_internal/_kg_vulkan.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaCore::ContainsValue;
using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;

using KalaHeaders::KalaString::BoolValue;

using KalaHeaders::KalaMath::vec2;

using KalaGraphics::Core::FramebufferSize;
using KalaGraphics::Internal::Vulkan_Core;

using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;
using std::vector;

//4:3

constexpr string_view fb_640_480 = "640x480";
constexpr string_view fb_800_600 = "800x600";
constexpr string_view fb_1024_768 = "1024x768";
constexpr string_view fb_1600_1200 = "1600x1200";

//16:9

constexpr string_view fb_1280_720 = "1280x720";
constexpr string_view fb_1600_900 = "1600x900";
constexpr string_view fb_1920_1080 = "1920x1080";
constexpr string_view fb_2560_1440 = "2560x1440";
constexpr string_view fb_3840_2160 = "3840x2160";

//16:10

constexpr string_view fb_1280_800 = "640x480";
constexpr string_view fb_1680_1050 = "640x480";
constexpr string_view fb_1920_1200 = "640x480";
constexpr string_view fb_2560_1600 = "2560x1600";

//21:9

constexpr string_view fb_2560_1080 = "2560x1080";
constexpr string_view fb_3440_1440 = "3440x1440";
constexpr string_view fb_5120_2160 = "5120x2160";

//32:9

constexpr string_view fb_3840_1080 = "3840x1080";
constexpr string_view fb_5120_1440 = "5120x1440";

static const unordered_map<FramebufferSize, string_view, EnumHash<FramebufferSize>> framebufferNames =
{
    { FramebufferSize::FB_640_480,   fb_640_480 },
    { FramebufferSize::FB_800_600,   fb_800_600 },
    { FramebufferSize::FB_1024_768,  fb_1024_768 },
    { FramebufferSize::FB_1600_1200, fb_1600_1200 },

    { FramebufferSize::FB_1280_720,  fb_1280_720 },
    { FramebufferSize::FB_1600_900,  fb_1600_900 },
    { FramebufferSize::FB_1920_1080, fb_1920_1080 },
    { FramebufferSize::FB_2560_1440, fb_2560_1440 },
    { FramebufferSize::FB_3840_2160, fb_3840_2160 },

    { FramebufferSize::FB_1280_800,  fb_1280_800 },
    { FramebufferSize::FB_1680_1050, fb_1680_1050 },
    { FramebufferSize::FB_1920_1200, fb_1920_1200 },
    { FramebufferSize::FB_2560_1600, fb_2560_1600 },

    { FramebufferSize::FB_2560_1080, fb_2560_1080 },
    { FramebufferSize::FB_3440_1440, fb_3440_1440 },
    { FramebufferSize::FB_5120_2160, fb_5120_2160 },

    { FramebufferSize::FB_3840_1080, fb_3840_1080 },
    { FramebufferSize::FB_5120_1440, fb_5120_1440 }
};

static const unordered_map<FramebufferSize, vec2, EnumHash<FramebufferSize>> framebufferSizes =
{
    { FramebufferSize::FB_640_480,   vec2(640, 480) },
    { FramebufferSize::FB_800_600,   vec2(800, 600) },
    { FramebufferSize::FB_1024_768,  vec2(1024, 768) },
    { FramebufferSize::FB_1600_1200, vec2(1600, 1200) },

    { FramebufferSize::FB_1280_720,  vec2(1280, 720) },
    { FramebufferSize::FB_1600_900,  vec2(1600, 900) },
    { FramebufferSize::FB_1920_1080, vec2(1920, 1080) },
    { FramebufferSize::FB_2560_1440, vec2(2560, 1440) },
    { FramebufferSize::FB_3840_2160, vec2(3840, 2160) },

    { FramebufferSize::FB_1280_800,  vec2(1280, 800) },
    { FramebufferSize::FB_1680_1050, vec2(1680, 1050) },
    { FramebufferSize::FB_1920_1200, vec2(1920, 1200) },
    { FramebufferSize::FB_2560_1600, vec2(2560, 1600) },

    { FramebufferSize::FB_2560_1080, vec2(2560, 1080) },
    { FramebufferSize::FB_3440_1440, vec2(3440, 1440) },
    { FramebufferSize::FB_5120_2160, vec2(5120, 2160) },

    { FramebufferSize::FB_3840_1080, vec2(3840, 1080) },
    { FramebufferSize::FB_5120_1440, vec2(5120, 1440) }
};

namespace KalaGraphics::Core
{
    static VkInstance vk_instance{};

    static KalaGraphicsRegistry<WindowContext> registry{};

    KalaGraphicsRegistry<WindowContext>& WindowContext::GetRegistry() { return registry; }

    void WindowContext::SetVKInstance(VkInstance in_vk_instance)
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
    VkInstance WindowContext::GetVKInstance()
    {
        if (!vk_instance)
        {
            Log::Print(
                "Cannot get VK instance because it is not assigned!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return vk_instance;
    }

    bool WindowContext::IsValidWindowID(u32 windowID)
    {
        if (registry.runtimeContent.empty()) return false;

        for (const auto& c : registry.runtimeContent)
        {
            if (c->context.windowID == windowID) return true;
        }

        return false;
    }

    string_view WindowContext::GetFramebufferName(FramebufferSize fbSize)
    {   
        string_view out{};
        if (!EnumToString(fbSize, framebufferNames, out))
        {
            Log::Print(
                "Failed to get framebuffer name because the passed enum was invalid!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return out;
    }
    vec2 WindowContext::GetFramebufferSize(FramebufferSize fbSize)
    {
		auto it = framebufferSizes.find(fbSize);
		if (it == framebufferSizes.end())
        {
            Log::Print(
                "Failed to get framebuffer value because the passed enum was invalid!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

		return it->second;
    }

    WindowContext* WindowContext::Initialize(const WindowContextData& in_context)
    {
        unique_ptr<WindowContext> newCont = make_unique<WindowContext>();
        WindowContext* cont = newCont.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        cont->ID = newID;

        cont->context = in_context;

        if (cont->context.windowID == 0)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context because it had no window ID!");

            return nullptr;
        }

        string idStr = to_string(newID);

        if (registry.createdContent.contains(cont->context.windowID))
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because its ID was added more than once!");

            return nullptr;
        }

#ifdef _WIN32
        if (!cont->context.context_window)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because it was missing its window!");

            return nullptr;
        }

        HWND hwnd = ToVar<HWND>(cont->context.context_window);
        if (!IsWindow(hwnd))
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because it did not contain a real window!");

            return nullptr;
        }
#else
        if (!cont->context.context_display
            || !cont->context.context_window)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because it was missing its display or window!");

            return nullptr;
        }

        Display* display = ToVar<Display*>(cont->context.context_display);
        Window window = ToVar<Window>(cont->context.context_window);

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
                "Failed to initialize window context '" + idStr + "' because it was aimed for Vulkan but no Vulkan instance was passed!");

            return nullptr;
        }

        u32 apiVersion = VK_API_VERSION_1_0;
        vkEnumerateInstanceVersion(&apiVersion);

        u32 major = VK_API_VERSION_MAJOR(apiVersion);
        u32 minor = VK_API_VERSION_MINOR(apiVersion);

        if (major < 1
            || (major == 1
            && minor < 3))
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because its Vulkan version was lower than minimum required version 1.3!");

            return nullptr;
        }

        static u32 deviceCount{};
        static bool checkedInstance{};
        static vector<VkPhysicalDevice> devices{};
        
        if (!checkedInstance)
        {
            if (vkEnumeratePhysicalDevices(
                vk_instance, 
                &deviceCount, 
                nullptr) != VK_SUCCESS
                || deviceCount == 0)
            {
                KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because its Vulkan instance is invalid!");

                return nullptr;
            }

            devices.resize(deviceCount);

            vkEnumeratePhysicalDevices(
                vk_instance, 
                &deviceCount, 
                devices.data());

            checkedInstance = true;
        }

        bool surfaceSupported{};

        for (const auto& device : devices)
        {
            u32 queueCount{};
            vkGetPhysicalDeviceQueueFamilyProperties(
                device,
                &queueCount,
                nullptr);

            vector<VkQueueFamilyProperties> queues(queueCount);
            vkGetPhysicalDeviceQueueFamilyProperties(
                device,
                &queueCount,
                queues.data());

            for (u32 i = 0; i < queueCount; i++)
            {
                VkBool32 supported{};

                if (vkGetPhysicalDeviceSurfaceSupportKHR(
                    device,
                    i,
                    cont->context.context_vk_surface,
                    &supported) == VK_SUCCESS
                    && supported)
                {
                    surfaceSupported = true;
                    break;
                }
            }

            if (surfaceSupported) break;
        }

        if (!surfaceSupported)
        {
            KalaGraphicsCore::ForceClose(
                "Window context init error",
                "Failed to initialize window context '" + idStr + "' because its Vulkan surface is invalid!");

            return nullptr;
        }

        registry.AddContent(newID, std::move(newCont));

        string isFBDynamic = string(BoolValue(cont->context.isFramebufferDynamic));
        string fbVal = string(GetFramebufferName(cont->context.fbSize));

        Log::Print(
            "Created new context with ID '" + idStr + "'!",
            "KG_CONTEXT",
            LogType::LOG_SUCCESS);

        return cont;
    }

    bool WindowContext::IsInitialized() const { return isInitialized; }

    u32 WindowContext::GetID() const { return ID; }

    void WindowContext::SetVSyncState(VSyncState newState)
    {
        bool success{};
        
        //set vk vsync state

        if (success) context.state = newState;
    }
    VSyncState WindowContext::GetVSyncState() const { return context.state; }

    void WindowContext::Update()
    {
        for (const auto& c : registry.runtimeContent)
        {
            Vulkan_Core::Update(c->ID);
        }
    }

    void WindowContext::ResizeUpdate()
    {
        for (const auto& c : registry.runtimeContent)
        {
            Vulkan_Core::ResizeUpdate(c->context.windowID);
        }
    }

    void WindowContext::SetDynamicFramebufferState(bool newValue)
    {
        context.isFramebufferDynamic = newValue;

        Log::Print(
            "Set dynamic framebuffer state to " + string(BoolValue(newValue)) + " for window '" + to_string(context.windowID) + "'!", 
            "KG_CONTEXT",
            LogType::LOG_INFO);
    }
    bool WindowContext::IsDynamicFramebuffer() const { return context.isFramebufferDynamic; }

    void WindowContext::SetStaticFramebufferSize(FramebufferSize fbSize)
    {
        context.fbSize = fbSize;

        Log::Print(
            "Set static framebuffer size to " + string(GetFramebufferName(fbSize)) + " for window '" + to_string(context.windowID) + "'.", 
            "KG_CONTEXT",
            LogType::LOG_INFO);
    }
    vec2 WindowContext::GetStaticFramebufferSize() const
    {
        return GetFramebufferSize(context.fbSize);
    }

    WindowContextData& WindowContext::GetWindowContextData() { return context; }

    void WindowContext::Shutdown()
    {
        for (const auto& c : registry.runtimeContent)
        {
            Vulkan_Core::Shutdown(c->context.windowID);
        }

        registry.RemoveAllContent();
    }
}
