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
    class Shader;
    class Texture;
    class Mesh;
    class Camera;
}

namespace KalaGraphics::Core
{
    using KalaHeaders::KalaMath::vec2;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::array;
    using std::default_delete;

    static constexpr u8 MAX_FRAMES_IN_FLIGHT = 2;

    //Max total descriptor sets this pool can allocate at once,
    //shared across every Mesh, Camera and Texture descriptor set
    static constexpr u32 MAX_DESCRIPTOR_SETS = 4096;

    //Mesh matrix UBO + camera matrix UBO
    static constexpr u32 MAX_UNIFORM_BUFFER_DESCRIPTORS = 2048;
    //Texture sampler bindings
    static constexpr u32 MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTORS = 2048;
    //For future compute/SSBO-based features
    static constexpr u32 MAX_STORAGE_BUFFER_DESCRIPTORS = 2048;
    //For future bindless-style setups
    static constexpr u32 MAX_SAMPLER_DESCRIPTORS = 64;

    enum class Severity : u8
    {
        SEVERITY_INFO = 0,
        SEVERITY_WARNING = 1,
        SEVERITY_FATAL = 2
    };

    enum class VSyncState : u8
	{
        //Lowest latency, no tearing (mailbox, fifo_relaxed/fifo as fallback)
        VSYNC_ON_TRIPLE_BUFFERED = 0,

        //Frames synced to display refresh rate to prevent stuttering,
        //can cause tearing (fifo_relaxed, fifo as fallback)
        VSYNC_ON_ADAPTIVE = 1,

        //Uncapped framerates, no waiting, causes tearing (immediate)
        VSYNC_OFF = 2
	};

    struct LIB_API GraphicsContextData
    {
        u32 windowID{};

#if defined(KWIN_ANY)
        uintptr_t context_window{};
#elif defined(KLIN_ANY)
        uintptr_t context_display{};
        uintptr_t context_window{};
#endif

        VkSurfaceKHR context_vk_surface{};
    };

    class LIB_API GraphicsContext
    {
    friend class KalaGraphics::Resources::Shader;
    friend class KalaGraphics::Resources::Mesh;
    friend class KalaGraphics::Resources::Texture;
    friend class KalaGraphics::Resources::Camera;
    friend class Viewport;
    friend struct default_delete<GraphicsContext>;
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

        //Global one-time Vulkan 1.4 device init,
        //needs to be called before per-window Vulkan init
        static void Initialize(VkInstance vkInstance);
        static bool IsInitialized();

        //Single draw call for all existing contexts,
        //handles all active meshes, light sources and cameras
        static void Update();

        //Initialize a per-window Vulkan context, creates the swapchain logic
        static GraphicsContext* InitializeInstance(GraphicsContextData&& context);

        u32 GetID() const;
        const vector<u32>& GetViewportIDs() const;

        VSyncState GetVSyncState() const;
        void SetVSyncState(VSyncState newValue);

        //Get current Windows/X11 window true window size,
        //this is also used as VkExtent
        vec2 GetRenderSize() const;
    
        const GraphicsContextData& GetGraphicsContextData() const;

        void Destroy();
    private:
        ~GraphicsContext();

        static VkInstance GetInstance();

        static VkPhysicalDevice GetPhysicalDevice();
        static VkDevice GetLogicalDevice();
        static VmaAllocator GetVmaAllocator();
        static VkDescriptorPool GetDescriptorPool();        

        static u32 GetDefaultColorFormat();
        static u32 GetDefaultDepthFormat();

        void InitializeVulkanContext();

        //Create and use a single time command buffer for a small batch of operations
        VkCommandBuffer BeginSingleTimeCommands();
        //Destroy and stop using the created command buffer
        void EndSingleTimeCommands(VkCommandBuffer vkCommandBuffer);

        void UpdateInstance();

        void RecreateSwapchain();

        u32 ID{};

        //viewports that use this graphics context
        vector<u32> viewportIDs{};

        u8 missingShaderWarningCount{};

        VSyncState vsyncState = VSyncState::VSYNC_ON_TRIPLE_BUFFERED;

        GraphicsContextData contextData{};

        size_t currentFrame{};

        vec2 renderSize{};

        VkSwapchainKHR swapchain{};
        u32 swapchainFormat{};
        vector<VkFence> swapchainImagesInFlight{};
        vector<VkImage> swapchainImages{};
        vector<VkImageView> swapchainImageViews{};
        VkImage depthImage{};
        VmaAllocation depthAllocation{};
        VkImageView depthImageView{};
        array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> availableSemaphores{};
        vector<VkSemaphore> renderFinishedSemaphores{};
        VkCommandPool commandPool{};
        array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
        array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
    };
}