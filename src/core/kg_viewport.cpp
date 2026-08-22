//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <unordered_map>

#include "log_utils.hpp"

#include "core/kg_viewport.hpp"
#include "core/kg_core.hpp"
#include "resources/kg_shader.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;
using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec2;

using KalaGraphics::Core::ViewportStaticSize;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Resources::Shader;

using std::string_view;
using std::to_string;
using std::unordered_map;

//
// 4:3
//

static constexpr string_view vp_640_480   = "640x480";
static constexpr string_view vp_800_600   = "800x600";
static constexpr string_view vp_1024_768  = "1024x768";
static constexpr string_view vp_1600_1200 = "1600x1200";

//
// 16:9
//

static constexpr string_view vp_1280_720 = "1280x720";
static constexpr string_view vp_1600_900 = "1600x900";
static constexpr string_view vp_1920_1080 = "1920x1080";
static constexpr string_view vp_2560_1440 = "2560x1440";
static constexpr string_view vp_3840_2160 = "3840x2160";

//16:10

static constexpr string_view vp_1280_800 = "640x480";
static constexpr string_view vp_1680_1050 = "640x480";
static constexpr string_view vp_1920_1200 = "640x480";
static constexpr string_view vp_2560_1600 = "2560x1600";

//21:9

static constexpr string_view vp_2560_1080 = "2560x1080";
static constexpr string_view vp_3440_1440 = "3440x1440";
static constexpr string_view vp_5120_2160 = "5120x2160";

//32:9

static constexpr string_view vp_3840_1080 = "3840x1080";
static constexpr string_view vp_5120_1440 = "5120x1440";

static unordered_map<ViewportStaticSize, string_view, EnumHash<ViewportStaticSize>> vpNames =
{
    { ViewportStaticSize::VP_640_480,   vp_640_480 },
    { ViewportStaticSize::VP_800_600,   vp_800_600 },
    { ViewportStaticSize::VP_1024_768,  vp_1024_768 },
    { ViewportStaticSize::VP_1600_1200, vp_1600_1200 },

    { ViewportStaticSize::VP_1280_720,  vp_1280_720 },
    { ViewportStaticSize::VP_1600_900,  vp_1600_900 },
    { ViewportStaticSize::VP_1920_1080, vp_1920_1080 },
    { ViewportStaticSize::VP_2560_1440, vp_2560_1440 },
    { ViewportStaticSize::VP_3840_2160, vp_3840_2160 },

    { ViewportStaticSize::VP_1280_800,  vp_1280_800 },
    { ViewportStaticSize::VP_1680_1050, vp_1680_1050 },
    { ViewportStaticSize::VP_1920_1200, vp_1920_1200 },
    { ViewportStaticSize::VP_2560_1600, vp_2560_1600 },

    { ViewportStaticSize::VP_2560_1080, vp_2560_1080 },
    { ViewportStaticSize::VP_3440_1440, vp_3440_1440 },
    { ViewportStaticSize::VP_5120_2160, vp_5120_2160 },

    { ViewportStaticSize::VP_3840_1080, vp_3840_1080 },
    { ViewportStaticSize::VP_5120_1440, vp_5120_1440 }
};

static unordered_map<ViewportStaticSize, vec2, EnumHash<ViewportStaticSize>> vpSizes =
{
    { ViewportStaticSize::VP_640_480,   vec2(640, 480) },
    { ViewportStaticSize::VP_800_600,   vec2(800, 600) },
    { ViewportStaticSize::VP_1024_768,  vec2(1024, 768) },
    { ViewportStaticSize::VP_1600_1200, vec2(1600, 1200) },

    { ViewportStaticSize::VP_1280_720,  vec2(1280, 720) },
    { ViewportStaticSize::VP_1600_900,  vec2(1600, 900) },
    { ViewportStaticSize::VP_1920_1080, vec2(1920, 1080) },
    { ViewportStaticSize::VP_2560_1440, vec2(2560, 1440) },
    { ViewportStaticSize::VP_3840_2160, vec2(3840, 2160) },

    { ViewportStaticSize::VP_1280_800,  vec2(1280, 800) },
    { ViewportStaticSize::VP_1680_1050, vec2(1680, 1050) },
    { ViewportStaticSize::VP_1920_1200, vec2(1920, 1200) },
    { ViewportStaticSize::VP_2560_1600, vec2(2560, 1600) },

    { ViewportStaticSize::VP_2560_1080, vec2(2560, 1080) },
    { ViewportStaticSize::VP_3440_1440, vec2(3440, 1440) },
    { ViewportStaticSize::VP_5120_2160, vec2(5120, 2160) },

    { ViewportStaticSize::VP_3840_1080, vec2(3840, 1080) },
    { ViewportStaticSize::VP_5120_1440, vec2(5120, 1440) }
};

