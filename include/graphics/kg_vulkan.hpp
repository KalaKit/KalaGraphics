//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

#include "core_utils.hpp"

#include "core/kg_context.hpp"

struct VkPhysicalDevice_T;
using VkPhysicalDevice = VkPhysicalDevice_T*;

struct VkDevice_T;
using VkDevice = VkDevice_T*;

struct VkQueue_T;
using VkQueue = VkQueue_T*;

struct VkCommandPool_T;
using VkCommandPool = VkCommandPool_T*;

struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

struct VkDescriptorPool_T;
using VkDescriptorPool = VkDescriptorPool_T*;

struct VkSwapchainKHR_T;
using VkSwapchainKHR = VkSwapchainKHR_T*;

struct VkImage_T;
using VkImage = VkImage_T*;

struct VkImageView_T;
using VkImageView = VkImageView_T*;

struct VkRenderPass_T;
using VkRenderPass = VkRenderPass_T*;

struct VkFramebuffer_T;
using VkFramebuffer = VkFramebuffer_T*;

struct VkSemaphore_T;
using VkSemaphore = VkSemaphore_T*;

struct VkFence_T;
using VkFence = VkFence_T*;

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;

namespace KalaGraphics::Graphics
{
    using std::vector;
    using std::string;
    using std::string_view;

    using u32 = uint32_t;

    using KalaGraphics::Core::VSyncState;

    enum class Severity : u8
    {
        S_INVALID = 0u,

        S_INFO = 1u,
        S_WARNING = 2u,
        S_FATAL = 3u
    };

    class LIB_API Vulkan_Core
    {
    public:
        //Global Vulkan instance-based initialization
        static void Initialize();

        //Is the global Vulkan instance content initialized
        static bool IsInitialized();

        static void SetVerboseLoggingState(bool state);
        static bool IsVerboseLoggingEnabled();

        static string GetVkResultMessage(int result);
        static Severity GetVkResultSeverity(int result);

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
        static void InitializeContext(u32 windowContextID);

        static VkSwapchainKHR GetSwapchain(u32 windowContextID);
        static vector<VkImageView> GetImageViews(u32 windowContextID);
        static VkRenderPass GetRenderPass(u32 windowContextID);
        static VkImage GetDepthImage(u32 windowContextID);
        static VkImageView GetDepthImageView(u32 windowContextID);
        static vector<VkFramebuffer> GetFramebuffers(u32 windowContextID);
        static VkSemaphore GetAvailableSemaphore(u32 windowContextID);
        static VkSemaphore GetRenderFinishedSemaphore(u32 windowContextID);
        static VkFence GetInFlightFence(u32 windowContextID);
        static VkCommandBuffer GetCommandBuffer(u32 windowContextID);

        static bool SetVSyncState(u32 windowContextID);

        //Main draw call
        static void Update(u32 windowID);
        //Actions that occur only when the window size changes
        static void ResizeUpdate(u32 windowID);

        //Clean all resources
        static void Shutdown(u32 windowID = UINT32_MAX);
    };
}