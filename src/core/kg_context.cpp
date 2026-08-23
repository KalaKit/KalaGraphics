//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/kg_context.hpp"

#if defined(KWIN_ANY)
#include <windows.h>
#elif defined(KLIN_ANY)
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include "vulkan/vulkan_core.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <mutex>
#include <shared_mutex>

#include "core/kg_viewport.hpp"

struct VmaStdMutex
{
    void Lock() { m.lock(); }
    void Unlock() { m.unlock(); }
    std::mutex m;
};
struct VmaStdRWMutex
{
    void LockRead() { m.lock_shared(); }
    void UnlockRead() { m.unlock_shared(); }
    void LockWrite() { m.lock(); }
    void UnlockWrite() { m.unlock(); }
    std::shared_mutex m;
};

#define VMA_MUTEX VmaStdMutex
#define VMA_RW_MUTEX VmaStdRWMutex

#include "core/kg_core.hpp"

#define VMA_IMPLEMENTATION
KG_VK_MEM_ALLOC_IGNORE_PUSH
#include "vma/vk_mem_alloc.h"
KG_VK_MEM_ALLOC_IGNORE_POP

#include "core_utils.hpp"
#include "log_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_texture.hpp"
#include "resources/kg_mesh.hpp"
#include "resources/kg_camera.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec2;
using KalaHeaders::KalaMath::mat4;

using KalaGraphics::Core::Severity;
using KalaGraphics::Resources::Shader;
using KalaGraphics::Resources::Texture;
using KalaGraphics::Resources::Mesh;
using KalaGraphics::Resources::Camera;

using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;
using std::vector;
using std::array;
using std::clamp;

static constexpr array<const char*, 2> DEVICE_EXTENSIONS =
{
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1"
};

static constexpr VkFormat TARGET_COLOR_FORMAT = VK_FORMAT_B8G8R8A8_SRGB;
static constexpr array<VkFormat, 2> DEPTH_FORMAT_CANDIDATES =
{
    //32-bit float depth, preferred for forward/deferred rendering
    VK_FORMAT_D32_SFLOAT,
    //16-bit depth, last resort for for memory-constrained targets
    VK_FORMAT_D16_UNORM
};

static bool isInitialized{};
static bool isVerboseLoggingEnabled{};

static u32 graphicsFamily = UINT32_MAX;

static VkFormat DEFAULT_COLOR_FORMAT{};
static VkFormat DEFAULT_DEPTH_FORMAT{};
 
