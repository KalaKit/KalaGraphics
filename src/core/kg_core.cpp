//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include "wglext.h"
#else
#include <csignal>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan_core.h"

#include <string>
#include "log_utils.hpp"
#include "core_utils.hpp"

#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using KalaHeaders::KalaCore::ContainsValue;
using KalaHeaders::KalaCore::ToVar;

#ifdef __linux__
using std::raise;
#endif

using std::string;
using std::string_view;
using std::to_string;

namespace KalaGraphics::Core
{
    static bool isInitialized{};

    static RenderTarget renderTarget{};

    static vector<GraphicsFeature> gfxFeatures{};
    static VkInstance vk_instance{};
    static vector<Context> contexts{};

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

        gfxFeatures = in_gfxFeatures.value_or({});
        vk_instance = in_vk_instance.value_or(nullptr);
        contexts = in_contexts;

        if (contexts.empty())
        {
            ForceClose(
                "KalaGraphics initialization error",
                "Failed to initialize KalaGraphics because no contexts were passed!");

            return false;
        }

        for (const auto& c : contexts)
        {
            if (c.windowID == 0)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because a context was missing its window ID!");

                return false;
            }
            string windowID = to_string(c.windowID);

#ifdef _WIN32
            if (!c.context_window)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowID + "' was missing its window!");

                return false;
            }

            HWND hwnd = ToVar<HWND>(c.context_window);
            if (!IsWindow(hwnd))
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowID + "' did not contain a real window!");

                return false;
            }
#else
            if (!c.context_display
                || !c.context_window)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowID + "' was missing its display or window!");

                return false;
            }

            Display* display = ToVar<Display*>(c.context_display);
            Window window = ToVar<Window>(c.context_window);

            XWindowAttributes attr{};
            if (!XGetWindowAttributes(display, window, &attr))
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowID + "' did not contain a real display or window!");

                return false;
            }
#endif

            if (c.context_gl
                && c.context_vk_surface)
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to initialize KalaGraphics because context '" + windowID + "' was found to contain both an OpenGL and Vulkan context!");

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
                        "Failed to initialize KalaGraphics because context '" + windowID + "' contained a broken OpenGL context!");

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
                        "Failed to initialize KalaGraphics because context '" + windowID + "' OpenGL context is invalid!");

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
                        "Failed to initialize KalaGraphics because context '" + windowID + "' OpenGL version was lower than minimum required version 3.3!");

                    return false;
                }
            }
            else if (c.context_vk_surface)
            {
                if (!vk_instance)
                {
                    ForceClose(
                        "KalaGraphics initialization error",
                        "Failed to initialize KalaGraphics because context '" + windowID + "' context was aimed for Vulkan but no Vulkan instance was passed!");

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
                        "Failed to initialize KalaGraphics because context '" + windowID + "' Vulkan version was lower than minimum required version 1.3!");

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
                        "Failed to initialize KalaGraphics because context '" + windowID + "' Vulkan instance is invalid!");

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
                        "Failed to initialize KalaGraphics because context '" + windowID + "' Vulkan surface is invalid!");

                    return false;
                }
            }
        }

        bool forceSoftware = ContainsValue(gfxFeatures, GraphicsFeature::GF_FORCE_SOFTWARE);
        bool forceOpenGL = ContainsValue(gfxFeatures, GraphicsFeature::GF_FORCE_OPENGL);
        bool forceVulkan = ContainsValue(gfxFeatures, GraphicsFeature::GF_FORCE_VULKAN);

        auto can_use_opengl = []() -> bool
            {
                for (const auto& c : contexts)
                {
                    if (c.context_gl) return true;
                }

                return false;
            };

        auto can_use_vulkan = []() -> bool
            {
                for (const auto& c : contexts)
                {
                    if (c.context_vk_surface)
                    {
                        return true;
                    }
                }

                return false;
            };

        if (forceSoftware) renderTarget = RenderTarget::RT_SOFTWARE;
        else if (forceOpenGL)
        {
            if (!can_use_opengl())
            {
                ForceClose(
                    "KalaGraphics initialization error",
                    "Failed to force OpenGL because no OpenGL context was passed or no OpenGL contexts camne with a valid window!");

                return false;
            }
            else renderTarget = RenderTarget::RT_OPENGL;
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
            else renderTarget = RenderTarget::RT_VULKAN;
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

                renderTarget = RenderTarget::RT_VULKAN;
            }
            else
            {
                if (can_use_opengl()) renderTarget = RenderTarget::RT_OPENGL;
                else if (can_use_vulkan())
                {
                    Log::Print(
                        "Using Vulkan because no valid OpenGL contexts were passed.", 
                        "KALAGRAPHICS_CORE",
                        LogType::LOG_WARNING);

                    renderTarget = RenderTarget::RT_VULKAN;
                }
                else
                {
                    Log::Print(
                        "Fell back to software rendering because no valid OpenGL or Vulkan contexts were passed.", 
                        "KALAGRAPHICS_CORE",
                        LogType::LOG_WARNING);

                    renderTarget = RenderTarget::RT_SOFTWARE;
                }
            }
        }

        isInitialized = true;

        Log::Print(
            "Finished initializing KalaGraphics! The chosen render backend is '" + GetRenderTargetName() + "'", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_SUCCESS);

        return true;
    }

    bool KalaGraphicsCore::IsInitialized() { return isInitialized; }

    RenderTarget KalaGraphicsCore::GetRenderTarget() { return renderTarget; }

    string KalaGraphicsCore::GetRenderTargetName()
    {
        return renderTarget == RenderTarget::RT_SOFTWARE
            ? "Software"
            : renderTarget == RenderTarget::RT_OPENGL
                ? "OpenGL"
                : "Vulkan";
    }

    bool KalaGraphicsCore::IsValidWindowID(u32 windowID)
    {
        if (contexts.empty()) return false;

        for (const auto& c : contexts)
        {
            if (c.windowID == windowID) return true;
        }

        return false;
    }

    Context KalaGraphicsCore::GetContext(u32 windowID)
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

        for (const auto& c : contexts)
        {
            if (c.windowID == windowID) return c;
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