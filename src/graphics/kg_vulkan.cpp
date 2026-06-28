//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <cstdint>
#include <vector>
#include <array>
#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>

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

#include "graphics/kg_vulkan.hpp"
#include "graphics/kg_shader.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaCore::ContainsValue;
using KalaHeaders::KalaCore::EnumHash;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec2;

using KalaGraphics::Graphics::VulkanContext;
using KalaGraphics::Graphics::Severity;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::ViewportData;
using KalaGraphics::Core::VSyncState;

using std::vector;
using std::array;
using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;
using std::clamp;
using std::unique_ptr;
using std::make_unique;

constexpr array<const char*, 2> deviceExtensions =
{
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1"
};

static bool isInitialized{};
static bool isVerboseLoggingEnabled{};

static u32 deviceCount{};
static u32 graphicsFamily = UINT32_MAX;
static vector<VkPhysicalDevice> devices{};

static VkPhysicalDevice physicalDevice{};
static VkDevice logicalDevice{};
static VkQueue graphicsQueue{};
static VmaAllocator vmaAllocator{};
static VkDescriptorPool descriptorPool{};

struct VkResultData
{
    string message{};
    Severity severity{};
};

static unordered_map<VkResult, VkResultData, EnumHash<VkResult>> vkResultData
{
    { VK_SUCCESS,                     { "Operation successful",           Severity::S_INFO } },
    //instead of closing - wait or retry operation
    { VK_NOT_READY,                   { "Resource not ready",             Severity::S_WARNING } },
    //instead of closing - wait or retry operation
    { VK_TIMEOUT,                     { "Operation timed out",            Severity::S_WARNING } },
    { VK_EVENT_SET,                   { "Event signaled",                 Severity::S_INFO } },
    { VK_EVENT_RESET,                 { "Event reset",                    Severity::S_INFO } },
    //instead of closing - ignore or retry operation
    { VK_INCOMPLETE,                  { "Operation incomplete",           Severity::S_WARNING } },
    //close reason - no recovery possible
    { VK_ERROR_OUT_OF_HOST_MEMORY,    { "System memory exhausted",        Severity::S_FATAL } },
    //close reason - no recovery possible
    { VK_ERROR_OUT_OF_DEVICE_MEMORY,  { "Device memory (VRAM) exhausted", Severity::S_FATAL } },
    //close reason - driver crash or corrupted system files
    { VK_ERROR_INITIALIZATION_FAILED, { "Initialization failed",          Severity::S_FATAL } },
    //close reason - hardware connection is severed or unstable, cannot render anymore
    { VK_ERROR_DEVICE_LOST,           { "Device lost",                    Severity::S_FATAL } },
    //instead of closing - retry mapping or switch to a different memory type
    { VK_ERROR_MEMORY_MAP_FAILED,     { "Memory mapping failed",          Severity::S_WARNING } },
    //close reason - critical logic error
    { VK_ERROR_LAYER_NOT_PRESENT,     { "Required layer missing",         Severity::S_FATAL } },
    //close reason - critical logic error
    { VK_ERROR_EXTENSION_NOT_PRESENT, { "Required extension missing",     Severity::S_FATAL } },
    //close reason - critical logic error
    { VK_ERROR_FEATURE_NOT_PRESENT,   { "Required feature unsupported",   Severity::S_FATAL } },
    //close reason - critical logic error
    { VK_ERROR_INCOMPATIBLE_DRIVER,   { "Incompatible driver version",    Severity::S_FATAL } },
    //instead of closing - prevent creating new objects
    { VK_ERROR_TOO_MANY_OBJECTS,      { "Too many resources created",     Severity::S_WARNING } },
    //instead of closing - try an alternative or fallback format
    { VK_ERROR_FORMAT_NOT_SUPPORTED,  { "Texture/format not supported",   Severity::S_WARNING } },
    //instead of closing - try to shrink the pool, migrate memory or reallocate
    { VK_ERROR_FRAGMENTED_POOL,       { "Fragmented memory pool",         Severity::S_WARNING } },
    //close reason - unknown and potentially fatal internal Vulkan error
    { VK_ERROR_UNKNOWN,               { "Unknown error",                  Severity::S_FATAL } },
    
    //close reason - the validation layers found a bug, can lead to corrupted graphics and crashes
    { VK_ERROR_VALIDATION_FAILED,                            { "Validation failed (layer error)",                        Severity::S_FATAL } },
    //close reason - applications managed memory is drained, cannot allocate any more objects
    { VK_ERROR_OUT_OF_POOL_MEMORY,                           { "Allocation pool exhausted",                              Severity::S_FATAL } },
    //close reason - OS/driver received garbage data, continuing will cause immediate instability
    { VK_ERROR_INVALID_EXTERNAL_HANDLE,                      { "Invalid external handle passed",                         Severity::S_FATAL } },
    //close reason - invalid capture data, you can't capture that frame
    { VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS,               { "Invalid opaque capture address",                         Severity::S_FATAL } },
    //instead of closing - try to migrate existing memory to a new pool or shrink/expand pools
    { VK_ERROR_FRAGMENTATION,                                { "Memory pool too fragmented to allocate",                 Severity::S_WARNING } },
    { VK_PIPELINE_COMPILE_REQUIRED,                          { "Shader requires recompilation",                          Severity::S_INFO } },
    //close reason - the driver forbade the operation
    { VK_ERROR_NOT_PERMITTED,                                { "Operation not permitted by driver",                      Severity::S_FATAL } },
    { VK_ERROR_SURFACE_LOST_KHR,                             { "Surface lost (window closed/minimized) (KHR)",           Severity::S_WARNING } },
    { VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,                     { "Native window still in use (KHR)",                       Severity::S_INFO } },
    { VK_SUBOPTIMAL_KHR,                                     { "Image suboptimal (re-acquire swapchain required) (KHR)", Severity::S_WARNING } },
    { VK_ERROR_OUT_OF_DATE_KHR,                              { "Surface out of date (window resized) (KHR)",             Severity::S_WARNING } },
    //instead of closing - ignore or retry on different monitor, treat as fatal at initialization
    { VK_ERROR_INCOMPATIBLE_DISPLAY_KHR,                     { "Display incompatible with requested config (KHR)",       Severity::S_WARNING } },
    //close reason - the shader binary is bad, the driver won't let it run
    { VK_ERROR_INVALID_SHADER_NV,                            { "Invalid NVidia shader program",                          Severity::S_FATAL } },
    //close reason - texture format or usage is impossible, rendering will likely crash on the next draw call
    { VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR,                { "Image usage not supported (KHR)",                        Severity::S_FATAL } },
    //instead of closing - switch plane layout
    { VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR,       { "Video picture layout unsupported (KHR)",                 Severity::S_WARNING } },
    //instead of closing - switch to different operation
    { VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR,    { "Video profile operation not supported (KHR)",            Severity::S_WARNING } },
    //instead of closing - change the container/profile
    { VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR,       { "Video profile format unsupported (KHR)",                 Severity::S_WARNING } },
    //instead of closing - skip the file or use software decode
    { VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR,        { "Video profile codec unsupported (KHR)",                  Severity::S_WARNING } },
    //instead of closing - fall back to a lower profile
    { VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR,          { "Video standard version unsupported (KHR)",               Severity::S_WARNING } },
    //instead of closing - reset to default DRM layout
    { VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT, { "Invalid DRM plane layout (EXT)",                         Severity::S_WARNING } },
    //instead of closing - try to wait or retry the operation
    { VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT,                { "Presentation timing queue full (EXT)",                   Severity::S_WARNING } },
    //close reason - the app has lost control of the display surface
    { VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT,          { "Fullscreen exclusive mode lost (EXT)",                   Severity::S_FATAL } },
    { VK_THREAD_IDLE_KHR,                                    { "Thread idle (KHR)",                                      Severity::S_INFO } },
    { VK_THREAD_DONE_KHR,                                    { "Thread completed (KHR)",                                 Severity::S_INFO } },
    { VK_OPERATION_DEFERRED_KHR,                             { "Operation deferred (KHR)",                               Severity::S_INFO } },
    { VK_OPERATION_NOT_DEFERRED_KHR,                         { "Operation not deferred (KHR)",                           Severity::S_INFO } },
    //instead of closing - resize the stream
    { VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR,             { "Invalid video standard parameters (KHR)",                Severity::S_WARNING } },
    //close reason - compression buffer is exhausted because of severe memory pressure, can lead to texture corruption
    { VK_ERROR_COMPRESSION_EXHAUSTED_EXT,                    { "Compression memory exhausted (EXT)",                     Severity::S_FATAL } },
    //close reason - the shader binary is bad, the driver won't let it run
    { VK_INCOMPATIBLE_SHADER_BINARY_EXT,                     { "Incompatible shader binary (EXT)",                       Severity::S_FATAL } },
    { VK_PIPELINE_BINARY_MISSING_KHR,                        { "Missing pipeline binary (KHR)",                          Severity::S_INFO } },
    //close reason - no space left in virtual/disk memory for this operation
    { VK_ERROR_NOT_ENOUGH_SPACE_KHR,                         { "Insufficient space for operation (KHR)",                 Severity::S_FATAL } },

    //duplicates

    //close reason - the validation layers found a bug, can lead to corrupted graphics and crashes
    { VK_ERROR_VALIDATION_FAILED_EXT,              { "Validation failed (layer error) (EXT)",        Severity::S_FATAL } },
    //close reason - applications managed memory is drained, cannot allocate any more objects
    { VK_ERROR_OUT_OF_POOL_MEMORY_KHR,             { "Allocation pool exhausted (KHR)",              Severity::S_FATAL } },
    //close reason - OS/driver received garbage data, continuing will cause immediate instability
    { VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR,        { "Invalid external handle passed (EXT)",         Severity::S_FATAL } },
    //instead of closing - try to migrate existing memory to a new pool or shrink/expand pools
    { VK_ERROR_FRAGMENTATION_EXT,                  { "Memory pool too fragmented to allocate (EXT)", Severity::S_WARNING } },
    //close reason - the driver forbade the operation
    { VK_ERROR_NOT_PERMITTED_EXT,                  { "Operation not permitted by driver (EXT)",      Severity::S_FATAL } },
    //close reason - the driver forbade the operation
    { VK_ERROR_NOT_PERMITTED_KHR,                  { "Operation not permitted by driver (KHR)",      Severity::S_FATAL } },
    //close reason - invalid capture data, you can't capture that frame
    { VK_ERROR_INVALID_DEVICE_ADDRESS_EXT,         { "Invalid opaque capture address (EXT)",         Severity::S_FATAL } },
    //close reason - invalid capture data, you can't capture that frame
    { VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR, { "Invalid opaque capture address (KHR)",         Severity::S_FATAL } },
    { VK_PIPELINE_COMPILE_REQUIRED_EXT,            { "Shader requires recompilation (EXT)",          Severity::S_INFO } },
    { VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT,      { "Shader requires recompilation (EXT)",          Severity::S_INFO } },
    //close reason - the shader binary is bad, the driver won't let it run
    { VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT,     { "Incompatible shader binary (EXT)",             Severity::S_FATAL } }
};

