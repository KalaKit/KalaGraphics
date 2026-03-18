//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <optional>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

struct VkInstance_T;
using VkInstance = VkInstance_T*;

struct VkSurfaceKHR_T;
using VkSurfaceKHR = VkSurfaceKHR_T*;

namespace KalaGraphics::Core
{
    using KalaHeaders::KalaMath::vec2;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::optional;
    using std::nullopt;

    using u8 = uint8_t;
    using u32 = uint32_t;

    enum class RenderTarget : u8
    {
        RT_INVALID = 0u,

        //CPU rendering - fallback if no gl or vk context was passed
        RT_SOFTWARE = 1u,

        //OpenGL 3.3 - default renderer unless overridden by graphics features
        RT_OPENGL = 2u,

        //Vulkan 1.3 - prioritized when vk-specific graphics features were chosen
        RT_VULKAN = 3u
    };

    //Graphics features that trigger KalaMake to use Vulkan over OpenGL
    enum class GraphicsFeature : u8
    {
        GF_INVALID = 0u,

        //enforce software rendering even if gl or vk contexts were passed
        GF_FORCE_SOFTWARE = 1u,

        //enforce opengl even if vk features were passed
        GF_FORCE_OPENGL = 2u,

        //enforce vulkan even if capable as opengl
        GF_FORCE_VULKAN = 3u,

        //vk-only compute shaders that enable gpu-driven particle systems,
        //gpu physics, gpu culling, gpu-driven procedual generation and more
        GF_COMPUTE_SHADERS = 4u,

        //vk-only hardware-level raytracing
        GF_RAY_TRACING = 5u,

        //vk-only hardware-level raytracing and pathtracing
        GF_PATH_TRACING = 6u
    };

    enum class FramebufferSize : u8
    {
        FB_INVALID = 0u,

        //4:3

        FB_640_480 = 1u,
        FB_800_600 = 2u,
        FB_1024_768 = 3u,
        FB_1600_1200 = 4u,

        //16:9

        FB_1280_720 = 5u,
        FB_1600_900 = 6u,
        FB_1920_1080 = 7u,
        FB_2560_1440 = 8u,
        FB_3840_2160 = 9u,

        //16:10

        FB_1280_800 = 10u,
        FB_1680_1050 = 11u,
        FB_1920_1200 = 12u,
        FB_2560_1600 = 13u,

        //21:9

        FB_2560_1080 = 14u,
        FB_3440_1440 = 15u,
        FB_5120_2160 = 16u,

        //32:9

        FB_3840_1080 = 17u,
        FB_5120_1440 = 18u
    };

    struct LIB_API Context
    {
        u32 windowID{};

        bool isFramebufferDynamic = true;
        FramebufferSize fbSize = FramebufferSize::FB_1920_1080;

        RenderTarget renderTarget{};

#ifdef _WIN32
        uintptr_t context_window{};
#else
        uintptr_t context_display{};
        uintptr_t context_window{};
#endif

        optional<uintptr_t> context_gl = nullopt;
        optional<VkSurfaceKHR> context_vk_surface = nullopt;
    };

    class LIB_API KalaGraphicsCore
    {
    public:
        static KalaGraphicsRegistry<Context>& GetRegistry();

        static u32 GetGlobalID();
		static void SetGlobalID(u32 newID);

        //Initialize KalaGraphics with your desired features and contexts.
        //Framebuffer is dynamic by default.
        //If no GL or VK contexts were passed then KalaGraphics falls back to software rendering.
        static bool Initialize(
            const vector<Context>& contexts,
            const optional<vector<GraphicsFeature>> gfxFeatures = nullopt,
            const optional<VkInstance>& vk_instance = nullopt);

        static bool IsInitialized();

        //Regular update - single draw call
        static void Update();

        //Called to trigger resize events - single draw call
        static void ResizeUpdate();

        //If true - then framebuffer resizes dynamically with the true window size
        static void SetDynamicFramebufferState(
            u32 windowID,
            bool newValue);
        static bool IsDynamicFramebuffer(u32 windowID);

        //Sets static framebuffer size, only applied if dynamic framebuffer is disabled
        static void SetStaticFramebufferSize(
            u32 windowID,
            FramebufferSize fbSize);
        static vec2 GetStaticFramebufferSize(u32 windowID);

        static string_view GetFramebufferName(FramebufferSize fbSize);
        static vec2 GetFramebufferSize(FramebufferSize fbSize);

        //Returns the render target chosen for a window during initialization
        static RenderTarget GetRenderTarget(u32 windowID);

        static string GetRenderTargetName(u32 windowID);

        //Returns true if this window ID was passed
        static bool IsValidWindowID(u32 windowID);

        //Gives the gl/vk context attached to the window
        static Context* GetContext(u32 windowID);

        //Gives the global vk instance
        static VkInstance GetVKInstance();

        //Shuts down KalaGraphics cleanly and frees all resources
        static void Shutdown();

        //Force-closes the application and gives a breakpoint, good for hard stops or bad user errors
		static void ForceClose(
			string_view title,
			string_view reason);
    };
}