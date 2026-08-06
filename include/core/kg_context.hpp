//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <array>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

struct VkInstance_T;
using VkInstance = VkInstance_T*;

struct VkSurfaceKHR_T;
using VkSurfaceKHR = VkSurfaceKHR_T*;

struct VkPhysicalDevice_T;
using VkPhysicalDevice = VkPhysicalDevice_T*;

struct VkDevice_T;
using VkDevice = VkDevice_T*;

struct VkQueue_T;
using VkQueue = VkQueue_T*;

struct VkDescriptorPool_T;
using VkDescriptorPool = VkDescriptorPool_T*;

struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

struct VkSwapchainKHR_T;
using VkSwapchainKHR = VkSwapchainKHR_T*;

struct VkRenderPass_T;
using VkRenderPass = VkRenderPass_T*;

struct VkImage_T;
using VkImage = VkImage_T*;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

struct VkImageView_T;
using VkImageView = VkImageView_T*;

struct VkFramebuffer_T;
using VkFramebuffer = VkFramebuffer_T*;

struct VkSemaphore_T;
using VkSemaphore = VkSemaphore_T*;

struct VkFence_T;
using VkFence = VkFence_T*;

struct VkCommandPool_T;
using VkCommandPool = VkCommandPool_T*;

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;

namespace KalaGraphics::Resources
{
    class Mesh;
    class Camera;
    class Shader;
}

namespace KalaGraphics::Core
{
    constexpr u8 MAX_FRAMES_IN_FLIGHT = 2;

    using KalaHeaders::KalaMath::vec2;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::array;

    using u8 = uint8_t;
    using u32 = uint32_t;

    enum class Severity : u8
    {
        S_INVALID = 0u,

        S_INFO = 1u,
        S_WARNING = 2u,
        S_FATAL = 3u
    };

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
    friend class KalaGraphics::Resources::Mesh;
    friend class KalaGraphics::Resources::Camera;
    friend class KalaGraphics::Resources::Shader;
    public:
        static KalaGraphicsRegistry<GraphicsContext>& GetRegistry();

        //Close the program, this close function is useful for
        //printing the VkResult error type that occured so it can be logged
        static void ForceClose(
            string&& title,
            string&& reason,
            int result);

        static bool IsVerboseLoggingEnabled();
        static void SetVerboseLoggingState(bool state);

        static string GetVkResultMessage(int result);
        static Severity GetVkResultSeverity(int result);

        static VkInstance GetVKInstance();
        static void SetVKInstance(VkInstance vk_instance);

        static VkPhysicalDevice GetPhysicalDevice();
        static VkDevice GetLogicalDevice();
        static VkQueue GetGraphicsQueue();
        static VmaAllocator GetVmaAllocator();
        static VkDescriptorPool GetDescriptorPool();

        //Global one-time Vulkan 1.4 device init,
        //needs to be called before per-window Vulkan init
        static void Initialize();
        static bool IsInitialized();

        //Single draw call for all existing contexts,
        //handles all active meshes, light sources and cameras
        static void Update();

        //Initialize a per-window Vulkan context, creates the swapchain logic
        static GraphicsContext* InitializeInstance(GraphicsContextData&& context);

        u32 GetID() const;
        const vector<u32>& GetShaderIDs() const;
        const vector<u32>& GetCameraIDs() const;

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

        vec2 GetExtent();
        VkSwapchainKHR& GetSwapchain();
        vector<VkImageView>& GetImageViews();
        VkRenderPass& GetRenderPass();
        VkImage& GetDepthImage();
        VkImageView& GetDepthImageView();
        vector<VkFramebuffer>& GetFramebuffers();
        array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>& GetAvailableSemaphores();
        vector<VkSemaphore>& GetRenderFinishedSemaphores();
        VkCommandPool& GetCommandPool();
        array<VkFence, MAX_FRAMES_IN_FLIGHT>& GetInFlightFences();
        array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT>& GetCommandBuffers();

        //Called to trigger resize events
        void ResizeUpdate();

        //Recreates the Vulkan swapchain and its related content, useful for resize events etc
        void RecreateSwapchain();

        void Destroy();

        ~GraphicsContext();
    private:
        void InitializeVulkanContext();

        void UpdateInstance();

        u32 ID{};

        //shaders that use this graphics context
        vector<u32> shaderIDs{};
        //cameras that use this graphics context
        vector<u32> cameraIDs{};

        u8 missingShaderWarningCount{};

        VSyncState vsyncState = VSyncState::VSYNC_ON_TRIPLE_BUFFERED;

        GraphicsContextData contextData{};
        ViewportData vpData{};

        size_t currentFrame{};

        vec2 extent{};
        VkSwapchainKHR swapchain{};
        u32 swapchainFormat{};
        vector<VkFence> imagesInFlight{};
        vector<VkImageView> imageViews{};
        VkRenderPass renderPass{};
        VkImage depthImage{};
        VmaAllocation depthAllocation{};
        VkImageView depthImageView{};
        vector<VkFramebuffer> framebuffers{};
        array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> availableSemaphores{};
        vector<VkSemaphore> renderFinishedSemaphores{};
        VkCommandPool commandPool{};
        array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
        array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
    };
}