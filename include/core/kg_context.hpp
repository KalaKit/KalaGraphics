//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>

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

    using u8 = uint8_t;
    using u32 = uint32_t;

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

    enum class VSyncState : u8
	{
        VSYNC_INVALID = 0u,

        //Framerate is capped to monitor refresh rate
		VSYNC_ON = 1u,

        //Framerate is uncapped, runs as fast as render loop allows, introduces tearing
		VSYNC_OFF = 2u
	};

    struct LIB_API WindowContextData
    {
        u32 windowID{};

        bool isFramebufferDynamic = true;
        FramebufferSize fbSize = FramebufferSize::FB_1920_1080;

        VSyncState state = VSyncState::VSYNC_OFF;

#ifdef _WIN32
        uintptr_t context_window{};
#else
        uintptr_t context_display{};
        uintptr_t context_window{};
#endif

        VkSurfaceKHR context_vk_surface{};
    };

    class LIB_API WindowContext
    {
    public:
        static KalaGraphicsRegistry<WindowContext>& GetRegistry();

        //Sets the global vk instance
        static void SetVKInstance(VkInstance vk_instance);
        static VkInstance GetVKInstance();

        static bool IsValidWindowID(u32 windowID);

        static string_view GetFramebufferName(FramebufferSize fbSize);
        static vec2 GetFramebufferSize(FramebufferSize fbSize);

        //Initialize a new window context
        static WindowContext* Initialize(const WindowContextData& context);

        bool IsInitialized() const;

        u32 GetID() const;

        void SetVSyncState(VSyncState newValue);
        VSyncState GetVSyncState() const;

        //Regular update - single draw call
        void Update();

        //Called to trigger resize events - single draw call
        void ResizeUpdate();

        //If true then framebuffer resizes dynamically with the true window size
        void SetDynamicFramebufferState(bool newValue);
        bool IsDynamicFramebuffer() const;

        //Sets static framebuffer size, only applied if dynamic framebuffer is disabled
        void SetStaticFramebufferSize(FramebufferSize fbSize);
        vec2 GetStaticFramebufferSize() const;

        WindowContextData& GetWindowContextData();

        //Shuts down this window context cleanly and frees all its resources
        void Shutdown();
    private:
        bool isInitialized{};

        u32 ID{};

        WindowContextData context{};
    };
}