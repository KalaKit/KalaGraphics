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

    enum class ViewportSize : u8
    {
        VP_INVALID = 0u,

        //4:3

        VP_640_480 = 1u,
        VP_800_600 = 2u,
        VP_1024_768 = 3u,
        VP_1600_1200 = 4u,

        //16:9

        VP_1280_720 = 5u,
        VP_1600_900 = 6u,
        VP_1920_1080 = 7u,
        VP_2560_1440 = 8u,
        VP_3840_2160 = 9u,

        //16:10

        VP_1280_800 = 10u,
        VP_1680_1050 = 11u,
        VP_1920_1200 = 12u,
        VP_2560_1600 = 13u,

        //21:9

        VP_2560_1080 = 14u,
        VP_3440_1440 = 15u,
        VP_5120_2160 = 16u,

        //32:9

        VP_3840_1080 = 17u,
        VP_5120_1440 = 18u
    };

    enum class VSyncState : u8
	{
        VSYNC_INVALID = 0u,

        //Lowest latency, no tearing (mailbox, fifo_relaxed/fifo as fallback)
        VSYNC_ON_TRIPLE_BUFFERED = 1u,

        //Frames synced to display refresh rate to prevent stuttering,
        //can cause tearing (fifo_relaxed, fifo as fallback)
        VSYNC_ON_ADAPTIVE = 2u,

        //Uncapped framerates, no waiting, causes tearing (immediate)
        VSYNC_OFF = 3u
	};

    struct LIB_API GraphicsContextData
    {
        u32 windowID{};

#ifdef _WIN32
        uintptr_t context_window{};
#else
        uintptr_t context_display{};
        uintptr_t context_window{};
#endif

        VkSurfaceKHR context_vk_surface{};
    };

    struct LIB_API ViewportData
    {
        //does the viewport dynamically scale with the 
        bool isDynamicViewport = true;

        //where the viewport starts at, relative to top-left corner
        vec2 offset{};

        //min and max depth
        vec2 depth = vec2(0, 1);

        //pushes the drawable area down and right if x and y are positive
        vec2 viewportOffset{};

        //cuts everything outside of this area,
        //gpu can only draw clear color there
        vec2 scissorSize{};

        //static viewport size
        ViewportSize vpSize = ViewportSize::VP_1920_1080;
    };

    class LIB_API GraphicsContext
    {
    public:
        static KalaGraphicsRegistry<GraphicsContext>& GetRegistry();

        //Sets the global vk instance
        static void SetVKInstance(VkInstance vk_instance);
        static VkInstance GetVKInstance();

        //Initialize a new window context
        static GraphicsContext* Initialize(const GraphicsContextData& context);

        u32 GetID() const;
        u32 GetVulkanContextID() const;

        VSyncState GetVSyncState() const;
        void SetVSyncState(VSyncState newValue);

        static string_view GetStaticViewportName(ViewportSize vpSize);
        static vec2 GetStaticViewportSizeValue(ViewportSize vpSize);

        //Sets static viewport size, only applied if dynamic viewport is disabled
        vec2 GetStaticViewportSize() const;
        void SetStaticViewportSize(ViewportSize fbSize);

        //If true then viewport resizes dynamically with the true window size
        bool IsDynamicViewport() const;
        void SetDynamicViewportState(bool newValue);

        vec2 GetDepth() const;
        void SetDepth(vec2 newDepth);

        vec2 GetViewportOffset() const;
        void SetViewportOffset(vec2 newSize);

        vec2 GetScissorSize() const;
        void SetScissorSize(vec2 newOffset);
    
        const GraphicsContextData& GetGraphicsContextData() const;

        //Regular update - single draw call
        void Update();

        //Called to trigger resize events - single draw call
        void ResizeUpdate();

        void Destroy();

        ~GraphicsContext();
    private:
        u32 ID{};
        u32 vulkanContextID{};

        VSyncState vsyncState = VSyncState::VSYNC_ON_TRIPLE_BUFFERED;

        GraphicsContextData contextData{};
        ViewportData vpData{};
    };
}