static void PrintError(string_view message)
{
    Log::Print(
        message,
        "KG_VULKAN",
        LogType::LOG_ERROR,
        2);
}

namespace KalaGraphics::Graphics
{
    static KalaGraphicsRegistry<VulkanContext> registry{};

    KalaGraphicsRegistry<VulkanContext>& VulkanContext::GetRegistry() { return registry; }

    void VulkanContext::Initialize()
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
            CloseOnError(
                "Vulkan instance init error",
                "Failed to initialize Vulkan because the instance version was lower than minimum required version 1.3!",
                0);
        }

        VkInstance instance = GraphicsContext::GetVKInstance();

        if (!instance) exit(1);

        //
        // STORE DEVICE COUNT
        //

        VkResult vkResult = vkEnumeratePhysicalDevices(
            instance, 
            &deviceCount, 
            nullptr);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan instance init error",
                "The instance is invalid!",
                vkResult);
        }

        if (deviceCount == 0)
        {
            CloseOnError(
                "Vulkan instance init error",
                "No valid devices were found!",
                vkResult);
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
            CloseOnError(
                "Vulkan instance init error",
                "Failed to initialize Vulkan instance because no suitable physical device was found!",
                0);
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
            CloseOnError(
                "Vulkan instance init error",
                "Failed to initialize Vulkan instance because no graphics queue family was found!",
                0);
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

        vkResult = vkCreateDevice(
            physicalDevice,
            &deviceInfo,
            nullptr,
            &logicalDevice);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan instance init error",
                "Failed to initialize Vulkan instance because logical device creation failed!",
                vkResult);
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
        // CREATE VMA ALLOCATOR
        //

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = logicalDevice;
        allocatorInfo.instance = instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

        vkResult = vmaCreateAllocator(
            &allocatorInfo, 
            &vmaAllocator);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan instance init error",
                "Failed to initialize Vulkan instance because VMA allocator creation failed!",
                vkResult);
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

        vkResult = vkCreateDescriptorPool(
            logicalDevice,
            &descPoolInfo,
            nullptr,
            &descriptorPool);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan instance init error",
                "Failed to initialize Vulkan instance because descriptor pool creation failed!",
                vkResult);
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

    bool VulkanContext::IsInitialized() { return isInitialized; }

    bool VulkanContext::IsVerboseLoggingEnabled() { return isVerboseLoggingEnabled; }
    void VulkanContext::SetVerboseLoggingState(bool state) { isVerboseLoggingEnabled = state; }

    string VulkanContext::GetVkResultMessage(int result)
    {
        if (!vkResultData.contains((VkResult)result))
        {
            PrintError("Vulkan result code '" + to_string(result) + "' is invalid!");
            return {};
        }

        return vkResultData[(VkResult)result].message;
    }
    Severity VulkanContext::GetVkResultSeverity(int result)
    {
        if (!vkResultData.contains((VkResult)result))
        {
            PrintError("Vulkan result code '" + to_string(result) + "' is invalid!");
            return {};
        }

        return vkResultData[(VkResult)result].severity;
    }

    void VulkanContext::CloseOnError(
        string_view title,
        string_view message,
        int result)
    {
        if (!vkResultData.contains((VkResult)result))
        {
            PrintError("Vulkan result code '" + to_string(result) + "' is invalid!");

            KalaGraphicsCore::ForceClose(
                title,
                string(message));
        }

        if (result == VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                title,
                string(message));
        }
        else
        {
            KalaGraphicsCore::ForceClose(
                title,
                string(message) + "\nReason: " + GetVkResultMessage(result));
        }
    }

    VkPhysicalDevice VulkanContext::GetPhysicalDevice()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get physical device because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return physicalDevice;
    }
    VkDevice VulkanContext::GetLogicalDevice()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get logical device because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return logicalDevice;
    }
    VkQueue VulkanContext::GetGraphicsQueue()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get graphics queue because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return graphicsQueue;
    }
    VmaAllocator VulkanContext::GetVmaAllocator()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get VMA allocator because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return vmaAllocator;
    }
    VkDescriptorPool VulkanContext::GetDescriptorPool()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get descriptor pool because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return descriptorPool;
    }

    VulkanContext* VulkanContext::InitializeContext(u32 graphicsContextID)
    {
        GraphicsContext* context = GraphicsContext::GetRegistry().GetContent(graphicsContextID);
        if (!context)
        {
            PrintError("Failed to initialize Vulkan surface because window context ID '" + to_string(graphicsContextID) + "' was not found!");

            return nullptr;
        }

        VkSurfaceKHR surface = context->GetGraphicsContextData().context_vk_surface;

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
            CloseOnError(
                "Vulkan surface init error",
                "Failed to initialize Vulkan surface because the instance surface is not supported by the physical device!",
                0);
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
        vec2 staticFramebufferSize = context->GetStaticViewportSize();

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

        unique_ptr<VulkanContext> newContext = make_unique<VulkanContext>();
        VulkanContext* contextPtr = newContext.get();

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
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use MAILBOX but device does not support it, falling back to FIFO_RELAXED.",
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }

                chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
            else
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use MAILBOX and FIFO_RELAXED but device does not support them, falling back to FIFO.",
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }

                chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
            break;
        }
        case VSyncState::VSYNC_ON_ADAPTIVE:
        {
            if (supportsFifoRelaxed) chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            else
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use FIFO_RELAXED but device does not support it, falling back to FIFO.",
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }

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
        VkResult vkResult = vkCreateSwapchainKHR(
            logicalDevice,
            &swapchainInfo,
            nullptr,
            &swapchain);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan surface init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because swapchain creation failed!",
                vkResult);
        }

        contextPtr->swapchain = swapchain;
        contextPtr->swapchainFormat = chosenFormat.format;
        contextPtr->extent = extent;

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

            VkResult vkResult = vkCreateImageView(
                logicalDevice,
                &viewInfo,
                nullptr,
                &scImageViews[i]);

            if (vkResult != VK_SUCCESS)
            {
                CloseOnError(
                    "Vulkan surface init error",
                    "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because image view creation failed!",
                    vkResult);
            }
        }

        contextPtr->imageViews = scImageViews;
        contextPtr->imagesInFlight.resize(swapchainImageCount, VK_NULL_HANDLE);

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
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
            | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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
        vkResult = vkCreateRenderPass(
            logicalDevice,
            &renderPassInfo,
            nullptr,
            &renderPass);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan surface init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because render pass creation failed!",
                vkResult);
        }

        contextPtr->renderPass = renderPass;

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
        vkResult = vmaCreateImage(
            vmaAllocator,
            &depthImageInfo,
            &depthAllocInfo,
            &depthImage,
            &depthAllocation,
            nullptr);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan surface init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because depth image creation failed!",
                vkResult);
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
        vkResult = vkCreateImageView(
            logicalDevice,
            &depthViewInfo,
            nullptr,
            &depthImageView);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan surface init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because depth image view creation failed!",
                vkResult);
        }

        contextPtr->depthImage = depthImage;
        contextPtr->depthAllocation = depthAllocation;
        contextPtr->depthImageView = depthImageView;

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
            
            vkResult = vkCreateFramebuffer(
                logicalDevice,
                &framebufferInfo,
                nullptr,
                &newFramebuffers[i]);
            
            if (vkResult != VK_SUCCESS)
            {
                CloseOnError(
                    "Vulkan surface init error",
                    "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID()) + "' because framebuffer creation failed for image " + to_string(i) + "!",
                    vkResult);
            }
        }

        contextPtr->framebuffers = newFramebuffers;

        //
        // CREATE SYNC OBJECTS
        //

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> availableSemaphores{};
        array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkResult = vkCreateSemaphore(
                logicalDevice,
                &semaphoreInfo,
                nullptr,
                &availableSemaphores[i]);

            if (vkResult != VK_SUCCESS)
            {
                CloseOnError(
                    "Vulkan surface init error",
                    "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID())
                    + "' because available semaphore '" + to_string(i) + "' creation failed!",
                    vkResult);
            }

            vkResult = vkCreateFence(
                logicalDevice,
                &fenceInfo,
                nullptr,
                &inFlightFences[i]);

            if (vkResult != VK_SUCCESS)
            {
                CloseOnError(
                    "Vulkan surface init error",
                    "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID())
                    + "' because in flight fence '" + to_string(i) + "' creation failed!",
                    vkResult);
            }
        }

        contextPtr->availableSemaphores = availableSemaphores;
        contextPtr->inFlightFences = inFlightFences;

        vector<VkSemaphore> renderFinishedSemaphores(swapchainImageCount);
        for (u32 i = 0; i < swapchainImageCount; i++)
        {
            vkResult = vkCreateSemaphore(
                logicalDevice,
                &semaphoreInfo,
                nullptr,
                &renderFinishedSemaphores[i]);

            if (vkResult != VK_SUCCESS)
            {
                CloseOnError(
                    "Vulkan surface init error",
                    "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID())
                    + "' because render finished semaphore '" + to_string(i) + "' creation failed!",
                    vkResult);
            }
        }

        contextPtr->renderFinishedSemaphores = renderFinishedSemaphores;

        //
        // CREATE CONTEXT POOL
        //

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = graphicsFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkResult = vkCreateCommandPool(
            logicalDevice,
            &poolInfo,
            nullptr,
            &contextPtr->commandPool);

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan window context init error",
                "Failed to initialize Vulkan window context because command pool creation failed!",
                vkResult);
        }

        //
        // ALLOCATE COMMAND BUFFERS
        //

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = contextPtr->commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        vkResult = vkAllocateCommandBuffers(
            logicalDevice,
            &allocInfo,
            contextPtr->commandBuffers.data());

        if (vkResult != VK_SUCCESS)
        {
            CloseOnError(
                "Vulkan surface init error",
                "Failed to initialize Vulkan surface for window context '" + to_string(context->GetID())
                + "' because parallel command buffer allocation failed!",
                vkResult);
        }

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);
        contextPtr->ID = newID;
        contextPtr->graphicsContextID = graphicsContextID;

        registry.AddContent(newID, std::move(newContext));

        Log::Print(
            "Created new Vulkan context with ID '" + to_string(newID) + "'!",
            "KG_VULKAN",
            LogType::LOG_SUCCESS);

        return contextPtr;
    }

    u32 VulkanContext::GetID() const { return ID; }
    u32 VulkanContext::GetGraphicsContextID() const { return graphicsContextID; }

    VkSwapchainKHR& VulkanContext::GetSwapchain() { return swapchain; }
    vector<VkImageView>& VulkanContext::GetImageViews() { return imageViews; }
    VkRenderPass& VulkanContext::GetRenderPass() { return renderPass; } 
    VkImage& VulkanContext::GetDepthImage() { return depthImage; }
    VkImageView& VulkanContext::GetDepthImageView() { return depthImageView; }
    vector<VkFramebuffer>& VulkanContext::GetFramebuffers() { return framebuffers; } 
    array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>& VulkanContext::GetAvailableSemaphores() { return availableSemaphores; }
    vector<VkSemaphore>& VulkanContext::GetRenderFinishedSemaphores() { return renderFinishedSemaphores; }
    VkCommandPool& VulkanContext::GetCommandPool() { return commandPool; }
    array<VkFence, MAX_FRAMES_IN_FLIGHT>& VulkanContext::GetInFlightFences() { return inFlightFences; }
    array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT>& VulkanContext::GetCommandBuffers() { return commandBuffers; }

    bool VulkanContext::SetVSyncState() { return RecreateSwapchain(); }

    void VulkanContext::Update()
    {
        GraphicsContext* ctx = GraphicsContext::GetRegistry().GetContent(graphicsContextID);

        if (!ctx)
        {
            CloseOnError(
                "Vulkan update error",
                "Failed to run Vulkan update loop because the Vulkan context '" + to_string(ID) + "' lost its graphics context!",
                0);
        }

        if (logicalDevice == VK_NULL_HANDLE)
        {
            CloseOnError(
                "Vulkan update error",
                "Logical device for Vulkan context '" + to_string(ID) + "' was not found!",
                0);
        }

        vkWaitForFences(
            logicalDevice,
            1,
            &inFlightFences[currentFrame],
            VK_TRUE,
            UINT64_MAX);

        u32 imageIndex{};
        VkResult result = vkAcquireNextImageKHR(
            logicalDevice,
            swapchain,
            UINT64_MAX,
            availableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            &imageIndex);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_OUT_OF_DATE_KHR
                || result == VK_SUBOPTIMAL_KHR)
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Recreating swapchain because image aquire returned out of date or suboptimal.",
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }

                RecreateSwapchain();
                return;
            }

            if (GetVkResultSeverity(result) == Severity::S_FATAL)
            {
                CloseOnError(
                    "Vulkan runtime error", 
                    "Encountered a fatal image aquire error!",
                    result);
            }
            else if (GetVkResultSeverity(result) == Severity::S_WARNING)
            {
#ifdef KDEBUG
                Log::Print(
                    "Image aquire returned a warning: " + GetVkResultMessage(result),
                    "KG_VULKAN",
                    LogType::LOG_WARNING);
#endif
            }
            else
            {
                if (isVerboseLoggingEnabled
                    && result != VK_SUCCESS)
                {
                    Log::Print(
                        "Image aquire returned a message: " + GetVkResultMessage(result),
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }
            }
        }

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            vkWaitForFences(
                logicalDevice,
                1,
                &imagesInFlight[imageIndex],
                VK_TRUE,
                UINT64_MAX);
        }

        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        vkResetFences(
            logicalDevice,
            1,
            &inFlightFences[currentFrame]);
        vkResetCommandBuffer(
            commandBuffers[currentFrame],
            0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(
            commandBuffers[currentFrame],
            &beginInfo);

        array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 1.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount = scast<u32>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(
            commandBuffers[currentFrame],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        //
        // BEGIN DRAW
        //

        vec2 depth = ctx->GetDepth();
        vec2 vpOffset = ctx->GetViewportOffset();
        vec2 scissorSize = ctx->GetScissorSize();

        VkViewport viewport{};
        viewport.x = vpOffset.x;
        viewport.y = vpOffset.y;
        viewport.width = scast<f32>(extent.width);
        viewport.height = scast<f32>(extent.height);
        viewport.minDepth = depth.x;
        viewport.maxDepth = depth.y;

        VkRect2D scissor{};
        scissor.offset = { scast<int>(scissorSize.x), scast<int>(scissorSize.y) };
        scissor.extent = extent;

        vkCmdSetViewport(
            commandBuffers[currentFrame],
            0,
            1,
            &viewport);
            
        vkCmdSetScissor(
            commandBuffers[currentFrame],
            0,
            1,
            &scissor);

        //
        // END DRAW
        //

        vkCmdEndRenderPass(commandBuffers[currentFrame]);
        vkEndCommandBuffer(commandBuffers[currentFrame]);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &availableSemaphores[currentFrame];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphores[imageIndex];

        vkQueueSubmit(
            graphicsQueue,
            1,
            &submitInfo,
            inFlightFences[currentFrame]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(
            graphicsQueue,
            &presentInfo);

        if (GetVkResultSeverity(result) == Severity::S_FATAL)
        {
            CloseOnError(
                "Vulkan runtime error", 
                "Encountered a fatal queue present error!",
                result);
        }
        else if (GetVkResultSeverity(result) == Severity::S_WARNING)
        {
#ifdef KDEBUG
            Log::Print(
                "Queue present returned a warning: " + GetVkResultMessage(result),
                "KG_VULKAN",
                LogType::LOG_WARNING);
#endif
        }
        else
        {
            if (isVerboseLoggingEnabled
                && result != VK_SUCCESS)
            {
                Log::Print(
                    "Queue present returned a message: " + GetVkResultMessage(result),
                    "KG_VULKAN",
                    LogType::LOG_VERBOSE);
            }
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR
            || result == VK_SUBOPTIMAL_KHR)
        {
            if (isVerboseLoggingEnabled)
            {
                Log::Print(
                    "Recreating swapchain because image aquire returned out of date or suboptimal.",
                    "KG_VULKAN",
                    LogType::LOG_VERBOSE);
            }

            RecreateSwapchain();
            return;
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanContext::ResizeUpdate() { RecreateSwapchain(); }

    bool VulkanContext::RecreateSwapchain()
    {
        GraphicsContext* context = GraphicsContext::GetRegistry().GetContent(graphicsContextID);
        if (!context)
        {
            KalaGraphicsCore::ForceClose(
                "Vulkan swapchain error", 
                "Failed to recreate swapchain because Graphics context '" + to_string(graphicsContextID) + "' was not found!");
        }

        VkSurfaceKHR surface = context->GetGraphicsContextData().context_vk_surface;

        if (!isInitialized)
        {
            PrintError("Failed to recreate swapchain because Vulkan was not initialized!");

            return false;
        }

        //drain the gpu before freeing its context resources
        vkDeviceWaitIdle(logicalDevice);

        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice,
            surface,
            &caps);

        //skip minimized window
        if (caps.currentExtent.width == 0
            || caps.currentExtent.height == 0)
        {
            return true;
        }

        //
        // DESTROY
        //

        for (auto& fb : framebuffers)
        {
            vkDestroyFramebuffer(
                logicalDevice,
                fb,
                nullptr);
        }
        framebuffers.clear();

        for (auto& view : imageViews)
        {
            vkDestroyImageView(
                logicalDevice,
                view,
                nullptr);
        }
        imageViews.clear();

        if (depthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(
                logicalDevice,
                depthImageView,
                nullptr);

            depthImageView = VK_NULL_HANDLE;
        }

        if (depthImage != VK_NULL_HANDLE)
        {
            vmaDestroyImage(
                vmaAllocator,
                depthImage,
                depthAllocation);

            depthImage = VK_NULL_HANDLE;
            depthAllocation = VK_NULL_HANDLE;
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

        VkExtent2D newExtent{};
        vec2 staticFramebufferSize = context->GetStaticViewportSize();

        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            newExtent = capabilities.currentExtent;
        }
        else
        {
            newExtent.width = clamp(
                scast<u32>(staticFramebufferSize.x),
                capabilities.minImageExtent.width, 
                capabilities.maxImageExtent.width);
            newExtent.height = clamp(
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
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use MAILBOX but device does not support it, falling back to FIFO_RELAXED.",
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }

                chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
            else
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use MAILBOX and FIFO_RELAXED but device does not support them, falling back to FIFO.",
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }

                chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
            break;
        }
        case VSyncState::VSYNC_ON_ADAPTIVE:
        {
            if (supportsFifoRelaxed) chosenPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            else
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use FIFO_RELAXED but device does not support it, falling back to FIFO.",
                        "KG_VULKAN",
                        LogType::LOG_VERBOSE);
                }

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
        swapchainInfo.surface = context->GetGraphicsContextData().context_vk_surface;
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = chosenFormat.format;
        swapchainInfo.imageColorSpace = chosenFormat.colorSpace;
        swapchainInfo.imageExtent = newExtent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = capabilities.currentTransform;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = chosenPresentMode;
        swapchainInfo.clipped = VK_TRUE;
        swapchainInfo.oldSwapchain = swapchain;

        VkSwapchainKHR newSwapchain{};
        VkResult vkResult = vkCreateSwapchainKHR(
            logicalDevice,
            &swapchainInfo,
            nullptr,
            &newSwapchain);

        if (vkResult != VK_SUCCESS)
        {
            VulkanContext::CloseOnError(
                "Vulkan swapchain error",
                "Failed to recreate Vulkan swapchain for window context '" 
                + to_string(context->GetID()) + "' because swapchain creation failed!",
                vkResult);
        }

        if (swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(
                logicalDevice,
                swapchain,
                nullptr);
        }

        swapchain = newSwapchain;
        swapchainFormat = chosenFormat.format;
        extent = newExtent;

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

        imageViews.resize(swapchainImageCount);
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

            vkResult = vkCreateImageView(
                logicalDevice,
                &viewInfo,
                nullptr,
                &imageViews[i]);

            if (vkResult != VK_SUCCESS)
            {
                VulkanContext::CloseOnError(
                    "Vulkan swapchain error",
                    "Failed to recreate Vulkan swapchain for window context '" 
                    + to_string(context->GetID()) + "' because image view creation failed!",
                    vkResult);
            }
        }

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

        vkResult = vmaCreateImage(
            vmaAllocator,
            &depthImageInfo,
            &depthAllocInfo,
            &depthImage,
            &depthAllocation,
            nullptr);

        if (vkResult != VK_SUCCESS)
        {
            VulkanContext::CloseOnError(
                "Vulkan swapchain error",
                "Failed to recreate Vulkan swapchain for window context '" 
                + to_string(context->GetID()) + "' because depth image creation failed!",
                vkResult);
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

        vkResult = vkCreateImageView(
            logicalDevice,
            &depthViewInfo,
            nullptr,
            &depthImageView);

        if (vkResult != VK_SUCCESS)
        {
            VulkanContext::CloseOnError(
                "Vulkan swapchain error",
                "Failed to recreate Vulkan swapchain for window context '" 
                + to_string(context->GetID()) + "' because depth image view creation failed!",
                vkResult);
        }

        //
        // CREATE FRAMEBUFFERS
        //

        framebuffers.resize(swapchainImageCount);
        for (u32 i = 0; i < swapchainImageCount; i++)
        {
            array<VkImageView, 2> attachments = 
            {
                imageViews[i],
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
            
            vkResult = vkCreateFramebuffer(
                logicalDevice,
                &framebufferInfo,
                nullptr,
                &framebuffers[i]);

            if (vkResult != VK_SUCCESS)
            {
                VulkanContext::CloseOnError(
                    "Vulkan swapchain error",
                    "Failed to recreate Vulkan swapchain for window context '" 
                    + to_string(context->GetID()) + "' because framebuffer creation failed for image " + to_string(i) + "!",
                    vkResult);
            }
        }

        //
        // CLEANUP AND FINISH
        //

        for (auto& sem : renderFinishedSemaphores)
        {
            if (sem != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(
                    logicalDevice,
                    sem,
                    nullptr);
            }
        }
        renderFinishedSemaphores.resize(swapchainImageCount);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (u32 i = 0; i < swapchainImageCount; i++)
        {
            vkResult = vkCreateSemaphore(
                logicalDevice,
                &semaphoreInfo,
                nullptr,
                &renderFinishedSemaphores[i]);

            if (vkResult != VK_SUCCESS)
            {
                VulkanContext::CloseOnError(
                    "Vulkan swapchain error",
                    "Failed to recreate render finished semaphore at index " + to_string(i),
                    vkResult);
            }
        }

        imagesInFlight.resize(swapchainImageCount);
        fill(
            imagesInFlight.begin(),
            imagesInFlight.end(),
            VK_NULL_HANDLE);

        if (isVerboseLoggingEnabled)
        {
            Log::Print(
                "Finished recreating Vulkan swapchain.",
                "KG_VULKAN",
                LogType::LOG_VERBOSE);
        }

        return true;
    }

    void VulkanContext::Destroy()
    {
        registry.RemoveContent(ID);
    }

    VulkanContext::~VulkanContext()
    {
        //drain the gpu before freeing its context resources
        if (logicalDevice != VK_NULL_HANDLE) vkDeviceWaitIdle(logicalDevice);

        Log::Print(
            "Destroying Vulkan context '" + to_string(ID) + "'.",
            "KG_VULKAN",
            LogType::LOG_INFO);

        if (commandBuffers[0] != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(
                logicalDevice,
                commandPool,
                MAX_FRAMES_IN_FLIGHT,
                commandBuffers.data());
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (inFlightFences[i] != VK_NULL_HANDLE)
            {
                vkDestroyFence(
                    logicalDevice,
                    inFlightFences[i],
                    nullptr);   

                inFlightFences[i] = VK_NULL_HANDLE;
            }

            if (availableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(
                    logicalDevice,
                    availableSemaphores[i],
                    nullptr);

                availableSemaphores[i] = VK_NULL_HANDLE;
            }
        }

        if (commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(
                logicalDevice, 
                commandPool,
                nullptr);

            commandPool = VK_NULL_HANDLE;
        }

        for (auto& sem : renderFinishedSemaphores)
        {
            if (sem != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(
                    logicalDevice,
                    sem,
                    nullptr);   
            }
        }
        renderFinishedSemaphores.clear();

        for (auto& fb : framebuffers)
        {
            vkDestroyFramebuffer(
                logicalDevice,
                fb,
                nullptr);
        }
        framebuffers.clear();

        if (depthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(
                logicalDevice,
                depthImageView,
                nullptr);

            depthImageView = VK_NULL_HANDLE;
        }

        if (depthImage != VK_NULL_HANDLE)
        {
            vmaDestroyImage(
                vmaAllocator,
                depthImage,
                depthAllocation);

            depthImage = VK_NULL_HANDLE;
            depthAllocation = VK_NULL_HANDLE;
        }

        if (renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(
                logicalDevice,
                renderPass,
                nullptr);

            renderPass = VK_NULL_HANDLE;
        }

        for (auto& view : imageViews)
        {
            vkDestroyImageView(
                logicalDevice,
                view,
                nullptr);
        }
        imageViews.clear();

        if (swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(
                logicalDevice,
                swapchain,
                nullptr);

            swapchain = VK_NULL_HANDLE;
        }

        imagesInFlight.clear();

        //only destroy the static resources if all vulkan contexts are destroyed
        if (registry.runtimeContent.empty())
        {
            if (descriptorPool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(
                    logicalDevice,
                    descriptorPool,
                    nullptr);

                descriptorPool = VK_NULL_HANDLE;
            }
            if (vmaAllocator != VK_NULL_HANDLE)
            {
                vmaDestroyAllocator(vmaAllocator);

                vmaAllocator = VK_NULL_HANDLE;
            }
            if (logicalDevice != VK_NULL_HANDLE)
            {
                vkDestroyDevice(
                    logicalDevice,
                    nullptr);

                logicalDevice = VK_NULL_HANDLE;
            }
        }
    }
}