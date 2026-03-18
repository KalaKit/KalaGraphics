//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include "wglext.h"
#else
#include <csignal>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#include <string>
#include <unordered_map>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan_core.h"

#include "log_utils.hpp"
#include "core_utils.hpp"
#include "string_utils.hpp"

#include "core/kg_core.hpp"
#include "core/kg_registry.hpp"
#include "_internal/software/_kg_software.hpp"
#include "_internal/opengl/_kg_opengl.hpp"
#include "_internal/vulkan/_kg_vulkan.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using KalaHeaders::KalaCore::ContainsValue;
using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;

using KalaHeaders::KalaString::BoolValue;

using KalaGraphics::Internal::Software::Software_Core;
using KalaGraphics::Internal::OpenGL::OpenGL_Core;
using KalaGraphics::Internal::Vulkan::Vulkan_Core;

#ifdef __linux__
using std::raise;
#endif

using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;

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

namespace KalaGraphics::Core
{
    static u32 globalID{};

    static bool isInitialized{};

    static vector<GraphicsFeature> gfxFeatures{};
    static VkInstance vk_instance{};

    static KalaGraphicsRegistry<Context> registry{};

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

    KalaGraphicsRegistry<Context>& KalaGraphicsCore::GetRegistry() { return registry; }

    u32 KalaGraphicsCore::GetGlobalID() { return globalID; }
	void KalaGraphicsCore::SetGlobalID(u32 newID) { globalID = newID; }

    bool KalaGraphicsCore::Initialize(
        const vector<Context>& in_contexts,
        const optional<vector<GraphicsFeature>> in_gfxFeatures,
        const optional<VkInstance>& in_vk_instance)
    {
        if (isInitialized)
        {
            Log::Print(
                "Failed to initialize KalaGraphics because it has already been initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return false;
        }

        Log::Print(
            "Initializing KalaGraphics.", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_INFO);

        if (in_contexts.empty())
        {
            ForceClose(
                "KalaGraphics initialization error",
                "Failed to initialize KalaGraphics because no contexts were passed!");

            return false;
        }

        gfxFeatures = in_gfxFeatures.value_or({});
        vk_instance = in_vk_instance.value_or(nullptr);

        for (const auto& c : in_contexts)
        {
            if (c.windowID == 0)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because a context was missing its window ID!");

                return false;
            }
            string windowIDStr = to_string(c.windowID);

#ifdef _WIN32
            if (!c.context_window)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowIDStr + "' was missing its window!");

                return false;
            }

            HWND hwnd = ToVar<HWND>(c.context_window);
            if (!IsWindow(hwnd))
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowIDStr + "' did not contain a real window!");

                return false;
            }
#else
            if (!c.context_display
                || !c.context_window)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowIDStr + "' was missing its display or window!");

                return false;
            }

            Display* display = ToVar<Display*>(c.context_display);
            Window window = ToVar<Window>(c.context_window);

            XWindowAttributes attr{};
            if (!XGetWindowAttributes(display, window, &attr))
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowIDStr + "' did not contain a real display or window!");

                return false;
            }
#endif

            if (c.context_gl
                && c.context_vk_surface)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowIDStr + "' was found to contain both an OpenGL and Vulkan context!");

                return false;
            }

            if (c.context_gl)
            {
#ifdef _WIN32
                HGLRC thisContext = ToVar<HGLRC>(c.context_gl.value());
                HDC thisDC = GetDC(hwnd);

                HGLRC oldContext = wglGetCurrentContext();
                HDC oldDC = wglGetCurrentDC();

                if (!wglMakeCurrent(thisDC, thisContext))
                {
                    ReleaseDC(hwnd, thisDC);

                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowIDStr + "' contained a broken OpenGL context!");

                    return false;
                }

                int major{};
                int minor{};

                glGetIntegerv(GL_MAJOR_VERSION, &major);
                glGetIntegerv(GL_MINOR_VERSION, &minor);

                wglMakeCurrent(oldDC, oldContext);
                ReleaseDC(hwnd, thisDC);
