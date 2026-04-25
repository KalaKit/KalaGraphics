//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <vector>
#include <array>
#include <cstring>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan_core.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include "core_utils.hpp"
#include "log_utils.hpp"
#include "math_utils.hpp"

#include "_internal/_kg_vulkan.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec2;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::VSyncState;

using std::vector;
using std::array;
using std::to_string;
using std::unordered_map;
using std::clamp;

constexpr array<const char*, 2> deviceExtensions =
{
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1"
};

static bool isInitialized{};
static bool isVerboseLoggingEnabled{};

static u32 deviceCount{};
static vector<VkPhysicalDevice> devices{};

static VkPhysicalDevice physicalDevice{};
static VkDevice logicalDevice{};
static VkQueue graphicsQueue{};
static VkCommandPool commandPool{};
static VmaAllocator vmaAllocator{};
static VkDescriptorPool descriptorPool{};

static unordered_map<u32, VkExtent2D> extents{};
static unordered_map<u32, VkSwapchainKHR> swapchains{};
static unordered_map<u32, VkFormat> swapchainFormats{};
static unordered_map<u32, vector<VkImageView>> imageViews{};
static unordered_map<u32, VkRenderPass> renderPasses{};
static unordered_map<u32, VkImage> depthImages{};
static unordered_map<u32, VmaAllocation> depthAllocations{};
static unordered_map<u32, VkImageView> depthImageViews{};
static unordered_map<u32, vector<VkFramebuffer>> framebuffers{};
static unordered_map<u32, VkSemaphore> availableSemaphores{};
static unordered_map<u32, VkSemaphore> renderFinishedSemaphores{};
static unordered_map<u32, VkFence> inFlightFences{};
static unordered_map<u32, VkCommandBuffer> commandBuffers{};

static bool RecreateSwapchain(u32 windowContextID);

namespace KalaGraphics::Internal
{
    void Vulkan_Core::Initialize()
    {
        //
        // CHECK VERSION
        //

        u32 apiVersion = VK_API_VERSION_1_0;
        vkEnumerateInstanceVersion(&apiVersion);

        u32 major = VK_API_VERSION_MAJOR(apiVersion);
        u32 minor = VK_API_VERSION_MINOR(apiVersion);

        if (major < 1
            || (major == 1
            && minor < 3))
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan backend because the instance version was lower than minimum required version 1.3!");
        }

        VkInstance instance = WindowContext::GetVKInstance();

        if (!instance) exit(1);

        //
        // STORE DEVICE COUNT
        //

        if (vkEnumeratePhysicalDevices(
            instance, 
            &deviceCount, 
            nullptr) != VK_SUCCESS
            || deviceCount == 0)
        {
            KalaGraphicsCore::ForceClose(
            "Vulkan backend init error",
            "Failed to initialize Vulkan backend because the instance is invalid!");
        }

        devices.resize(deviceCount);

        vkEnumeratePhysicalDevices(
            instance, 
            &deviceCount, 
            devices.data());

        //
        // GET BEST DEVICE
        //

        VkPhysicalDevice bestDevice{};
        u32 bestScore{};

        for (const auto& device : devices)
        {
            //check required extensions

            u32 extCount{};
            vkEnumerateDeviceExtensionProperties(
                device,
                nullptr,
                &extCount,
                nullptr);

            vector<VkExtensionProperties> availableExts(extCount);
            vkEnumerateDeviceExtensionProperties(
                device,
                nullptr,
                &extCount,
                availableExts.data());

            bool hasAllExtensions = true;
            for (const auto& required : deviceExtensions)
            {
                bool found{};
                for (const auto& ext :availableExts)
                {
                    if (strcmp(ext.extensionName, required) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    hasAllExtensions = false;
                    break;
                }
            }
            if (!hasAllExtensions) continue;

            //check required features

            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(
                device,
                &features);

            if (!features.samplerAnisotropy
                || !features.fillModeNonSolid
                || !features.depthClamp
                || !features.sampleRateShading
                || !features.multiDrawIndirect)
            {
                continue;
            }

            //score by device type

            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(
                device,
                &props);

            u32 score{};
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 2;
            else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 1;
            else continue;

            if (score > bestScore)
            {
                bestScore = score;
                bestDevice = device;
            }
        }

        if (bestDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
            "Vulkan backend init error",
            "Failed to initialize Vulkan backend because no suitable physical device was found!");
        }