static VkInstance vkInstance{};

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
    { VK_SUCCESS,                     { "Operation successful",           Severity::SEVERITY_INFO } },
    //instead of closing - wait or retry operation
    { VK_NOT_READY,                   { "Resource not ready",             Severity::SEVERITY_WARNING } },
    //instead of closing - wait or retry operation
    { VK_TIMEOUT,                     { "Operation timed out",            Severity::SEVERITY_WARNING } },
    { VK_EVENT_SET,                   { "Event signaled",                 Severity::SEVERITY_INFO } },
    { VK_EVENT_RESET,                 { "Event reset",                    Severity::SEVERITY_INFO } },
    //instead of closing - ignore or retry operation
    { VK_INCOMPLETE,                  { "Operation incomplete",           Severity::SEVERITY_WARNING } },
    //close reason - no recovery possible
    { VK_ERROR_OUT_OF_HOST_MEMORY,    { "System memory exhausted",        Severity::SEVERITY_FATAL } },
    //close reason - no recovery possible
    { VK_ERROR_OUT_OF_DEVICE_MEMORY,  { "Device memory (VRAM) exhausted", Severity::SEVERITY_FATAL } },
    //close reason - driver crash or corrupted system files
    { VK_ERROR_INITIALIZATION_FAILED, { "Initialization failed",          Severity::SEVERITY_FATAL } },
    //close reason - hardware connection is severed or unstable, cannot render anymore
    { VK_ERROR_DEVICE_LOST,           { "Device lost",                    Severity::SEVERITY_FATAL } },
    //instead of closing - retry mapping or switch to a different memory type
    { VK_ERROR_MEMORY_MAP_FAILED,     { "Memory mapping failed",          Severity::SEVERITY_WARNING } },
    //close reason - critical logic error
    { VK_ERROR_LAYER_NOT_PRESENT,     { "Required layer missing",         Severity::SEVERITY_FATAL } },
    //close reason - critical logic error
    { VK_ERROR_EXTENSION_NOT_PRESENT, { "Required extension missing",     Severity::SEVERITY_FATAL } },
    //close reason - critical logic error
    { VK_ERROR_FEATURE_NOT_PRESENT,   { "Required feature unsupported",   Severity::SEVERITY_FATAL } },
    //close reason - critical logic error
    { VK_ERROR_INCOMPATIBLE_DRIVER,   { "Incompatible driver version",    Severity::SEVERITY_FATAL } },
    //instead of closing - prevent creating new objects
    { VK_ERROR_TOO_MANY_OBJECTS,      { "Too many resources created",     Severity::SEVERITY_WARNING } },
    //instead of closing - try an alternative or fallback format
    { VK_ERROR_FORMAT_NOT_SUPPORTED,  { "Texture/format not supported",   Severity::SEVERITY_WARNING } },
    //instead of closing - try to shrink the pool, migrate memory or reallocate
    { VK_ERROR_FRAGMENTED_POOL,       { "Fragmented memory pool",         Severity::SEVERITY_WARNING } },
    //close reason - unknown and potentially fatal internal Vulkan error
    { VK_ERROR_UNKNOWN,               { "Unknown error",                  Severity::SEVERITY_FATAL } },
    
    //close reason - the validation layers found a bug, can lead to corrupted graphics and crashes
    { VK_ERROR_VALIDATION_FAILED,                            { "Validation failed (layer error)",                        Severity::SEVERITY_FATAL } },
    //close reason - applications managed memory is drained, cannot allocate any more objects
    { VK_ERROR_OUT_OF_POOL_MEMORY,                           { "Allocation pool exhausted",                              Severity::SEVERITY_FATAL } },
    //close reason - OS/driver received garbage data, continuing will cause immediate instability
    { VK_ERROR_INVALID_EXTERNAL_HANDLE,                      { "Invalid external handle passed",                         Severity::SEVERITY_FATAL } },
    //close reason - invalid capture data, you can't capture that frame
    { VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS,               { "Invalid opaque capture address",                         Severity::SEVERITY_FATAL } },
    //instead of closing - try to migrate existing memory to a new pool or shrink/expand pools
    { VK_ERROR_FRAGMENTATION,                                { "Memory pool too fragmented to allocate",                 Severity::SEVERITY_WARNING } },
    { VK_PIPELINE_COMPILE_REQUIRED,                          { "Shader requires recompilation",                          Severity::SEVERITY_INFO } },
    //close reason - the driver forbade the operation
    { VK_ERROR_NOT_PERMITTED,                                { "Operation not permitted by driver",                      Severity::SEVERITY_FATAL } },
    { VK_ERROR_SURFACE_LOST_KHR,                             { "Surface lost (window closed/minimized) (KHR)",           Severity::SEVERITY_WARNING } },
    { VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,                     { "Native window still in use (KHR)",                       Severity::SEVERITY_INFO } },
    { VK_SUBOPTIMAL_KHR,                                     { "Image suboptimal (re-acquire swapchain required) (KHR)", Severity::SEVERITY_WARNING } },
    { VK_ERROR_OUT_OF_DATE_KHR,                              { "Surface out of date (window resized) (KHR)",             Severity::SEVERITY_WARNING } },
    //instead of closing - ignore or retry on different monitor, treat as fatal at initialization
    { VK_ERROR_INCOMPATIBLE_DISPLAY_KHR,                     { "Display incompatible with requested config (KHR)",       Severity::SEVERITY_WARNING } },
    //close reason - the shader binary is bad, the driver won't let it run
    { VK_ERROR_INVALID_SHADER_NV,                            { "Invalid NVidia shader program",                          Severity::SEVERITY_FATAL } },
    //close reason - texture format or usage is impossible, rendering will likely crash on the next draw call
    { VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR,                { "Image usage not supported (KHR)",                        Severity::SEVERITY_FATAL } },
    //instead of closing - switch plane layout
    { VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR,       { "Video picture layout unsupported (KHR)",                 Severity::SEVERITY_WARNING } },
    //instead of closing - switch to different operation
    { VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR,    { "Video profile operation not supported (KHR)",            Severity::SEVERITY_WARNING } },
    //instead of closing - change the container/profile
    { VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR,       { "Video profile format unsupported (KHR)",                 Severity::SEVERITY_WARNING } },
    //instead of closing - skip the file or use software decode
    { VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR,        { "Video profile codec unsupported (KHR)",                  Severity::SEVERITY_WARNING } },
    //instead of closing - fall back to a lower profile
    { VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR,          { "Video standard version unsupported (KHR)",               Severity::SEVERITY_WARNING } },
    //instead of closing - reset to default DRM layout
    { VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT, { "Invalid DRM plane layout (EXT)",                         Severity::SEVERITY_WARNING } },
    //instead of closing - try to wait or retry the operation
    { VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT,                { "Presentation timing queue full (EXT)",                   Severity::SEVERITY_WARNING } },
    //close reason - the app has lost control of the display surface
    { VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT,          { "Fullscreen exclusive mode lost (EXT)",                   Severity::SEVERITY_FATAL } },
    { VK_THREAD_IDLE_KHR,                                    { "Thread idle (KHR)",                                      Severity::SEVERITY_INFO } },
    { VK_THREAD_DONE_KHR,                                    { "Thread completed (KHR)",                                 Severity::SEVERITY_INFO } },
    { VK_OPERATION_DEFERRED_KHR,                             { "Operation deferred (KHR)",                               Severity::SEVERITY_INFO } },
    { VK_OPERATION_NOT_DEFERRED_KHR,                         { "Operation not deferred (KHR)",                           Severity::SEVERITY_INFO } },
    //instead of closing - resize the stream
    { VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR,             { "Invalid video standard parameters (KHR)",                Severity::SEVERITY_WARNING } },
    //close reason - compression buffer is exhausted because of severe memory pressure, can lead to texture corruption
    { VK_ERROR_COMPRESSION_EXHAUSTED_EXT,                    { "Compression memory exhausted (EXT)",                     Severity::SEVERITY_FATAL } },
    //close reason - the shader binary is bad, the driver won't let it run
    { VK_INCOMPATIBLE_SHADER_BINARY_EXT,                     { "Incompatible shader binary (EXT)",                       Severity::SEVERITY_FATAL } },
    { VK_PIPELINE_BINARY_MISSING_KHR,                        { "Missing pipeline binary (KHR)",                          Severity::SEVERITY_INFO } },
    //close reason - no space left in virtual/disk memory for this operation
    { VK_ERROR_NOT_ENOUGH_SPACE_KHR,                         { "Insufficient space for operation (KHR)",                 Severity::SEVERITY_FATAL } },

    //duplicates

    //close reason - the validation layers found a bug, can lead to corrupted graphics and crashes
    { VK_ERROR_VALIDATION_FAILED_EXT,              { "Validation failed (layer error) (EXT)",        Severity::SEVERITY_FATAL } },
    //close reason - applications managed memory is drained, cannot allocate any more objects
    { VK_ERROR_OUT_OF_POOL_MEMORY_KHR,             { "Allocation pool exhausted (KHR)",              Severity::SEVERITY_FATAL } },
    //close reason - OS/driver received garbage data, continuing will cause immediate instability
    { VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR,        { "Invalid external handle passed (EXT)",         Severity::SEVERITY_FATAL } },
    //instead of closing - try to migrate existing memory to a new pool or shrink/expand pools
    { VK_ERROR_FRAGMENTATION_EXT,                  { "Memory pool too fragmented to allocate (EXT)", Severity::SEVERITY_WARNING } },
    //close reason - the driver forbade the operation
    { VK_ERROR_NOT_PERMITTED_EXT,                  { "Operation not permitted by driver (EXT)",      Severity::SEVERITY_FATAL } },
    //close reason - the driver forbade the operation
    { VK_ERROR_NOT_PERMITTED_KHR,                  { "Operation not permitted by driver (KHR)",      Severity::SEVERITY_FATAL } },
    //close reason - invalid capture data, you can't capture that frame
    { VK_ERROR_INVALID_DEVICE_ADDRESS_EXT,         { "Invalid opaque capture address (EXT)",         Severity::SEVERITY_FATAL } },
    //close reason - invalid capture data, you can't capture that frame
    { VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR, { "Invalid opaque capture address (KHR)",         Severity::SEVERITY_FATAL } },
    { VK_PIPELINE_COMPILE_REQUIRED_EXT,            { "Shader requires recompilation (EXT)",          Severity::SEVERITY_INFO } },
    { VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT,      { "Shader requires recompilation (EXT)",          Severity::SEVERITY_INFO } },
    //close reason - the shader binary is bad, the driver won't let it run
    { VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT,     { "Incompatible shader binary (EXT)",             Severity::SEVERITY_FATAL } }
};

