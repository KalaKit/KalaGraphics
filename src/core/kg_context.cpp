//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "vulkan/vulkan_core.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <mutex>
#include <shared_mutex>

#include "vulkan/vulkan_core.h"

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

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include "core_utils.hpp"
#include "log_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_context.hpp"
#include "core/kg_core.hpp"
#include "core/kg_registry.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_texture.hpp"
#include "resources/kg_mesh.hpp"
#include "resources/kg_camera.hpp"
#include "resources/kg_text.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;
using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec2;
using KalaHeaders::KalaMath::mat4;

using KalaGraphics::Core::ViewportSize;
using KalaGraphics::Core::Severity;
using KalaGraphics::Resources::Shader;
using KalaGraphics::Resources::Texture;
using KalaGraphics::Resources::Mesh;
using KalaGraphics::Resources::Camera;
using KalaGraphics::Resources::Text;

using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;
using std::vector;
using std::array;
using std::clamp;

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
        "KG_CONTEXT",
        LogType::LOG_ERROR,
        2);
}

//4:3

constexpr string_view vp_640_480 = "640x480";
constexpr string_view vp_800_600 = "800x600";
constexpr string_view vp_1024_768 = "1024x768";
constexpr string_view vp_1600_1200 = "1600x1200";

//16:9

constexpr string_view vp_1280_720 = "1280x720";
constexpr string_view vp_1600_900 = "1600x900";
constexpr string_view vp_1920_1080 = "1920x1080";
constexpr string_view vp_2560_1440 = "2560x1440";
constexpr string_view vp_3840_2160 = "3840x2160";

//16:10

constexpr string_view vp_1280_800 = "640x480";
constexpr string_view vp_1680_1050 = "640x480";
constexpr string_view vp_1920_1200 = "640x480";
constexpr string_view vp_2560_1600 = "2560x1600";

//21:9

constexpr string_view vp_2560_1080 = "2560x1080";
constexpr string_view vp_3440_1440 = "3440x1440";
constexpr string_view vp_5120_2160 = "5120x2160";

//32:9

constexpr string_view vp_3840_1080 = "3840x1080";
constexpr string_view vp_5120_1440 = "5120x1440";

static unordered_map<ViewportSize, string_view, EnumHash<ViewportSize>> vpNames =
{
    { ViewportSize::VP_640_480,   vp_640_480 },
    { ViewportSize::VP_800_600,   vp_800_600 },
    { ViewportSize::VP_1024_768,  vp_1024_768 },
    { ViewportSize::VP_1600_1200, vp_1600_1200 },

    { ViewportSize::VP_1280_720,  vp_1280_720 },
    { ViewportSize::VP_1600_900,  vp_1600_900 },
    { ViewportSize::VP_1920_1080, vp_1920_1080 },
    { ViewportSize::VP_2560_1440, vp_2560_1440 },
    { ViewportSize::VP_3840_2160, vp_3840_2160 },

    { ViewportSize::VP_1280_800,  vp_1280_800 },
    { ViewportSize::VP_1680_1050, vp_1680_1050 },
    { ViewportSize::VP_1920_1200, vp_1920_1200 },
    { ViewportSize::VP_2560_1600, vp_2560_1600 },

    { ViewportSize::VP_2560_1080, vp_2560_1080 },
    { ViewportSize::VP_3440_1440, vp_3440_1440 },
    { ViewportSize::VP_5120_2160, vp_5120_2160 },

    { ViewportSize::VP_3840_1080, vp_3840_1080 },
    { ViewportSize::VP_5120_1440, vp_5120_1440 }
};