        physicalDevice = bestDevice;

        //
        // FIND QUEUE FAMILIES
        //

        u32 queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice, 
            &queueFamilyCount,
            nullptr);

        vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice,
            &queueFamilyCount,
            queueFamilies.data());

        u32 graphicsFamily = UINT32_MAX;
        for (u32 i = 0; i < queueFamilyCount; i++)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsFamily = i;
                break;
            }
        }

        if (graphicsFamily == UINT32_MAX)
        {
            KalaGraphicsCore::ForceClose(
            "Vulkan backend init error",
            "Failed to initialize Vulkan backend because no graphics queue family was found!");
        }

        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = graphicsFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures enabledFeatures{};
        enabledFeatures.samplerAnisotropy = VK_TRUE;
        enabledFeatures.fillModeNonSolid = VK_TRUE;
        enabledFeatures.depthClamp = VK_TRUE;
        enabledFeatures.sampleRateShading = VK_TRUE;
        enabledFeatures.multiDrawIndirect = VK_TRUE;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = scast<u32>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        deviceInfo.pEnabledFeatures = &enabledFeatures;

        //
        // CREATE LOGICAL DEVICE
        //

        if (vkCreateDevice(
            physicalDevice,
            &deviceInfo,
            nullptr,
            &logicalDevice) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
            "Vulkan backend init error",
            "Failed to initialize Vulkan backend because logical device creation failed!");
        }

        //
        // GET QUEUE HANDLE
        //

        vkGetDeviceQueue(
            logicalDevice,
            graphicsFamily,
            0,
            &graphicsQueue);

        //
        // CREATE COMMAND POOL
        //

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = graphicsFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(
            logicalDevice,
            &poolInfo,
            nullptr,
            &commandPool) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
            "Vulkan backend init error",
            "Failed to initialize Vulkan backend because command pool creation failed!");
        }

        //
        // CREATE VMA ALLOCATOR
        //

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = logicalDevice;
        allocatorInfo.instance = instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

        if (vmaCreateAllocator(
            &allocatorInfo, 
            &vmaAllocator) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
            "Vulkan backend init error",
            "Failed to initialize Vulkan backend because VMA allocator creation failed!");
        }

        //
        // CREATE DESCRIPTOR POOL
        //

        array<VkDescriptorPoolSize, 3> poolSizes{};
        
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1000;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 1000;

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = 1000;

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descPoolInfo.maxSets = 1000;
        descPoolInfo.poolSizeCount = scast<u32>(poolSizes.size());
        descPoolInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(
            logicalDevice,
            &descPoolInfo,
            nullptr,
            &descriptorPool) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
            "Vulkan backend init error",
            "Failed to initialize Vulkan backend because descriptor pool creation failed!");
        }

        //
        // SUCCESS
        //

        isInitialized = true;

        Log::Print(
            "Initialized Vulkan core!",
            "KG_VULKAN",
            LogType::LOG_SUCCESS);
    }

    bool Vulkan_Core::IsInitialized() { return isInitialized; }

    void Vulkan_Core::SetVerboseLoggingState(bool state) { isVerboseLoggingEnabled = state; }
    bool Vulkan_Core::IsVerboseLoggingEnabled() { return isVerboseLoggingEnabled; }

    VkPhysicalDevice Vulkan_Core::GetPhysicalDevice()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get physical device because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return physicalDevice;
    }
    VkDevice Vulkan_Core::GetLogicalDevice()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get logical device because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return logicalDevice;
    }
    VkQueue Vulkan_Core::GetGraphicsQueue()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get graphics queue because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return graphicsQueue;
    }
    VkCommandPool Vulkan_Core::GetCommandPool()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get command pool because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return commandPool;
    }
    VmaAllocator Vulkan_Core::GetVmaAllocator()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get VMA allocator because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return vmaAllocator;
    }
    VkDescriptorPool Vulkan_Core::GetDescriptorPool()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get descriptor pool because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return descriptorPool;
    }

    void Vulkan_Core::InitializeContext(u32 windowContextID)
    {
        WindowContext* context = WindowContext::GetRegistry().GetContent(windowContextID);

        if (!context)
        {
            Log::Print(
                "Failed to initialize Vulkan surface because window context ID '" + to_string(windowContextID) + "' was not found!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (swapchains.contains(windowContextID))
        {
            Log::Print(
                "Failed to initialize Vulkan surface because window context ID '" + to_string(windowContextID) + "' has already been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return;
        }

        VkSurfaceKHR surface = context->GetWindowContextData().context_vk_surface;

        //
        // CHECK SURFACE
        //

        bool surfaceSupported{};

        u32 queueCount{};
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice,
            &queueCount,
            nullptr);

        vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevice,
            &queueCount,
            queues.data());

        for (u32 i = 0; i < queueCount; i++)
        {
            VkBool32 supported{};

            if (vkGetPhysicalDeviceSurfaceSupportKHR(
                physicalDevice,
                i,
                surface,
                &supported) == VK_SUCCESS
                && supported)
            {
                surfaceSupported = true;
                break;
            }
        }

        if (!surfaceSupported)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan backend because the instance surface is not supported by the physical device!");
        }
        
        //
        // QUERY SURFACE
        //

        VkSurfaceCapabilitiesKHR capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, 
            surface,
            &capabilities);

        u32 formatCount{};
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice,
            surface,
            &formatCount,
            nullptr);

        vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice,
            surface,
            &formatCount,
            formats.data());

        VkSurfaceFormatKHR chosenFormat = formats[0];
        for (const auto& format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB
                && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = format;
                break;
            }
        }

        u32 presentModeCount{};
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice,
            surface,
            &presentModeCount,
            nullptr);

        vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice,
            surface,
            &presentModeCount,
            presentModes.data());

        VkExtent2D extent{};
        vec2 staticFramebufferSize = context->GetStaticFramebufferSize();

        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            extent.width = clamp(
                scast<u32>(staticFramebufferSize.x),
                capabilities.minImageExtent.width, 
                capabilities.maxImageExtent.width);
            extent.height = clamp(
                scast<u32>(staticFramebufferSize.y),
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);
        }

        u32 imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0
            && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        //
        // CREATE SWAPCHAIN
        //

        bool supportsMailbox = ContainsValue(presentModes, VK_PRESENT_MODE_MAILBOX_KHR);
        bool supportsFifoRelaxed = ContainsValue(presentModes, VK_PRESENT_MODE_FIFO_RELAXED_KHR);

        VkPresentModeKHR chosenPresentMode{};
        switch (context->GetVSyncState())
        {
        case VSyncState::VSYNC_ON_TRIPLE_BUFFERED:
        {
            if (supportsMailbox) chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            else if (supportsFifoRelaxed)
            {
                Log::Print(
                    "Tried to use MAILBOX but device does not support it, falling back to FIFO_RELAXED.",
                    "KG_VULKAN",
                    LogType::LOG_WARNING);

                chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
            else
            {
                Log::Print(
                    "Tried to use MAILBOX and FIFO_RELAXED but device does not support them, falling back to FIFO.",
                    "KG_VULKAN",
                    LogType::LOG_WARNING);

                chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
            break;
        }
        case VSyncState::VSYNC_ON_ADAPTIVE:
        {
            if (supportsFifoRelaxed) chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            else
            {
                Log::Print(
                    "Tried to use FIFO_RELAXED but device does not support it, falling back to FIFO.",
                    "KG_VULKAN",
                    LogType::LOG_WARNING);

                chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
            break;
        }
        case VSyncState::VSYNC_OFF:
        {
            chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        }
        default: break;
        }

        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = chosenFormat.format;
        swapchainInfo.imageColorSpace = chosenFormat.colorSpace;
        swapchainInfo.imageExtent = extent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = capabilities.currentTransform;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = chosenPresentMode;
        swapchainInfo.clipped = VK_TRUE;
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain{};
        if (vkCreateSwapchainKHR(
            logicalDevice,
            &swapchainInfo,
            nullptr,
            &swapchain) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because swapchain creation failed!");
        }

        swapchains[windowContextID] = swapchain;
        swapchainFormats[windowContextID] = chosenFormat.format;
        extents[windowContextID] = extent;

        //
        // GET SWAPCHAIN IMAGES 
        //

        u32 swapchainImageCount{};
        vkGetSwapchainImagesKHR(
            logicalDevice,
            swapchain,
            &swapchainImageCount,
            nullptr);

        vector<VkImage> swapchainImages(swapchainImageCount);
        vkGetSwapchainImagesKHR(
            logicalDevice,
            swapchain,
            &swapchainImageCount,
            swapchainImages.data());

        //
        // CREATE IMAGE VIEWS
        //

        vector<VkImageView> scImageViews(swapchainImageCount);
        for (u32 i = 0; i < swapchainImageCount; i++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = chosenFormat.format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(
                logicalDevice,
                &viewInfo,
                nullptr,
                &scImageViews[i]) != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "Vulkan backend init error",
                    "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because image view creation failed!");
            }
        }

        imageViews[windowContextID] = scImageViews;

        //
        // CREATE RENDER PASS
        //

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = chosenFormat.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        array<VkAttachmentDescription, 2> attachments =
        {
            colorAttachment,
            depthAttachment
        };

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = scast<u32>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkRenderPass renderPass{};
        if (vkCreateRenderPass(
            logicalDevice,
            &renderPassInfo,
            nullptr,
            &renderPass) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because render pass creation failed!");
        }

        renderPasses[windowContextID] = renderPass;

        //
        // CREATE DEPTH IMAGE
        //

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
        depthImageInfo.extent.width = extent.width;
        depthImageInfo.extent.height = extent.height;
        depthImageInfo.extent.depth = 1;
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo depthAllocInfo{};
        depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkImage depthImage{};
        VmaAllocation depthAllocation{};
        if (vmaCreateImage(
            vmaAllocator,
            &depthImageInfo,
            &depthAllocInfo,
            &depthImage,
            &depthAllocation,
            nullptr) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because depth image creation failed!");
        }

        //
        // CREATE DEPTH IMAGE VIEW
        //

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        VkImageView depthImageView{};
        if (vkCreateImageView(
            logicalDevice,
            &depthViewInfo,
            nullptr,
            &depthImageView) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because depth image view creation failed!");
        }

        depthImages[windowContextID] = depthImage;
        depthAllocations[windowContextID] = depthAllocation;
        depthImageViews[windowContextID] = depthImageView;

        //
        // CREATE FRAMEBUFFERS
        //

        vector<VkFramebuffer> newFramebuffers(swapchainImageCount);
        for (u32 i = 0; i < swapchainImageCount; i++)
        {
            array<VkImageView, 2> attachments = 
            {
                scImageViews[i],
                depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = scast<u32>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;
            
            if (vkCreateFramebuffer(
                logicalDevice,
                &framebufferInfo,
                nullptr,
                &newFramebuffers[i]) != VK_SUCCESS)
            {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because framebuffer creation failed for image " + to_string(i) + "!");
            }
        }

        framebuffers[windowContextID] = newFramebuffers;

        //
        // CREATE SYNC OBJECTS
        //

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphore availableSemaphore{};
        VkSemaphore renderFinishedSemaphore{};
        VkFence inFlightFence{};

        if (vkCreateSemaphore(
            logicalDevice,
            &semaphoreInfo,
            nullptr,
            &availableSemaphore) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because available semaphore creation failed!");
        }

        if (vkCreateSemaphore(
            logicalDevice,
            &semaphoreInfo,
            nullptr,
            &renderFinishedSemaphore) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because render finished semaphore creation failed!");
        }

        if (vkCreateFence(
            logicalDevice,
            &fenceInfo,
            nullptr,
            &inFlightFence) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because in flight fence creation failed!");
        }

        availableSemaphores[windowContextID] = availableSemaphore;
        renderFinishedSemaphores[windowContextID] = renderFinishedSemaphore;
        inFlightFences[windowContextID] = inFlightFence;

        //
        // ALLOCATE COMMAND BUFFER
        //

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer{};
        if (vkAllocateCommandBuffers(
            logicalDevice,
            &allocInfo,
            &commandBuffer) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan backend init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because command buffer allocation failed!");
        }

        commandBuffers[windowContextID] = commandBuffer;

        Log::Print(
            "Initialized Vulkan context!",
            "KG_VULKAN",
            LogType::LOG_SUCCESS);
    }

    VkSwapchainKHR Vulkan_Core::GetSwapchain(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get swapchain because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!swapchains.contains(windowContextID))
        {
            Log::Print(
                "Failed to get swapchain because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return swapchains[windowContextID];
    }
    vector<VkImageView> Vulkan_Core::GetImageViews(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get image views because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return {};
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get image views because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return {};
        }
        
        return imageViews[windowContextID];
    }
    VkRenderPass Vulkan_Core::GetRenderPass(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get render pass because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get render pass because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return renderPasses[windowContextID];
    }
    VkImage Vulkan_Core::GetDepthImage(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get depth image because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get depth image because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return depthImages[windowContextID];
    }
    VkImageView Vulkan_Core::GetDepthImageView(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get depth image view because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get depth image view because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return depthImageViews[windowContextID];
    }
    vector<VkFramebuffer> Vulkan_Core::GetFramebuffers(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get framebuffers because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return {};
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get framebuffers because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return {};
        }
        
        return framebuffers[windowContextID];
    }
    VkSemaphore Vulkan_Core::GetAvailableSemaphore(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get available semaphore because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get available semaphore because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return availableSemaphores[windowContextID];
    }
    VkSemaphore Vulkan_Core::GetRenderFinishedSemaphore(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get render finished semaphore because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get render finished semaphore because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return renderFinishedSemaphores[windowContextID];
    }
    VkFence Vulkan_Core::GetInFlightFence(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get in flight fence because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get in flight fence because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return inFlightFences[windowContextID];
    }
    VkCommandBuffer Vulkan_Core::GetCommandBuffer(u32 windowContextID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to get command buffer because Vulkan has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (!imageViews.contains(windowContextID))
        {
            Log::Print(
                "Failed to get command buffer because window context '" + to_string(windowContextID) + "' has not been initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        
        return commandBuffers[windowContextID];
    }

    bool Vulkan_Core::SetVSyncState(u32 windowID) { return RecreateSwapchain(windowID); }

    void Vulkan_Core::Update(u32 windowID)
    {
        if (!isInitialized)
        {
            Log::Print(
                "Failed to run update loop because Vulkan was not initialized!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (!swapchains.contains(windowID))
        {
            Log::Print(
                "Failed to run update loop because window '" + to_string(windowID) + "' was not found!",
                "KG_VULKAN",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vkWaitForFences(
            logicalDevice,
            1,
            &inFlightFences[windowID],
            VK_TRUE,
            UINT64_MAX);
        vkResetFences(
            logicalDevice,
            1,
            &inFlightFences[windowID]);

        u32 imageIndex{};
        vkAcquireNextImageKHR(
            logicalDevice,
            swapchains[windowID],
            UINT64_MAX,
            availableSemaphores[windowID],
            VK_NULL_HANDLE,
            &imageIndex);

        vkResetCommandBuffer(
            commandBuffers[windowID],
            0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(
            commandBuffers[windowID],
            &beginInfo);

        array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 1.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[windowID];
        renderPassInfo.framebuffer = framebuffers[windowID][imageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = extents[windowID];
        renderPassInfo.clearValueCount = scast<u32>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(
            commandBuffers[windowID],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(commandBuffers[windowID]);
        vkEndCommandBuffer(commandBuffers[windowID]);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &availableSemaphores[windowID];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[windowID];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphores[windowID];

        vkQueueSubmit(
            graphicsQueue,
            1,
            &submitInfo,
            inFlightFences[windowID]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[windowID];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchains[windowID];
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(
            graphicsQueue,
            &presentInfo);
    }

    void Vulkan_Core::ResizeUpdate(u32 windowContextID) { RecreateSwapchain(windowContextID); }

    void Vulkan_Core::Shutdown(u32 windowID)
    {
        auto clean_context = [](u32 windowID) -> void
            {
                vkFreeCommandBuffers(
                    logicalDevice,
                    commandPool,
                    1,
                    &commandBuffers[windowID]);

                vkDestroyFence(
                    logicalDevice,
                    inFlightFences[windowID],
                    nullptr);

                vkDestroySemaphore(
                    logicalDevice,
                    renderFinishedSemaphores[windowID],
                    nullptr);

                vkDestroySemaphore(
                    logicalDevice,
                    availableSemaphores[windowID],
                    nullptr);

                for (auto& fb : framebuffers[windowID])
                {
                    vkDestroyFramebuffer(
                        logicalDevice,
                        fb,
                        nullptr);
                }

                vkDestroyImageView(
                    logicalDevice,
                    depthImageViews[windowID],
                    nullptr);

                vmaDestroyImage(
                    vmaAllocator,
                    depthImages[windowID],
                    depthAllocations[windowID]);

                vkDestroyRenderPass(
                    logicalDevice,
                    renderPasses[windowID],
                    nullptr);

                for (auto& view : imageViews[windowID])
                {
                    vkDestroyImageView(
                        logicalDevice,
                        view,
                        nullptr);
                }

                vkDestroySwapchainKHR(
                    logicalDevice,
                    swapchains[windowID],
                    nullptr);
            };

        auto clean_all = [clean_context]() -> void
            {
                for (const auto& c : WindowContext::GetRegistry().runtimeContent)
                {
                    u32 ID = c->GetID();
                    if (!swapchains.contains(ID)) continue;

                    clean_context(ID);
                }

                if (descriptorPool)
                {
                    vkDestroyDescriptorPool(
                        logicalDevice,
                        descriptorPool,
                        nullptr);
                }
                if (vmaAllocator) vmaDestroyAllocator(vmaAllocator);
                if (commandPool)
                {
                    vkDestroyCommandPool(
                        logicalDevice, 
                        commandPool,
                        nullptr);
                }
                if (logicalDevice)
                {
                    vkDestroyDevice(
                        logicalDevice,
                        nullptr);
                }

                commandBuffers.clear();
                inFlightFences.clear();
                renderFinishedSemaphores.clear();
                availableSemaphores.clear();
                framebuffers.clear();
                depthImageViews.clear();
                depthAllocations.clear();
                depthImages.clear();
                renderPasses.clear();
                imageViews.clear();
                swapchains.clear();
                swapchainFormats.clear();
                extents.clear();
            };

        if (windowID == UINT32_MAX)
        {
            Log::Print(
                "Destroying all Vulkan context because no window ID was passed.",
                "KG_VULKAN",
                LogType::LOG_INFO);

            clean_all();
        }
        else
        {
            WindowContext* context = WindowContext::GetRegistry().GetContent(windowID);

            if (!context)
            {
                Log::Print(
                    "Failed to shut down Vulkan core for window context ID '" + to_string(windowID) + "' because it was not found!",
                    "KG_VULKAN",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            if (WindowContext::GetRegistry().runtimeContent.size() == 1)
            {
                Log::Print(
                    "Destroying all Vulkan context because only one window exists.",
                    "KG_VULKAN",
                    LogType::LOG_INFO);

                clean_all();
                return;
            }

            //drain the gpu before freeing its context resources
            vkDeviceWaitIdle(logicalDevice);

            if (!swapchains.contains(windowID))
            {
                Log::Print(
                    "Failed to shut down Vulkan core for window context ID '" + to_string(windowID) + "' because it was not found!",
                    "KG_VULKAN",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            Log::Print(
                "Destroying Vulkan content for window '" + to_string(windowID) + "'.",
                "KG_VULKAN",
                LogType::LOG_INFO);

            clean_context(windowID);

            //also remove content for target window from maps

            commandBuffers.erase(windowID);
            inFlightFences.erase(windowID);
            renderFinishedSemaphores.erase(windowID);
            availableSemaphores.erase(windowID);
            framebuffers.erase(windowID);
            depthImageViews.erase(windowID);
            depthAllocations.erase(windowID);
            depthImages.erase(windowID);
            renderPasses.erase(windowID);
            imageViews.erase(windowID);
            swapchains.erase(windowID);
            swapchainFormats.erase(windowID);
            extents.erase(windowID);
        }
    }
}

bool RecreateSwapchain(u32 windowContextID)
{
    WindowContext* context = WindowContext::GetRegistry().GetContent(windowContextID);
    VkSurfaceKHR surface = context->GetWindowContextData().context_vk_surface;

    if (!isInitialized)
    {
        Log::Print(
            "Failed to recreate swapchain because Vulkan was not initialized!",
            "KG_VULKAN",
            LogType::LOG_ERROR,
            2);

        return false;
    }
    if (!swapchains.contains(windowContextID))
    {
        Log::Print(
            "Failed to recreate swapchain because window '" + to_string(windowContextID) + "' was not found!",
            "KG_VULKAN",
            LogType::LOG_ERROR,
            2);

        return false;
    }

    //drain the gpu before freeing its context resources
    vkDeviceWaitIdle(logicalDevice);

    //
    // DESTROY
    //

    auto destroy_semaphores = [&windowContextID]() -> void
        {
            vkDestroySemaphore(
                logicalDevice,
                availableSemaphores[windowContextID],
                nullptr);

            availableSemaphores.erase(windowContextID);
        };
    auto destroy_framebuffers = [&windowContextID]() -> void
        {
            for (auto& fb : framebuffers[windowContextID])
            {
                vkDestroyFramebuffer(
                    logicalDevice,
                    fb,
                    nullptr);
            }

            framebuffers.erase(windowContextID);
        };
    auto destroy_depth_image_views = [&windowContextID]() -> void
        {
            vkDestroyImageView(
                logicalDevice,
                depthImageViews[windowContextID],
                nullptr);
            vmaDestroyImage(
                vmaAllocator,
                depthImages[windowContextID],
                depthAllocations[windowContextID]);

            depthImageViews.erase(windowContextID);
            depthAllocations.erase(windowContextID);
            depthImages.erase(windowContextID);
        };
    auto destroy_image_views = [&windowContextID]() -> void
        {
            for (auto& view : imageViews[windowContextID])
            {
                vkDestroyImageView(
                    logicalDevice,
                    view,
                    nullptr);
            }

            imageViews.erase(windowContextID);
        };
    auto destroy_swapchain = [&windowContextID]() -> void
        {
            vkDestroySwapchainKHR(
                logicalDevice,
                swapchains[windowContextID],
                nullptr);

            swapchains.erase(windowContextID);
            swapchainFormats.erase(windowContextID);
            extents.erase(windowContextID);
        };

    destroy_semaphores();
    destroy_framebuffers();
    destroy_depth_image_views();
    destroy_image_views();
    destroy_swapchain();

    //
    // QUERY SURFACE
    //

    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, 
        surface,
        &capabilities);

    u32 formatCount{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice,
        surface,
        &formatCount,
        nullptr);

    vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice,
        surface,
        &formatCount,
        formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosenFormat = format;
            break;
        }
    }

    u32 presentModeCount{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice,
        surface,
        &presentModeCount,
        nullptr);

    vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice,
        surface,
        &presentModeCount,
        presentModes.data());

    VkExtent2D extent{};
    vec2 staticFramebufferSize = context->GetStaticFramebufferSize();

    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        extent = capabilities.currentExtent;
    }
    else
    {
        extent.width = clamp(
            scast<u32>(staticFramebufferSize.x),
            capabilities.minImageExtent.width, 
            capabilities.maxImageExtent.width);
        extent.height = clamp(
            scast<u32>(staticFramebufferSize.y),
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }

    u32 imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0
        && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    //
    // CREATE SWAPCHAIN
    //

    bool supportsMailbox = ContainsValue(presentModes, VK_PRESENT_MODE_MAILBOX_KHR);
    bool supportsFifoRelaxed = ContainsValue(presentModes, VK_PRESENT_MODE_FIFO_RELAXED_KHR);

    VkPresentModeKHR chosenPresentMode{};
    switch (context->GetVSyncState())
    {
    case VSyncState::VSYNC_ON_TRIPLE_BUFFERED:
    {
        if (supportsMailbox) chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (supportsFifoRelaxed)
        {
            Log::Print(
                "Tried to use MAILBOX but device does not support it, falling back to FIFO_RELAXED.",
                "KG_VULKAN",
                LogType::LOG_WARNING);

            chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }
        else
        {
            Log::Print(
                "Tried to use MAILBOX and FIFO_RELAXED but device does not support them, falling back to FIFO.",
                "KG_VULKAN",
                LogType::LOG_WARNING);

            chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
        break;
    }
    case VSyncState::VSYNC_ON_ADAPTIVE:
    {
        if (supportsFifoRelaxed) chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        else
        {
            Log::Print(
                "Tried to use FIFO_RELAXED but device does not support it, falling back to FIFO.",
                "KG_VULKAN",
                LogType::LOG_WARNING);

            chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
        break;
    }
    case VSyncState::VSYNC_OFF:
    {
        chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        break;
    }
    default: break;
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = context->GetWindowContextData().context_vk_surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = chosenFormat.format;
    swapchainInfo.imageColorSpace = chosenFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = chosenPresentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain{};
    if (vkCreateSwapchainKHR(
        logicalDevice,
        &swapchainInfo,
        nullptr,
        &swapchain) != VK_SUCCESS)
    {
        KalaGraphicsCore::ForceClose(
            "Vulkan swapchain error",
            "Failed to recreate Vulkan swapchain for window context '" + to_string(context->GetID()) + "' because swapchain creation failed!");
    }

    swapchains[windowContextID] = swapchain;
    swapchainFormats[windowContextID] = chosenFormat.format;
    extents[windowContextID] = extent;

    //
    // GET SWAPCHAIN IMAGES 
    //

    u32 swapchainImageCount{};
    vkGetSwapchainImagesKHR(
        logicalDevice,
        swapchain,
        &swapchainImageCount,
        nullptr);

    vector<VkImage> swapchainImages(swapchainImageCount);
    vkGetSwapchainImagesKHR(
        logicalDevice,
        swapchain,
        &swapchainImageCount,
        swapchainImages.data());

    //
    // CREATE IMAGE VIEWS
    //

    vector<VkImageView> scImageViews(swapchainImageCount);
    for (u32 i = 0; i < swapchainImageCount; i++)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chosenFormat.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(
            logicalDevice,
            &viewInfo,
            nullptr,
            &scImageViews[i]) != VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan swapchain error",
                "Failed to recreate Vulkan swapchain for window context '" + to_string(context->GetID()) + "' because image view creation failed!");
        }
    }

    imageViews[windowContextID] = scImageViews;

    //
    // CREATE DEPTH IMAGE
    //

    VkImageCreateInfo depthImageInfo{};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageInfo.extent.width = extent.width;
    depthImageInfo.extent.height = extent.height;
    depthImageInfo.extent.depth = 1;
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depthAllocInfo{};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage depthImage{};
    VmaAllocation depthAllocation{};
    if (vmaCreateImage(
        vmaAllocator,
        &depthImageInfo,
        &depthAllocInfo,
        &depthImage,
        &depthAllocation,
        nullptr) != VK_SUCCESS)
    {
        KalaGraphicsCore::ForceClose(
            "Vulkan swapchain error",
            "Failed to recreate Vulkan swapchain for window context '" + to_string(context->GetID()) + "' because depth image creation failed!");
    }

    //
    // CREATE DEPTH IMAGE VIEW
    //

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    VkImageView depthImageView{};
    if (vkCreateImageView(
        logicalDevice,
        &depthViewInfo,
        nullptr,
        &depthImageView) != VK_SUCCESS)
    {
        KalaGraphicsCore::ForceClose(
            "Vulkan swapchain error",
            "Failed to recreate Vulkan swapchain for window context '" + to_string(context->GetID()) + "' because depth image view creation failed!");
    }

    depthImages[windowContextID] = depthImage;
    depthAllocations[windowContextID] = depthAllocation;
    depthImageViews[windowContextID] = depthImageView;

    //
    // CREATE FRAMEBUFFERS
    //

    vector<VkFramebuffer> newFramebuffers(swapchainImageCount);
    for (u32 i = 0; i < swapchainImageCount; i++)
    {
        array<VkImageView, 2> attachments = 
        {
            scImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPasses[windowContextID];
        framebufferInfo.attachmentCount = scast<u32>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;
        
        if (vkCreateFramebuffer(
            logicalDevice,
            &framebufferInfo,
            nullptr,
            &newFramebuffers[i]) != VK_SUCCESS)
        {
        KalaGraphicsCore::ForceClose(
            "Vulkan swapchain error",
            "Failed to recreate Vulkan swapchain for window context '" + to_string(context->GetID()) + "' because framebuffer creation failed for image " + to_string(i) + "!");
        }
    }

    framebuffers[windowContextID] = newFramebuffers;

    //
    // CREATE SEMAPHORES
    //

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore availableSemaphore{};
    if (vkCreateSemaphore(
        logicalDevice,
        &semaphoreInfo,
        nullptr,
        &availableSemaphore) != VK_SUCCESS)
    {
        KalaGraphicsCore::ForceClose(
            "Vulkan swapchain error",
            "Failed to recreate Vulkan swapchain for window context '" + to_string(context->GetID()) + "' because semaphore creation failed!");
    }

    availableSemaphores[windowContextID] = availableSemaphore;

    if (isVerboseLoggingEnabled)
    {
        Log::Print(
            "Initialized Vulkan context!",
            "KG_VULKAN",
            LogType::LOG_VERBOSE);
    }

    return true;
}