static void PrintError(string_view message)
{
    Log::Print(
        message,
        "KG_CONTEXT",
        LogType::LOG_ERROR,
        2);
}

namespace KalaGraphics::Core
{
    static KalaGraphicsRegistry<GraphicsContext> registry{};

    KalaGraphicsRegistry<GraphicsContext>& GraphicsContext::GetRegistry() { return registry; }

    void GraphicsContext::ForceClose(
        string&& title,
        string&& message,
        int result)
    {
        if (!vkResultData.contains((VkResult)result))
        {
            PrintError("Vulkan result code '" + to_string(result) + "' is invalid!");

            KalaGraphicsCore::ForceClose(
                std::move(title),
                std::move(message));
        }

        if (result == VK_SUCCESS)
        {
            KalaGraphicsCore::ForceClose(
                std::move(title),
                std::move(message));
        }
        else
        {
            KalaGraphicsCore::ForceClose(
                std::move(title),
                std::move(message) + "\nReason: " + GetVkResultMessage(result));
        }
    }

    bool GraphicsContext::IsVerboseLoggingEnabled() { return isVerboseLoggingEnabled; }
    void GraphicsContext::SetVerboseLoggingState(bool state) { isVerboseLoggingEnabled = state; }

    string GraphicsContext::GetVkResultMessage(int result)
    {
        if (!vkResultData.contains((VkResult)result))
        {
            PrintError("Vulkan result code '" + to_string(result) + "' is invalid!");
            return {};
        }

        return vkResultData[(VkResult)result].message;
    }
    Severity GraphicsContext::GetVkResultSeverity(int result)
    {
        if (!vkResultData.contains((VkResult)result))
        {
            PrintError("Vulkan result code '" + to_string(result) + "' is invalid!");
            return {};
        }

        return vkResultData[(VkResult)result].severity;
    }