static unordered_map<ViewportSize, vec2, EnumHash<ViewportSize>> vpSizes =
{
    { ViewportSize::VP_640_480,   vec2(640, 480) },
    { ViewportSize::VP_800_600,   vec2(800, 600) },
    { ViewportSize::VP_1024_768,  vec2(1024, 768) },
    { ViewportSize::VP_1600_1200, vec2(1600, 1200) },

    { ViewportSize::VP_1280_720,  vec2(1280, 720) },
    { ViewportSize::VP_1600_900,  vec2(1600, 900) },
    { ViewportSize::VP_1920_1080, vec2(1920, 1080) },
    { ViewportSize::VP_2560_1440, vec2(2560, 1440) },
    { ViewportSize::VP_3840_2160, vec2(3840, 2160) },

    { ViewportSize::VP_1280_800,  vec2(1280, 800) },
    { ViewportSize::VP_1680_1050, vec2(1680, 1050) },
    { ViewportSize::VP_1920_1200, vec2(1920, 1200) },
    { ViewportSize::VP_2560_1600, vec2(2560, 1600) },

    { ViewportSize::VP_2560_1080, vec2(2560, 1080) },
    { ViewportSize::VP_3440_1440, vec2(3440, 1440) },
    { ViewportSize::VP_5120_2160, vec2(5120, 2160) },

    { ViewportSize::VP_3840_1080, vec2(3840, 1080) },
    { ViewportSize::VP_5120_1440, vec2(5120, 1440) }
};

namespace KalaGraphics::Core
{
    static VkInstance vk_instance{};

    static KalaGraphicsRegistry<GraphicsContext> registry{};

    KalaGraphicsRegistry<GraphicsContext>& GraphicsContext::GetRegistry() { return registry; }

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

