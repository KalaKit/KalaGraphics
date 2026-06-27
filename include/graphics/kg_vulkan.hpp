//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <array>
#include <string>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

#include "core_utils.hpp"

#include "core/kg_context.hpp"
#include "core/kg_registry.hpp"

namespace KalaGraphics::Graphics
{
    constexpr u8 MAX_FRAMES_IN_FLIGHT = 2;

    using KalaGraphics::Core::KalaGraphicsRegistry;
    using KalaGraphics::Core::VSyncState;

    using std::vector;
    using std::array;
    using std::string;
    using std::string_view;

    using u32 = uint32_t;

    enum class Severity : u8
    {
        S_INVALID = 0u,

        S_INFO = 1u,
        S_WARNING = 2u,
        S_FATAL = 3u
    };

    class LIB_API VulkanContext
    {
    public:
        static KalaGraphicsRegistry<VulkanContext>& GetRegistry();

        //Global Vulkan instance-based initialization
        static void Initialize();

        //Is the global Vulkan instance content initialized
        static bool IsInitialized();

        static bool IsVerboseLoggingEnabled();
        static void SetVerboseLoggingState(bool state);

        static string GetVkResultMessage(int result);
        static Severity GetVkResultSeverity(int result);

        //Close the program, this close function is useful for
        //printing the VkResult error type that occured so it can be logged
        static void CloseOnError(
            string_view title,
            string_view reason,
            int result);

        static VkPhysicalDevice GetPhysicalDevice();
        static VkDevice GetLogicalDevice();
        static VkQueue GetGraphicsQueue();
        static VkCommandPool GetCommandPool();
        static VmaAllocator GetVmaAllocator();
        static VkDescriptorPool GetDescriptorPool();

        //Per-surface initialization
        static VulkanContext* InitializeContext(u32 graphicsContextID);

        u32 GetID() const;
        u32 GetGraphicsContextID() const;

        VkSwapchainKHR& GetSwapchain();
        vector<VkImageView>& GetImageViews();
        VkRenderPass& GetRenderPass();
        VkImage& GetDepthImage();
        VkImageView& GetDepthImageView();
        vector<VkFramebuffer>& GetFramebuffers();
        array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>& GetAvailableSemaphores();
        vector<VkSemaphore>& GetRenderFinishedSemaphores();
        array<VkFence, MAX_FRAMES_IN_FLIGHT>& GetInFlightFences();
        array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT>& GetCommandBuffers();

        bool SetVSyncState();

        //Main draw call
        void Update();
        //Actions that occur only when the window size changes
        void ResizeUpdate();

        //Recreates the Vulkan swapchain and its related content, useful for resize events etc
        bool RecreateSwapchain();

        void Destroy();

        ~VulkanContext();
    private:
        u32 ID{};
        u32 graphicsContextID{};

        size_t currentFrame{};

        VkExtent2D extent{};
        VkSwapchainKHR swapchain{};
        VkFormat swapchainFormat{};
        vector<VkFence> imagesInFlight{};
        vector<VkImageView> imageViews{};
        VkRenderPass renderPass{};
        VkImage depthImage{};
        VmaAllocation depthAllocation{};
        VkImageView depthImageView{};
        vector<VkFramebuffer> framebuffers{};
        array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> availableSemaphores{};
        vector<VkSemaphore> renderFinishedSemaphores{};
        array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
        array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
    };
}