    void GraphicsContext::Initialize(VkInstance newVkInstace)
    {
        if (newVkInstace == VK_NULL_HANDLE)
        {
            Log::Print(
                "Failed to initialize global graphics context because "
                "VkInstance has not been assigned!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (isInitialized)
        {
            Log::Print(
                "Failed to initialize global Vulkan because it is already initialized!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vkInstance = newVkInstace;

        //
        // CHECK VERSION
        //

        u32 apiVersion = VK_API_VERSION_1_0;
        vkEnumerateInstanceVersion(&apiVersion);

        u32 major = VK_API_VERSION_MAJOR(apiVersion);
        u32 minor = VK_API_VERSION_MINOR(apiVersion);

        if (major < 1
            || (major == 1
            && minor < 4))
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because "
                "the instance version was lower than minimum required version 1.4!");
        }

        //
        // STORE DEVICE COUNT
        //

        u32 deviceCount{};
        vector<VkPhysicalDevice> devices{};

        VkResult vkResult = vkEnumeratePhysicalDevices(
            vkInstance, 
            &deviceCount, 
            nullptr);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because vkEnumeratePhysicalDevices failed!",
                vkResult);
        }

        if (deviceCount == 0)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because no valid devices were found!",
                vkResult);
        }

        devices.resize(deviceCount);

        vkEnumeratePhysicalDevices(
            vkInstance, 
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
            for (const auto& required : DEVICE_EXTENSIONS)
            {
                bool found{};
                for (const auto& ext : availableExts)
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
                "KalaGraphics context error",
                "Failed to initialize global graphics context because no suitable physical device was found!");
        }

        physicalDevice = bestDevice;

        VkFormat chosenDepthFormat = VK_FORMAT_UNDEFINED;
        for (VkFormat candidate : DEPTH_FORMAT_CANDIDATES)
        {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(
                physicalDevice,
                candidate,
                &props);

            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                chosenDepthFormat = candidate;
            }
        }

        if (chosenDepthFormat == VK_FORMAT_UNDEFINED)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because no valid depth format was found!");
        }
        else DEFAULT_DEPTH_FORMAT = chosenDepthFormat;

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
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because no graphics queue family was found!");
        }

        //
        // SET DEVICE FEATURES
        //

        f32 queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = graphicsFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures2 enabledFeatures2{};
        enabledFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        enabledFeatures2.features.samplerAnisotropy = VK_TRUE;
        enabledFeatures2.features.fillModeNonSolid = VK_TRUE;
        enabledFeatures2.features.depthClamp = VK_TRUE;
        enabledFeatures2.features.sampleRateShading = VK_TRUE;
        enabledFeatures2.features.multiDrawIndirect = VK_TRUE;

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
        dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

        VkPhysicalDeviceSynchronization2Features sync2Features{};
        sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        sync2Features.synchronization2 = VK_TRUE;

        enabledFeatures2.pNext = &dynamicRenderingFeatures;
        dynamicRenderingFeatures.pNext = &sync2Features;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &enabledFeatures2;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = scast<u32>(DEVICE_EXTENSIONS.size());
        deviceInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
        deviceInfo.pEnabledFeatures = nullptr;

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
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because logical device creation failed!",
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
        allocatorInfo.instance = vkInstance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;

        vkResult = vmaCreateAllocator(
            &allocatorInfo, 
            &vmaAllocator);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because VMA allocator creation failed!",
                vkResult);
        }

        //
        // CREATE DESCRIPTOR POOL
        //

        array<VkDescriptorPoolSize, 4> poolSizes{};
        
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = MAX_UNIFORM_BUFFER_DESCRIPTORS;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTORS;

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = MAX_STORAGE_BUFFER_DESCRIPTORS;

        poolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[3].descriptorCount = MAX_SAMPLER_DESCRIPTORS;

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.flags = 
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
            | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descPoolInfo.maxSets = MAX_DESCRIPTOR_SETS;
        descPoolInfo.poolSizeCount = scast<u32>(poolSizes.size());
        descPoolInfo.pPoolSizes = poolSizes.data();

        vkResult = vkCreateDescriptorPool(
            logicalDevice,
            &descPoolInfo,
            nullptr,
            &descriptorPool);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because descriptor pool creation failed!",
                vkResult);
        }

        isInitialized = true;

        Log::Print(
            "Initialized Vulkan Core!",
            "KG_CONTEXT",
            LogType::LOG_SUCCESS);
    }

    bool GraphicsContext::IsInitialized() { return isInitialized; }

    void GraphicsContext::Update()
    {
        for (GraphicsContext* gctx : registry.GetAllContent())
        {
            if (!gctx)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to update graphics contexts because one of those was nullptr");
            }

            gctx->UpdateInstance();
        }
    }

    GraphicsContext* GraphicsContext::InitializeInstance(GraphicsContextData&& in_context)
    {
        if (vkInstance == VK_NULL_HANDLE)
        {
            Log::Print(
                "Failed to initialize graphics context because "
                "VkInstance has not been assigned!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (!isInitialized)
        {
            Log::Print(
                "Failed to initialize graphics context because "
                "global graphics context has not yet been initialized!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (in_context.windowID == 0)
        {
            Log::Print(
                "Failed to initialize graphics context because no window ID was passed!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

#if defined(KWIN_ANY)
        if (!in_context.context_window)
        {
            Log::Print(
                "Failed to initialize graphics context because no window was passed!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        HWND hwnd = ToVar<HWND>(in_context.context_window);
        if (!IsWindow(hwnd))
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because it did not contain a real window!");
        }
#elif defined(KLIN_ANY)
        if (!in_context.context_display
            || !in_context.context_window)
        {
            Log::Print(
                "Failed to initialize graphics context because no window or display was passed!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        Display* display = ToVar<Display*>(in_context.context_display);
        Window window = ToVar<Window>(in_context.context_window);

        XWindowAttributes attr{};
        if (!XGetWindowAttributes(display, window, &attr))
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because it did not contain a real display or window!");
        }
#endif

        unique_ptr<GraphicsContext> newContext = make_unique<GraphicsContext>();
        GraphicsContext* contextPtr = newContext.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        contextPtr->ID = newID;
        contextPtr->contextData = std::move(in_context);

        string idStr = to_string(newID);

        contextPtr->InitializeVulkanContext();

        string err = registry.AddContent(newID, std::move(newContext));
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context! Reason: " + err);
        }

        Viewport* vp = Viewport::_Initialize();
        contextPtr->rootViewportID = vp->ID;
        vp->contextID = contextPtr->ID;
        vp->isRootViewport = true;

        Log::Print(
            "Created new graphics context '" + idStr + "'!",
            "KG_CONTEXT",
            LogType::LOG_SUCCESS);

        return contextPtr;
    }

    u32 GraphicsContext::GetID() const { return ID; }
    u32 GraphicsContext::GetRootViewportID() const { return rootViewportID; }
    const vector<u32>& GraphicsContext::GetExtraViewportIDs() const { return extraViewportIDs; }

    VSyncState GraphicsContext::GetVSyncState() const { return vsyncState; }
    void GraphicsContext::SetVSyncState(VSyncState newState)
    {
        if (newState == vsyncState)
        {
            Log::Print(
                "Failed to set vsync state because it already is the same!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vsyncState = newState;

        RecreateSwapchain();
    }

    vec2 GraphicsContext::GetRenderSize() const { return renderSize; }

    const GraphicsContextData& GraphicsContext::GetGraphicsContextData() const { return contextData; }

    VkInstance GraphicsContext::GetInstance()
    {
        if (vkInstance == VK_NULL_HANDLE)
        {
            Log::Print(
                "Failed to get instance because it was not assigned!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return vkInstance;
    }

    VkPhysicalDevice GraphicsContext::GetPhysicalDevice()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get physical device because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return physicalDevice;
    }
    VkDevice GraphicsContext::GetLogicalDevice()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get logical device because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return logicalDevice;
    }
    VmaAllocator GraphicsContext::GetVmaAllocator()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get VMA allocator because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return vmaAllocator;
    }
    VkDescriptorPool GraphicsContext::GetDescriptorPool()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get descriptor pool because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return descriptorPool;
    }

    u32 GraphicsContext::GetDefaultColorFormat() { return DEFAULT_COLOR_FORMAT; }
    u32 GraphicsContext::GetDefaultDepthFormat() { return DEFAULT_DEPTH_FORMAT; }

    void GraphicsContext::InitializeVulkanContext()
    {
        VkSurfaceKHR surface = contextData.context_vk_surface;

        //
        // CHECK SURFACE
        //

        bool surfaceSupported{};

        if (physicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because the physical device was invalid!");
        }

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
                "KalaGraphics context error",
                "Failed to initialize graphics context because the instance surface is not supported by the physical device!");
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
            if (format.format == TARGET_COLOR_FORMAT
                && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = format;
                DEFAULT_COLOR_FORMAT = format.format;
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
        vec2 newRenderSize{};

#if defined(KWIN_ANY)
        HWND _hwnd = ToVar<HWND>(contextData.context_window);

        RECT _rect{};
		GetClientRect(_hwnd, &_rect);

		newRenderSize = 
		{
			scast<f32>(_rect.right - _rect.left),
			scast<f32>(_rect.bottom - _rect.top)
		};
#else
        Display* _display = ToVar<Display*>(contextData.context_display);
        Window _window = ToVar<Window>(contextData.context_window);

        XWindowAttributes _attrs{};
        XGetWindowAttributes(
            _display, 
            _window, 
            &_attrs);

        newRenderSize = 
        { 
            scast<f32>(_attrs.width), 
            scast<f32>(_attrs.height)
        };
#endif

        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            newExtent = capabilities.currentExtent;
        }
        else
        {
            newExtent.width = clamp(
                scast<u32>(newRenderSize.x),
                capabilities.minImageExtent.width, 
                capabilities.maxImageExtent.width);
            newExtent.height = clamp(
                scast<u32>(newRenderSize.y),
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
        switch (vsyncState)
        {
        default:
        case VSyncState::VSYNC_OFF:
        {
            chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        }
        case VSyncState::VSYNC_ON_TRIPLE_BUFFERED:
        {
            if (supportsMailbox) chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            else if (supportsFifoRelaxed)
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use MAILBOX but device does not support it, falling back to FIFO_RELAXED.",
                        "KG_CONTEXT",
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
                        "KG_CONTEXT",
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
                        "KG_CONTEXT",
                        LogType::LOG_VERBOSE);
                }

                chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
            break;
        }
        }

        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface;
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
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

        VkSwapchainKHR newSwapchain{};
        VkResult vkResult = vkCreateSwapchainKHR(
            logicalDevice,
            &swapchainInfo,
            nullptr,
            &newSwapchain);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because swapchain creation failed!",
                vkResult);
        }

        swapchain = newSwapchain;
        swapchainFormat = chosenFormat.format;
        renderSize.x = newExtent.width;
        renderSize.y = newExtent.height;

        //
        // GET SWAPCHAIN IMAGES 
        //

        u32 swapchainImageCount{};
        vkGetSwapchainImagesKHR(
            logicalDevice,
            swapchain,
            &swapchainImageCount,
            nullptr);

        //
        // CREATE IMAGES
        //

        swapchainImages.resize(swapchainImageCount);
        vkResult = vkGetSwapchainImagesKHR(
            logicalDevice,
            swapchain,
            &swapchainImageCount,
            swapchainImages.data());

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context "
                "because image creation failed!",
                vkResult);
        }

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
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to initialize graphics context because "
                    "image view '" + to_string(i) + "' creation failed!",
                    vkResult);
            }
        }

        swapchainImageViews = scImageViews;
        swapchainImagesInFlight.resize(swapchainImageCount, VK_NULL_HANDLE);

        //
        // CREATE DEPTH IMAGE
        //

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = DEFAULT_DEPTH_FORMAT;
        depthImageInfo.extent.width = scast<u32>(renderSize.x);
        depthImageInfo.extent.height = scast<u32>(renderSize.y);
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

        VkImage newDepthImage{};
        VmaAllocation newDepthAllocation{};
        vkResult = vmaCreateImage(
            vmaAllocator,
            &depthImageInfo,
            &depthAllocInfo,
            &newDepthImage,
            &newDepthAllocation,
            nullptr);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context "
                "because depth image creation failed!",
                vkResult);
        }

        //
        // CREATE DEPTH IMAGE VIEW
        //

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = newDepthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = DEFAULT_DEPTH_FORMAT;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        VkImageView newDepthImageView{};
        vkResult = vkCreateImageView(
            logicalDevice,
            &depthViewInfo,
            nullptr,
            &newDepthImageView);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because depth image view creation failed!",
                vkResult);
        }

        depthImage = newDepthImage;
        depthAllocation = newDepthAllocation;
        depthImageView = newDepthImageView;

        //
        // CREATE SYNC OBJECTS
        //

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> newAvailableSemaphores{};
        array<VkFence, MAX_FRAMES_IN_FLIGHT> newInFlightFences{};

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkResult = vkCreateSemaphore(
                logicalDevice,
                &semaphoreInfo,
                nullptr,
                &newAvailableSemaphores[i]);

            if (vkResult != VK_SUCCESS)
            {
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to initialize graphics context "
                    "because available semaphore '" + to_string(i) + "' creation failed!",
                    vkResult);
            }

            vkResult = vkCreateFence(
                logicalDevice,
                &fenceInfo,
                nullptr,
                &newInFlightFences[i]);

            if (vkResult != VK_SUCCESS)
            {
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to initialize graphics context "
                    "because in flight fence '" + to_string(i) + "' creation failed!",
                    vkResult);
            }
        }

        availableSemaphores = newAvailableSemaphores;
        inFlightFences = newInFlightFences;

        vector<VkSemaphore> newRenderFinishedSemaphores(swapchainImageCount);
        for (u32 i = 0; i < swapchainImageCount; i++)
        {
            vkResult = vkCreateSemaphore(
                logicalDevice,
                &semaphoreInfo,
                nullptr,
                &newRenderFinishedSemaphores[i]);

            if (vkResult != VK_SUCCESS)
            {
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to initialize graphics context "
                    "because render finished semaphore '" + to_string(i) + "' creation failed!",
                    vkResult);
            }
        }

        renderFinishedSemaphores = newRenderFinishedSemaphores;

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
            &commandPool);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because command pool creation failed!",
                vkResult);
        }

        //
        // ALLOCATE COMMAND BUFFERS
        //

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        //
        // FINISH
        //

        vkResult = vkAllocateCommandBuffers(
            logicalDevice,
            &allocInfo,
            commandBuffers.data());

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because parallel command buffer allocation failed!",
                vkResult);
        }
    }

    VkCommandBuffer GraphicsContext::BeginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer vkCommandBuffer{};
        vkAllocateCommandBuffers(
            logicalDevice,
            &allocInfo,
            &vkCommandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(
            vkCommandBuffer,
            &beginInfo);

        return vkCommandBuffer;
    }
    void GraphicsContext::EndSingleTimeCommands(VkCommandBuffer vkCommandBuffer)
    {
        if (vkCommandBuffer == VK_NULL_HANDLE)
        {
            Log::Print(
                "Failed to end single time commands "
                "because the passed command buffer was invalid!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vkEndCommandBuffer(vkCommandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &vkCommandBuffer;

        vkQueueSubmit(
            graphicsQueue,
            1,
            &submitInfo,
            VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(
            logicalDevice,
            commandPool,
            1,
            &vkCommandBuffer);
    }

    void GraphicsContext::UpdateInstance()
    {
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to update graphics context '" + to_string(ID) 
                + "' because the logical device was invalid!");
        }

        //ignore if minimized
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice,
            contextData.context_vk_surface,
            &caps);

        if (caps.currentExtent.width == 0
            || caps.currentExtent.height == 0)
        {
            return;
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
                        "KG_CONTEXT",
                        LogType::LOG_VERBOSE);
                }

                RecreateSwapchain();
                return;
            }

            if (GetVkResultSeverity(result) == Severity::SEVERITY_FATAL)
            {
                ForceClose(
                    "KalaGraphics context error", 
                    "Failed to update graphics context '" + to_string(ID) 
                    + "' because it encountered a fatal image aquire error!",
                    result);
            }
            else if (GetVkResultSeverity(result) == Severity::SEVERITY_WARNING)
            {
#ifdef KDEBUG
                Log::Print(
                    "Image aquire returned a warning: " + GetVkResultMessage(result),
                    "KG_CONTEXT",
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
                        "KG_CONTEXT",
                        LogType::LOG_VERBOSE);
                }
            }
        }

        if (swapchainImagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            vkWaitForFences(
                logicalDevice,
                1,
                &swapchainImagesInFlight[imageIndex],
                VK_TRUE,
                UINT64_MAX);
        }

        swapchainImagesInFlight[imageIndex] = inFlightFences[currentFrame];

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

        VkImageMemoryBarrier2 colorBarrier{};
        colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        colorBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        colorBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.image = swapchainImages[imageIndex];
        colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorBarrier.subresourceRange.baseMipLevel = 0;
        colorBarrier.subresourceRange.levelCount = 1;
        colorBarrier.subresourceRange.baseArrayLayer = 0;
        colorBarrier.subresourceRange.layerCount = 1;

        VkImageMemoryBarrier2 depthBarrier{};
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        depthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        depthBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.dstStageMask =
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
            | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = depthImage;
        depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBarrier.subresourceRange.baseMipLevel = 0;
        depthBarrier.subresourceRange.levelCount = 1;
        depthBarrier.subresourceRange.baseArrayLayer = 0;
        depthBarrier.subresourceRange.layerCount = 1;

        array<VkImageMemoryBarrier2, 2> beginBarriers = 
        {
            colorBarrier,
            depthBarrier
        };

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = scast<u32>(beginBarriers.size());
        depInfo.pImageMemoryBarriers = beginBarriers.data();

        vkCmdPipelineBarrier2(
            commandBuffers[currentFrame],
            &depInfo);

        //
        // START DRAW
        //

        Viewport* rvp{};
        string err = Viewport::GetRegistry().GetContent(rootViewportID, rvp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to update graphics context '" + to_string(ID) 
                + "' because its primary viewport was invalid! Reason: " + err);
        }

        rvp->Update(imageIndex);

        for (u32 vpID : extraViewportIDs)
        {
            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(vpID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to update graphics context '" + to_string(ID) 
                    + "' because its extra viewport was invalid! Reason: " + err);
            }

            vp->Update(imageIndex);
        }

        //
        // END DRAW
        //

        VkImageMemoryBarrier2 presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        presentBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        presentBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        presentBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        presentBarrier.dstAccessMask = VK_ACCESS_2_NONE;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.image = swapchainImages[imageIndex];
        presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presentBarrier.subresourceRange.baseMipLevel = 0;
        presentBarrier.subresourceRange.levelCount = 1;
        presentBarrier.subresourceRange.baseArrayLayer = 0;
        presentBarrier.subresourceRange.layerCount = 1;

        VkDependencyInfo presentDepInfo{};
        presentDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        presentDepInfo.imageMemoryBarrierCount = 1;
        presentDepInfo.pImageMemoryBarriers = &presentBarrier;

        vkCmdPipelineBarrier2(
            commandBuffers[currentFrame],
            &presentDepInfo);

        vkEndCommandBuffer(commandBuffers[currentFrame]);

        VkSemaphoreSubmitInfo renderSignal{};
        renderSignal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        renderSignal.semaphore = renderFinishedSemaphores[imageIndex];
        renderSignal.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

        VkCommandBufferSubmitInfo cmdBufInfo{};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdBufInfo.commandBuffer = commandBuffers[currentFrame];

        VkSemaphoreSubmitInfo imageAvailableWait{};
        imageAvailableWait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        imageAvailableWait.semaphore = availableSemaphores[currentFrame];
        imageAvailableWait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo2 submitInfo2{};
        submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo2.commandBufferInfoCount = 1;
        submitInfo2.pCommandBufferInfos = &cmdBufInfo;
        submitInfo2.signalSemaphoreInfoCount = 1;
        submitInfo2.pSignalSemaphoreInfos = &renderSignal;
        submitInfo2.waitSemaphoreInfoCount = 1;
        submitInfo2.pWaitSemaphoreInfos = &imageAvailableWait;

        vkQueueSubmit2(
            graphicsQueue,
            1,
            &submitInfo2,
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

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_OUT_OF_DATE_KHR
                || result == VK_SUBOPTIMAL_KHR)
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Recreating swapchain because queue present returned out of date or suboptimal.",
                        "KG_CONTEXT",
                        LogType::LOG_VERBOSE);
                }

                RecreateSwapchain();
                return;
            }

            if (GetVkResultSeverity(result) == Severity::SEVERITY_FATAL)
            {
                ForceClose(
                    "KalaGraphics context error", 
                    "Failed to update graphics context '" + to_string(ID) 
                    + "' because it encountered a fatal queue present error!",
                    result);
            }
            else if (GetVkResultSeverity(result) == Severity::SEVERITY_WARNING)
            {
    #ifdef KDEBUG
                Log::Print(
                    "Queue present returned a warning: " + GetVkResultMessage(result),
                    "KG_CONTEXT",
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
                        "KG_CONTEXT",
                        LogType::LOG_VERBOSE);
                }
            }
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void GraphicsContext::RecreateSwapchain()
    {
        VkSurfaceKHR surface = contextData.context_vk_surface;

        if (!isInitialized)
        {
            PrintError("Failed to recreate swapchain because Vulkan was not initialized!");

            return;
        }

        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to recreate Vulkan swapchain for graphics context '" 
                + to_string(ID) + "' because the logical device was invalid!");
        }

        //drain the gpu before freeing its context resources
        VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to recreate Vulkan swapchain for graphics context '" 
                + to_string(ID) + "' because vkDeviceWaitIdle did not succeed!",
                vkResult);
        }

        //
        // DELETE OLD DATA
        //

        //dont delete, just clear
        swapchainImages.clear();

        for (auto& view : swapchainImageViews)
        {
            vkDestroyImageView(
                logicalDevice,
                view,
                nullptr);
        }
        swapchainImageViews.clear();

        if (depthImage != VK_NULL_HANDLE)
        {
            vmaDestroyImage(
                vmaAllocator,
                depthImage,
                depthAllocation);

            depthImage = VK_NULL_HANDLE;
            depthAllocation = VK_NULL_HANDLE;
        }

        if (depthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(
                logicalDevice,
                depthImageView,
                nullptr);

            depthImageView = VK_NULL_HANDLE;
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
            if (format.format == TARGET_COLOR_FORMAT
                && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = format;
                DEFAULT_COLOR_FORMAT = format.format;
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
        vec2 newRenderSize{};

#if defined(KWIN_ANY)
        HWND _hwnd = ToVar<HWND>(contextData.context_window);

        RECT _rect{};
		GetClientRect(_hwnd, &_rect);

		newRenderSize = 
		{
			scast<f32>(_rect.right - _rect.left),
			scast<f32>(_rect.bottom - _rect.top)
		};
#else
        Display* _display = ToVar<Display*>(contextData.context_display);
        Window _window = ToVar<Window>(contextData.context_window);

        XWindowAttributes _attrs{};
        XGetWindowAttributes(
            _display, 
            _window, 
            &_attrs);

        newRenderSize = 
        { 
            scast<f32>(_attrs.width), 
            scast<f32>(_attrs.height)
        };
#endif

        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            newExtent = capabilities.currentExtent;
        }
        else
        {
            newExtent.width = clamp(
                scast<u32>(newRenderSize.x),
                capabilities.minImageExtent.width, 
                capabilities.maxImageExtent.width);
            newExtent.height = clamp(
                scast<u32>(newRenderSize.y),
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
        switch (vsyncState)
        {
        default:
        case VSyncState::VSYNC_OFF:
        {
            chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        }
        case VSyncState::VSYNC_ON_TRIPLE_BUFFERED:
        {
            if (supportsMailbox) chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            else if (supportsFifoRelaxed)
            {
                if (isVerboseLoggingEnabled)
                {
                    Log::Print(
                        "Tried to use MAILBOX but device does not support it, falling back to FIFO_RELAXED.",
                        "KG_CONTEXT",
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
                        "KG_CONTEXT",
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
                        "KG_CONTEXT",
                        LogType::LOG_VERBOSE);
                }

                chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
            break;
        }
        }

        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = contextData.context_vk_surface;
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
        vkResult = vkCreateSwapchainKHR(
            logicalDevice,
            &swapchainInfo,
            nullptr,
            &newSwapchain);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to recreate Vulkan swapchain for graphics context '" 
                + to_string(ID) + "' because swapchain recreation failed!",
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
        renderSize.x = newExtent.width;
        renderSize.y = newExtent.height;

        u32 swapchainImageCount{};
        vkGetSwapchainImagesKHR(
            logicalDevice,
            swapchain,
            &swapchainImageCount,
            nullptr);

        //
        // CREATE IMAGES
        //

        swapchainImages.resize(swapchainImageCount);
        vkResult = vkGetSwapchainImagesKHR(
            logicalDevice,
            swapchain,
            &swapchainImageCount,
            swapchainImages.data());

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to recreate Vulkan swapchain for graphics context '" 
                + to_string(ID) + "' because image recreation failed!",
                vkResult);
        }

        //
        // CREATE IMAGE VIEWS
        //

        swapchainImageViews.resize(swapchainImageCount);
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
                &swapchainImageViews[i]);

            if (vkResult != VK_SUCCESS)
            {
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to recreate Vulkan swapchain for graphics context '" 
                    + to_string(ID) + "' because image view '" + to_string(i) + "' recreation failed!",
                    vkResult);
            }
        }

        //
        // CREATE DEPTH IMAGE
        //

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = DEFAULT_DEPTH_FORMAT;
        depthImageInfo.extent.width = renderSize.x;
        depthImageInfo.extent.height = renderSize.y;
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
            ForceClose(
                "KalaGraphics context error",
                "Failed to recreate Vulkan swapchain for graphics context '" 
                + to_string(ID) + "' because depth image recreation failed!",
                vkResult);
        }

        //
        // CREATE DEPTH IMAGE VIEW
        //

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = DEFAULT_DEPTH_FORMAT;
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
            ForceClose(
                "KalaGraphics context error",
                "Failed to recreate Vulkan swapchain for graphics context '" 
                + to_string(ID) + "' because depth image view recreation failed!",
                vkResult);
        }

        //
        // FINISH
        //

        u32 oldSwapchainImageCount = scast<u32>(renderFinishedSemaphores.size());

        if (swapchainImageCount != oldSwapchainImageCount)
        {
            //destroy excess semaphores
            for (u32 i = swapchainImageCount; i < oldSwapchainImageCount; ++i)
            {
                vkDestroySemaphore(
                    logicalDevice,
                    renderFinishedSemaphores[i],
                    nullptr);
            }

            renderFinishedSemaphores.resize(swapchainImageCount);

            //create new missing semaphores

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            for (u32 i = oldSwapchainImageCount; i < swapchainImageCount; ++i)
            {
                vkResult = vkCreateSemaphore(
                    logicalDevice,
                    &semaphoreInfo,
                    nullptr,
                    &renderFinishedSemaphores[i]);

                if (vkResult != VK_SUCCESS)
                {
                    ForceClose(
                        "KalaGraphics context error",
                        "Failed to recreate Vulkan swapchain for graphics context '" 
                        + to_string(ID) + "' because render finished semaphore recreation failed at index " + to_string(i),
                        vkResult);
                }
            }
        }

        u32 oldImagesInFlightCount = scast<u32>(swapchainImagesInFlight.size());

        if (swapchainImageCount != oldImagesInFlightCount)
        {
            //destroy excess fences
            for (u32 i = swapchainImageCount; i < oldImagesInFlightCount; ++i)
            {
                if (swapchainImagesInFlight[i] != VK_NULL_HANDLE)
                {
                    vkDestroyFence(
                        logicalDevice,
                        swapchainImagesInFlight[i],
                        nullptr);
                }
            }

            swapchainImagesInFlight.resize(
                swapchainImageCount,
                VK_NULL_HANDLE);
        }

        //TODO: rescale root viewport

        for (u32 vID : extraViewportIDs)
        {
            Viewport* vp{};
            string err = Viewport::GetRegistry().GetContent(vID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to recreate Vulkan swapchain because extra viewport was invalid! Reason: " + err);
            }

            //TODO: rescale extra viewports
            //TODO: update active cameras
            /*
            if (activeCamera)
            {
                u32 sid = activeCamera->shaderID;

                if (ContainsValue(vp->shaderIDs, sid))
                {
                    activeCamera->viewport = renderSize;

                    //enforce camera update with no data so orthographic/projection is updated correctly
                    activeCamera->Move({}, {});
                }
            }
            */
        }

        if (isVerboseLoggingEnabled)
        {
            Log::Print(
                "Finished recreating Vulkan swapchain.",
                "KG_CONTEXT",
                LogType::LOG_VERBOSE);
        }
    }

    void GraphicsContext::Destroy()
    {
        Viewport* rvp{};
        string err = Viewport::GetRegistry().GetContent(rootViewportID, rvp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to destroy graphics context '" + to_string(ID) 
                + "' because its root viewport was invalid! Reason: " + err);
        }

        rvp->isDestroyingGraphicsContext = true;
        rvp->Destroy();

        for (u32 vID : extraViewportIDs)
        {
            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(vID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy graphics context '" + to_string(ID) 
                    + "' because its extra viewport was invalid! Reason: " + err);
            }

            vp->isDestroyingGraphicsContext = true;
            vp->Destroy();
        }

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to destroy graphics context '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    GraphicsContext::~GraphicsContext()
    {
		Log::Print(
			"Destroying graphics context '" + to_string(ID) + "'.",
			"KG_CONTEXT",
			LogType::LOG_INFO);

        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics context error",
                "Failed to destroy graphics context '" + to_string(ID) 
                + "' because the logical device was invalid!");
        }

        //drain the gpu before destroying this context
        if (logicalDevice != VK_NULL_HANDLE) 
        {
            VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
            if (vkResult != VK_SUCCESS)
            {
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy graphics context '" 
                    + to_string(ID) + "' because vkDeviceWaitIdle did not succeed!",
                    vkResult);
            }
        }

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
            }

            if (availableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(
                    logicalDevice,
                    availableSemaphores[i],
                    nullptr);
            }
        }

        if (commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(
                logicalDevice, 
                commandPool,
                nullptr);
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

        if (depthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(
                logicalDevice,
                depthImageView,
                nullptr);
        }

        if (depthImage != VK_NULL_HANDLE)
        {
            vmaDestroyImage(
                vmaAllocator,
                depthImage,
                depthAllocation);
        }

        //dont delete, just clear
        swapchainImages.clear();

        for (auto& view : swapchainImageViews)
        {
            vkDestroyImageView(
                logicalDevice,
                view,
                nullptr);
        }
        swapchainImageViews.clear();

        if (swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(
                logicalDevice,
                swapchain,
                nullptr);
        }

        swapchainImagesInFlight.clear();

        //only destroy the static resources if all graphics contexts are destroyed
        if (registry.GetAllContent().empty())
        {
			Log::Print(
				"Destroying global Vulkan and all remaining resources "
                "because all graphics contexts were destroyed.",
				"KG_CONTEXT",
				LogType::LOG_INFO);
                
            Viewport::GetRegistry().DestroyAllContent();
            Shader::GetRegistry().DestroyAllContent();
            Texture::GetRegistry().DestroyAllContent();
            Camera::GetRegistry().DestroyAllContent();
            Mesh::GetRegistry().DestroyAllContent();

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