    void GraphicsContext::SetVKInstance(VkInstance in_vk_instance)
    {
        if (!in_vk_instance)
        {
            Log::Print(
                "Failed to set instance because it was empty!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);
        }

        vk_instance = in_vk_instance;
    }
    VkInstance GraphicsContext::GetVKInstance()
    {
        if (!vk_instance)
        {
            Log::Print(
                "Failed to get instance because it was not assigned!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return vk_instance;
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
    VkQueue GraphicsContext::GetGraphicsQueue()
    {
        if (!isInitialized)
        {
            PrintError("Failed to get graphics queue because Vulkan has not been initialized!");

            return nullptr;
        }
        
        return graphicsQueue;
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

    string_view GraphicsContext::GetStaticViewportName(ViewportSize vpSize)
    {   
        string_view out{};
        if (!EnumToString(vpSize, vpNames, out))
        {
            Log::Print(
                "Failed to get viewport name because the passed enum was invalid!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return out;
    }
    vec2 GraphicsContext::GetStaticViewportSizeValue(ViewportSize vpSize)
    {
		auto it = vpSizes.find(vpSize);
		if (it == vpSizes.end())
        {
            Log::Print(
                "Failed to get viewport value because the passed enum was invalid!", 
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

		return it->second;
    }

    void GraphicsContext::Initialize()
    {
        if (!vk_instance)
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
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because "
                "the instance version was lower than minimum required version 1.4!",
                0);
        }

        //
        // STORE DEVICE COUNT
        //

        VkResult vkResult = vkEnumeratePhysicalDevices(
            vk_instance, 
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
            vk_instance, 
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
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because no suitable physical device was found!",
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
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize global graphics context because no graphics queue family was found!",
                0);
        }

        f32 queuePriority = 1.0f;

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
        allocatorInfo.instance = vk_instance;
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
        poolSizes[0].descriptorCount = 2048;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 2048;

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = 2048;

        poolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[3].descriptorCount = 64;

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.flags = 
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
            | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descPoolInfo.maxSets = 10;
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

    GraphicsContext* GraphicsContext::InitializeInstance(GraphicsContextData&& in_context)
    {
        if (!vk_instance)
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

#ifdef _WIN32
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
#else
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

        registry.AddContent(newID, std::move(newContext));

        Log::Print(
            "Created new graphics context '" + idStr + "'!",
            "KG_CONTEXT",
            LogType::LOG_SUCCESS);

        return contextPtr;
    }

    u32 GraphicsContext::GetID() const { return ID; }
    const vector<u32>& GraphicsContext::GetShaderIDs() const { return shaderIDs; }

    VSyncState GraphicsContext::GetVSyncState() const { return vsyncState; }
    void GraphicsContext::SetVSyncState(VSyncState newState)
    {
        if (newState == VSyncState::VSYNC_INVALID)
        {
            Log::Print(
                "Failed to set vsync state because it was invalid!",
                "KG_CONTEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }
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

    vec2 GraphicsContext::GetStaticViewportSize() const
    {
        return vpSizes[vpData.vpSize];
    }
    void GraphicsContext::SetStaticViewportSize(ViewportSize vpSize)
    {
        vpData.vpSize = vpSize;

        Log::Print(
            "Set static viewport size to " + string(GetStaticViewportName(vpSize)) + " for window '" + to_string(contextData.windowID) + "'.", 
            "KG_CONTEXT",
            LogType::LOG_INFO);
    }

    bool GraphicsContext::IsDynamicViewport() const { return vpData.isDynamicViewport; }
    void GraphicsContext::SetDynamicViewportState(bool newValue)
    {
        vpData.isDynamicViewport = newValue;

        string val = vpData.isDynamicViewport ? "true" : "false";

        Log::Print(
            "Set dynamic viewport state to " + val + " for window '" + to_string(contextData.windowID) + "'!", 
            "KG_CONTEXT",
            LogType::LOG_INFO);
    }

    vec2 GraphicsContext::GetDepth() const { return vpData.depth; }
    void GraphicsContext::SetDepth(vec2 newValue) { vpData.depth = newValue; }

    vec2 GraphicsContext::GetViewportOffset() const { return vpData.viewportOffset; }
    void GraphicsContext::SetViewportOffset(vec2 newValue) { vpData.viewportOffset = newValue; }

    vec2 GraphicsContext::GetScissorSize() const { return vpData.scissorSize; }
    void GraphicsContext::SetScissorSize(vec2 newValue) { vpData.scissorSize = newValue; }

    const GraphicsContextData& GraphicsContext::GetGraphicsContextData() const { return contextData; }

    vec2 GraphicsContext::GetExtent()                                                   { return extent; }
    VkSwapchainKHR& GraphicsContext::GetSwapchain()                                     { return swapchain; }
    vector<VkImageView>& GraphicsContext::GetImageViews()                               { return imageViews; }
    VkRenderPass& GraphicsContext::GetRenderPass()                                      { return renderPass; }
    VkImage& GraphicsContext::GetDepthImage()                                           { return depthImage; }
    VkImageView& GraphicsContext::GetDepthImageView()                                   { return depthImageView; }
    vector<VkFramebuffer>& GraphicsContext::GetFramebuffers()                           { return framebuffers; }
    array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>& GraphicsContext::GetAvailableSemaphores() { return availableSemaphores; }
    vector<VkSemaphore>& GraphicsContext::GetRenderFinishedSemaphores()                 { return renderFinishedSemaphores; }
    VkCommandPool& GraphicsContext::GetCommandPool()                                    { return commandPool; }
    array<VkFence, MAX_FRAMES_IN_FLIGHT>& GraphicsContext::GetInFlightFences()          { return inFlightFences; }
    array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT>& GraphicsContext::GetCommandBuffers()  { return commandBuffers; }

    void GraphicsContext::UpdateInstance()
    {
        if (logicalDevice == VK_NULL_HANDLE)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to update graphics context '" + to_string(ID) 
                + "' because the logical device was invalid!",
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
                        "KG_CONTEXT",
                        LogType::LOG_VERBOSE);
                }

                RecreateSwapchain();
                return;
            }

            if (GetVkResultSeverity(result) == Severity::S_FATAL)
            {
                ForceClose(
                    "KalaGraphics context error", 
                    "Failed to update graphics context '" + to_string(ID) 
                    + "' because it encountered a fatal image aquire error!",
                    result);
            }
            else if (GetVkResultSeverity(result) == Severity::S_WARNING)
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
        renderPassInfo.renderArea.extent = { scast<u32>(extent.x), scast<u32>(extent.y) };
        renderPassInfo.clearValueCount = scast<u32>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(
            commandBuffers[currentFrame],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        //
        // BEGIN DRAW
        //

        vec2 depth = vpData.depth;
        vec2 vpOffset = vpData.viewportOffset;
        vec2 scissorSize = vpData.scissorSize;

        VkViewport viewport{};
        viewport.x = vpOffset.x;
        viewport.y = vpOffset.y;
        viewport.width = extent.x;
        viewport.height = extent.y;
        viewport.minDepth = depth.x;
        viewport.maxDepth = depth.y;

        VkRect2D scissor{};
        scissor.offset = { scast<int>(scissorSize.x), scast<int>(scissorSize.y) };
        scissor.extent = { scast<u32>(extent.x), scast<u32>(extent.y) };

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
        
        if (Shader::GetRegistry().GetAllContent().empty())
        {
            if (missingShaderWarningCount < 10)
            {
                Log::Print(
                    "Failed to render onto graphics context '" + to_string(ID) + "' "
                    "because there are no shaders to draw with! This warning will only be given 10 times.",
                    "KG_CONTEXT",
                    LogType::LOG_WARNING);

                missingShaderWarningCount++;
            }
        }

        for (Shader* shader : Shader::GetRegistry().GetAllContent())
        {
            if (!shader)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to update graphics context '" + to_string(ID) 
                    + "' because one of the the shaders was nullptr!");
            }

            shader->Update(commandBuffers[currentFrame]);
        }

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

            if (GetVkResultSeverity(result) == Severity::S_FATAL)
            {
                ForceClose(
                    "KalaGraphics context error", 
                    "Failed to update graphics context '" + to_string(ID) 
                    + "' because it encountered a fatal queue present error!",
                    result);
            }
            else if (GetVkResultSeverity(result) == Severity::S_WARNING)
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

    void GraphicsContext::InitializeVulkanContext()
    {
        VkSurfaceKHR surface = contextData.context_vk_surface;

        //
        // CHECK SURFACE
        //

        bool surfaceSupported{};

        if (physicalDevice == VK_NULL_HANDLE)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because the physical device was invalid!",
                0);
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
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because the instance surface is not supported by the physical device!",
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

        VkExtent2D newExtent{};
        vec2 staticFramebufferSize = GetStaticViewportSize();

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
        switch (vsyncState)
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
        extent.x = newExtent.width;
        extent.y = newExtent.height;

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
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to initialize graphics context because image view creation failed!",
                    vkResult);
            }
        }

        imageViews = scImageViews;
        imagesInFlight.resize(swapchainImageCount, VK_NULL_HANDLE);

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

        VkRenderPass newRenderPass{};
        vkResult = vkCreateRenderPass(
            logicalDevice,
            &renderPassInfo,
            nullptr,
            &newRenderPass);

        if (vkResult != VK_SUCCESS)
        {
            ForceClose(
                "KalaGraphics context error",
                "Failed to initialize graphics context because render pass creation failed!",
                vkResult);
        }

        renderPass = newRenderPass;

        //
        // CREATE DEPTH IMAGE
        //

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
        depthImageInfo.extent.width = extent.x;
        depthImageInfo.extent.height = extent.y;
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
                "Failed to initialize graphics context because depth image creation failed!",
                vkResult);
        }

        //
        // CREATE DEPTH IMAGE VIEW
        //

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = newDepthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
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
        // CREATE FRAMEBUFFERS
        //

        vector<VkFramebuffer> newFramebuffers(swapchainImageCount);
        for (u32 i = 0; i < swapchainImageCount; i++)
        {
            array<VkImageView, 2> attachments = 
            {
                scImageViews[i],
                newDepthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = newRenderPass;
            framebufferInfo.attachmentCount = scast<u32>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.x;
            framebufferInfo.height = extent.y;
            framebufferInfo.layers = 1;
            
            vkResult = vkCreateFramebuffer(
                logicalDevice,
                &framebufferInfo,
                nullptr,
                &newFramebuffers[i]);
            
            if (vkResult != VK_SUCCESS)
            {
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to initialize graphics context "
                    "because framebuffer creation failed for image " + to_string(i) + "!",
                    vkResult);
            }
        }

        framebuffers = newFramebuffers;

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

        vpData.depth = { 0.0f, 1.0f };
        vpData.viewportOffset = { 0.0f, 0.0f };
        vpData.scissorSize = { 0.0f, 0.0f };

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

    void GraphicsContext::ResizeUpdate() 
    {

    }

    void GraphicsContext::RecreateSwapchain()
    {
        VkSurfaceKHR surface = contextData.context_vk_surface;

        if (!isInitialized)
        {
            PrintError("Failed to recreate swapchain because Vulkan was not initialized!");

            return;
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
        vec2 staticFramebufferSize = GetStaticViewportSize();

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
        switch (vsyncState)
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
        case VSyncState::VSYNC_OFF:
        {
            chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        }
        default: break;
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
        extent.x = newExtent.width;
        extent.y = newExtent.height;

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
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to recreate Vulkan swapchain for graphics context '" 
                    + to_string(ID) + "' because image view recreation failed!",
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
        depthImageInfo.extent.width = extent.x;
        depthImageInfo.extent.height = extent.y;
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
            ForceClose(
                "KalaGraphics context error",
                "Failed to recreate Vulkan swapchain for graphics context '" 
                + to_string(ID) + "' because depth image view recreation failed!",
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
            framebufferInfo.width = extent.x;
            framebufferInfo.height = extent.y;
            framebufferInfo.layers = 1;
            
            vkResult = vkCreateFramebuffer(
                logicalDevice,
                &framebufferInfo,
                nullptr,
                &framebuffers[i]);

            if (vkResult != VK_SUCCESS)
            {
                ForceClose(
                    "KalaGraphics context error",
                    "Failed to recreate Vulkan swapchain for graphics context '" 
                    + to_string(ID) + "' because framebuffer recreation failed for image " + to_string(i) + "!",
                    vkResult);
            }
        }

        //
        // CLEANUP AND FINISH
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

        u32 oldImagesInFlightCount = scast<u32>(imagesInFlight.size());

        if (swapchainImageCount != oldImagesInFlightCount)
        {
            //destroy excess fences
            for (u32 i = swapchainImageCount; i < oldImagesInFlightCount; ++i)
            {
                if (imagesInFlight[i] != VK_NULL_HANDLE)
                {
                    vkDestroyFence(
                        logicalDevice,
                        imagesInFlight[i],
                        nullptr);
                }
            }

            imagesInFlight.resize(
                swapchainImageCount,
                VK_NULL_HANDLE);
        }

        for (u32 cid : cameraIDs)
        {
            Camera* c = Camera::GetRegistry().GetContent(cid);
            if (!c)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to recreate swapchain because "
                    "camera '" + to_string(cid) + "' was invalid!");
            }

            c->viewport = extent;

            //enforce camera update with no data so orthographic/projection is updated correctly
            c->Move({}, {});
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
        for (u32 cID : cameraIDs)
        {
            Camera* c = Camera::GetRegistry().GetContent(cID);
            if (!c)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy graphics context '" + to_string(ID) + "' because "
                    "camera '" + to_string(cID) + "' was invalid!");
            }

            c->isDestroyingGraphicsContext = true;
            c->Destroy();
        }

        for (u32 sID : shaderIDs)
        {
            Shader* s = Shader::GetRegistry().GetContent(sID);
            if (!s)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy graphics context '" + to_string(ID) + "' because "
                    "shader '" + to_string(sID) + "' was invalid!");
            }

            s->isDestroyingGraphicsContext = true;
            s->Destroy();
        }

        registry.RemoveContent(ID);
    }

    GraphicsContext::~GraphicsContext()
    {
		Log::Print(
			"Destroying graphics context '" + to_string(ID) + "'.",
			"KG_CONTEXT",
			LogType::LOG_INFO);

        //drain the gpu before freeing its context resources
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

        //only destroy the static resources if all graphics contexts are destroyed
        if (registry.GetAllContent().empty())
        {
			Log::Print(
				"Destroying global Vulkan and all remaining resources "
                "because all graphics contexts were destroyed.",
				"KG_CONTEXT",
				LogType::LOG_INFO);
                
            Text::GetRegistry().RemoveAllContent();
            Texture::GetRegistry().RemoveAllContent();
            Camera::GetRegistry().RemoveAllContent();
            Mesh::GetRegistry().RemoveAllContent();

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