namespace KalaGraphics::Core
{
    static KalaGraphicsRegistry<Viewport> registry{};

    KalaGraphicsRegistry<Viewport>& Viewport::GetRegistry() { return registry; }

    string_view Viewport::GetViewportStaticName(ViewportStaticSize vpSize)
    {   
        string_view out{};
        string err = EnumToString(vpSize, vpNames, out);
        if (!err.empty())
        {
            Log::Print(
                "Failed to get viewport name! Reason: " + err, 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        return out;
    }
    vec2 Viewport::GetViewportStaticValue(ViewportStaticSize vpSize)
    {
		auto it = vpSizes.find(vpSize);
		if (it == vpSizes.end())
        {
            Log::Print(
                "Failed to get viewport value because the passed enum was invalid!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

		return it->second;
    }

    Viewport* Viewport::Initialize(
        u32 contextID,
        bool isStatic)
    {
        return nullptr;
    }

    u32 Viewport::GetID() const { return ID; }
    const vector<u32>& Viewport::GetShaderIDs() const { return shaderIDs; }

    bool Viewport::IsRootViewport() const { return isRootViewport; }

    vec2 Viewport::GetViewportSize(bool isStatic) const
    {
        //TODO: get true dynamic viewport size

        return isStatic 
            ? vpSizes[viewportStaticSize]
            : 0.0f;
    }
    void Viewport::SetViewportSize(ViewportStaticSize vpSize)
    {
        if (!isStaticViewport)
        {
            Log::Print(
                "Failed to set graphics context '" + to_string(contextID) 
                + "' static viewport size because it is not static!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        viewportStaticSize = vpSize;

        Log::Print(
            "Set graphics context '" + to_string(contextID) 
            + "' static viewport size to '" + string(GetViewportStaticName(viewportStaticSize)) + "'!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }
    void Viewport::SetViewportSize(vec2 vpSize)
    {
        if (isStaticViewport)
        {
            Log::Print(
                "Failed to set graphics context '" + to_string(contextID) 
                + "' static viewport size because it is not dynamic!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        viewportDynamicSize = vpSize;

        Log::Print(
            "Set graphics context '" + to_string(contextID) 
            + "' dynamic viewport size to " 
            + to_string(viewportDynamicSize.x) + "x" 
            + to_string(viewportDynamicSize.y) + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    bool Viewport::IsStaticViewport() const { return isStaticViewport; }
    void Viewport::SetStaticViewportState(bool newValue)
    {
        //TODO: finish implementation

        isStaticViewport = newValue;

        string val = isStaticViewport ? "true" : "false";

        Log::Print(
            "Set graphics context '" + to_string(contextID) + "' "
            "static viewport state to " + val + "!", 
            "KG_VIEWPORT",
            LogType::LOG_SUCCESS);
    }

    vec2 Viewport::GetViewportOffset() const { return viewportOffset; }
    void Viewport::SetViewportOffset(vec2 newValue) { viewportOffset = newValue; }

    vec2 Viewport::GetScissorSize() const { return scissorSize; }
    void Viewport::SetScissorSize(vec2 newValue) { scissorSize = newValue; }

    vec2 Viewport::GetScissorOffset() const { return scissorOffset; }
    void Viewport::SetScissorOffset(vec2 newValue)
    { 
        if (newValue.x < 0.0f
            || newValue.y < 0.0f)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it was too small!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (newValue.x > 1.0f
            || newValue.y > 1.0f)
        {
            Log::Print(
                "Failed to set viewport '" + to_string(ID) 
                + "' scissor offset because it was too big!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        scissorOffset = newValue;
    }

    void Viewport::Destroy()
    {
        if (isRootViewport)
        {
            Log::Print(
                "Failed to destroy viewport '" + to_string(ID) 
                + "' because it is a root viewport of graphics context '" + to_string(contextID) + "'!", 
                "KG_VIEWPORT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        _Destroy();
    }

    void Viewport::_Destroy()
    {
		Log::Print(
			"Destroying viewport '" + to_string(ID) + "'.",
			"KG_VIEWPORT",
			LogType::LOG_INFO);

        //TODO: finish implementation

        for (u32 sID : shaderIDs)
        {
            Shader* s{};
            string err = Shader::GetRegistry().GetContent(sID, s);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics context error",
                    "Failed to destroy viewport '" + to_string(ID) + "' because "
                    "its shader was invalid! Reason: " + err);
            }

            s->isDestroyingViewport = true;
            s->Destroy();
        }

        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to destroy viewport '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Viewport::~Viewport()
    {
		Log::Print(
			"Destroying graphics context '" + to_string(ID) + "'.",
			"KG_VIEWPORT",
			LogType::LOG_INFO);
    }
}