#else
                GLXContext thisContext = ToVar<GLXContext>(c.context_gl.value());

                GLXContext oldContext = glXGetCurrentContext();
                GLXDrawable oldDrawable = glXGetCurrentDrawable();

                if (!glXMakeCurrent(display, window, thisContext))
                {
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowIDStr + "' OpenGL context is invalid!");

                    return false;
                }

                int major{};
                int minor{};

                glGetIntegerv(GL_MAJOR_VERSION, &major);
                glGetIntegerv(GL_MINOR_VERSION, &minor);

                glXMakeCurrent(display, oldDrawable, oldContext);
#endif

                if (major < 3
                    || (major == 3
                    && minor < 3))
                {
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowIDStr + "' OpenGL version was lower than minimum required version 3.3!");

                    return false;
                }
            }
            else if (c.context_vk_surface)
            {
                if (!vk_instance)
                {
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowIDStr + "' context was aimed for Vulkan but no Vulkan instance was passed!");

                    return false;
                }

                u32 apiVersion = VK_API_VERSION_1_0;
                vkEnumerateInstanceVersion(&apiVersion);

                u32 major = VK_API_VERSION_MAJOR(apiVersion);
                u32 minor = VK_API_VERSION_MINOR(apiVersion);

                if (major < 1
                    || (major == 1
                    && minor < 3))
                {
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowIDStr + "' Vulkan version was lower than minimum required version 1.3!");

                    return false;
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
                        ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowIDStr + "' Vulkan instance is invalid!");

                        return false;
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
                            c.context_vk_surface.value(),
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
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowIDStr + "' Vulkan surface is invalid!");

                    return false;
                }
            }

            bool forceSoftware = ContainsValue(gfxFeatures, GraphicsFeature::GF_FORCE_SOFTWARE);
            bool forceOpenGL = ContainsValue(gfxFeatures, GraphicsFeature::GF_FORCE_OPENGL);
            bool forceVulkan = ContainsValue(gfxFeatures, GraphicsFeature::GF_FORCE_VULKAN);

            auto can_use_opengl = []() -> bool
                {
                    for (const auto& c : registry.runtimeContent)
                    {
                        if (c->context_gl) return true;
                    }

                    return false;
                };

            auto can_use_vulkan = []() -> bool
                {
                    for (const auto& c : registry.runtimeContent)
                    {
                        if (c->context_vk_surface)
                        {
                            return true;
                        }
                    }

                    return false;
                };

            unique_ptr<Context> newCont = make_unique<Context>(c);
            Context* cont = newCont.get();

            if (forceSoftware) newCont->renderTarget = RenderTarget::RT_SOFTWARE;
            else if (forceOpenGL)
            {
                if (!can_use_opengl())
                {
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to force OpenGL because no OpenGL context was passed or no OpenGL contexts camne with a valid window!");

                    return false;
                }
                else newCont->renderTarget = RenderTarget::RT_OPENGL;
            }
            else if (forceVulkan)
            {
                if (!can_use_vulkan())
                {
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to force Vulkan because no Vulkan context was passed or no Vulkan contexts camne with a valid window!");

                    return false;
                }
                else newCont->renderTarget = RenderTarget::RT_VULKAN;
            }

            //default path - user did not force any specific render pipeline
            else
            {
                bool wantsToUseVulkan =
                    ContainsValue(gfxFeatures, GraphicsFeature::GF_COMPUTE_SHADERS)
                    || ContainsValue(gfxFeatures, GraphicsFeature::GF_RAY_TRACING)
                    || ContainsValue(gfxFeatures, GraphicsFeature::GF_PATH_TRACING);

                if (wantsToUseVulkan)
                {
                    if (!can_use_vulkan())
                    {
                        ForceClose(
                            "KalaGraphics initialization error",
                            "Failed to use Vulkan through Vulkan-only Graphics features because no Vulkan context was passed or no Vulkan contexts came with a valid window!");
                    
                        return false;
                    }

                    newCont->renderTarget = RenderTarget::RT_VULKAN;
                }
                else
                {
                    if (can_use_opengl()) newCont->renderTarget = RenderTarget::RT_OPENGL;
                    else if (can_use_vulkan())
                    {
                        Log::Print(
                            "Using Vulkan because no valid OpenGL contexts were passed.", 
                            "KALAGRAPHICS_CORE",
                            LogType::LOG_WARNING);

                        newCont->renderTarget = RenderTarget::RT_VULKAN;
                    }
                    else
                    {
                        Log::Print(
                            "Fell back to software rendering because no valid OpenGL or Vulkan contexts were passed.", 
                            "KALAGRAPHICS_CORE",
                            LogType::LOG_WARNING);

                        newCont->renderTarget = RenderTarget::RT_SOFTWARE;
                    }
                }
            }

            u32 newID = GetGlobalID() + 1;
		    SetGlobalID(newID);

            registry.AddContent(newID, std::move(newCont));

            string winID = to_string(cont->windowID);
            string isFBDynamic = string(BoolValue(cont->isFramebufferDynamic));
            string fbVal = string(GetFramebufferName(cont->fbSize));
            string renderTarget = GetRenderTargetName(cont->windowID);

            Log::Print(
                "Added valid context '" + winID + "'!\n"
                "    Render target: " + renderTarget + "\n"
                "    Framebuffer is dynamic: " + isFBDynamic + "\n"
                "    Framebuffer size: " + fbVal,
                "KALAGRAPHICS_CORE",
                LogType::LOG_INFO);
        }

        isInitialized = true;

        Log::Print(
            "Finished initializing all contexts!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_SUCCESS);

        return true;
    }

    bool KalaGraphicsCore::IsInitialized() { return isInitialized; }

    void KalaGraphicsCore::Update()
    {
        for (const auto& c : registry.runtimeContent)
        {
            switch (c->renderTarget)
            {
                default:
                case RenderTarget::RT_SOFTWARE:
                {
                    break;
                }
                case RenderTarget::RT_OPENGL:
                {
                    break;
                }
                case RenderTarget::RT_VULKAN:
                {
                    break;
                }
            }   
        }
    }

    void KalaGraphicsCore::ResizeUpdate()
    {
        for (const auto& c : registry.runtimeContent)
        {
            switch (c->renderTarget)
            {
                default:
                case RenderTarget::RT_SOFTWARE:
                {
                    break;
                }
                case RenderTarget::RT_OPENGL:
                {
                    break;
                }
                case RenderTarget::RT_VULKAN:
                {
                    break;
                }
            }   
        }
    }

    void KalaGraphicsCore::SetDynamicFramebufferState(
        u32 windowID,
        bool newValue)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Cannot set dynamic framebuffer state for window '" + to_string(windowID) + "' because KalaGraphics has not been initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        for (auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID)
            {
                c->isFramebufferDynamic = newValue;

                Log::Print(
                    "Set dynamic framebuffer state to " + string(BoolValue(newValue)) + " for window '" + to_string(windowID) + "'!", 
                    "KALAGRAPHICS_CORE",
                    LogType::LOG_INFO);

                return;
            }
        }

        Log::Print(
            "Failed to set dynamic framebuffer state for window '" + to_string(windowID) + "' because the window ID was not found!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_ERROR,
            2);
    }
    bool KalaGraphicsCore::IsDynamicFramebuffer(u32 windowID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Cannot get dynamic framebuffer state for window '" + to_string(windowID) + "' because KalaGraphics has not been initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return false;
        }

        for (auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID) return c->isFramebufferDynamic;
        }

        Log::Print(
            "Failed to get dynamic framebuffer state for window '" + to_string(windowID) + "' because the window ID was not found!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_ERROR,
            2);

        return false;
    }

    void KalaGraphicsCore::SetStaticFramebufferSize(
        u32 windowID,
        FramebufferSize fbSize)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Cannot set static framebuffer size for window '" + to_string(windowID) + "' because KalaGraphics has not been initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        for (auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID)
            {
                c->fbSize = fbSize;

                Log::Print(
                    "Set static framebuffer size to " + string(GetFramebufferName(fbSize)) + " for window '" + to_string(windowID) + "'.", 
                    "KALAGRAPHICS_CORE",
                    LogType::LOG_INFO);

                return;
            }
        }

        Log::Print(
            "Failed to set static framebuffer size for window '" + to_string(windowID) + "' because the window ID was not found!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_ERROR,
            2);
    }
    vec2 KalaGraphicsCore::GetStaticFramebufferSize(u32 windowID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Cannot get static framebuffer size for window '" + to_string(windowID) + "' because KalaGraphics has not been initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        for (const auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID) return GetFramebufferSize(c->fbSize);
        }

        Log::Print(
            "Failed to get static framebuffer size for window '" + to_string(windowID) + "' because the window ID was not found!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_ERROR,
            2);

        return {};
    }

    string_view KalaGraphicsCore::GetFramebufferName(FramebufferSize fbSize)
    {   
        string_view out{};
        if (!EnumToString(fbSize, framebufferNames, out))
        {
            Log::Print(
                "Failed to get framebuffer name because the passed enum was invalid!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return out;
    }
    vec2 KalaGraphicsCore::GetFramebufferSize(FramebufferSize fbSize)
    {
		auto it = framebufferSizes.find(fbSize);
		if (it == framebufferSizes.end())
        {
            Log::Print(
                "Failed to get framebuffer value because the passed enum was invalid!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return {};
        }

		return it->second;
    }

    RenderTarget KalaGraphicsCore::GetRenderTarget(u32 windowID)
    { 
        for (auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID) return c->renderTarget;
        }

        Log::Print(
            "Failed to get render target for window '" + to_string(windowID) + "' because the window ID was not found!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_ERROR,
            2);

        return {};
    }

    string KalaGraphicsCore::GetRenderTargetName(u32 windowID)
    {
        for (auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID)
            {
                return c->renderTarget == RenderTarget::RT_SOFTWARE
                    ? "Software"
                    : c->renderTarget == RenderTarget::RT_OPENGL
                        ? "OpenGL"
                        : "Vulkan";
            }
        }

        Log::Print(
            "Failed to get render target name for window '" + to_string(windowID) + "' because the window ID was not found!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_ERROR,
            2);

        return {};
    }

    bool KalaGraphicsCore::IsValidWindowID(u32 windowID)
    {
        if (registry.runtimeContent.empty()) return false;

        for (const auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID) return true;
        }

        return false;
    }

    Context* KalaGraphicsCore::GetContext(u32 windowID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Cannot get context because KalaGraphics is not initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        for (const auto& c : registry.runtimeContent)
        {
            if (c->windowID == windowID) return c;
        }

        Log::Print(
            "Failed to find context with window ID '" + to_string(windowID) + "'!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_ERROR,
            2);

        return {};
    }

    VkInstance KalaGraphicsCore::GetVKInstance()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Cannot get GL context because KalaGraphics is not initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return 0;
        }

        if (!vk_instance)
        {
            Log::Print(
                "Cannot get VK instance because it is not assigned!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return vk_instance;
    }

    void KalaGraphicsCore::Shutdown()
    {
        for (const auto& c : registry.runtimeContent)
        {
            switch (c->renderTarget)
            {
                default:
                case RenderTarget::RT_SOFTWARE:
                {
                    break;
                }
                case RenderTarget::RT_OPENGL:
                {
                    break;
                }
                case RenderTarget::RT_VULKAN:
                {
                    break;
                }
            }   
        }
    }

    void KalaGraphicsCore::ForceClose(
		string_view target,
		string_view reason)
	{
		Log::Print(
			"\n================"
			"\nFORCE CLOSE"
			"\n================\n",
			true);

		Log::Print(
			reason,
			target,
			LogType::LOG_ERROR,
			2,
			true,
			TimeFormat::TIME_NONE,
			DateFormat::DATE_NONE);

#ifdef _WIN32
		__debugbreak();
#else
		raise(SIGTRAP);
#endif
	}
}