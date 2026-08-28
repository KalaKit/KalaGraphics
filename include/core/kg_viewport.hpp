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
    using KalaHeaders::KalaMath::vec4;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::array;
    using std::default_delete;

    enum class ViewportStaticSize : u8
    {
        //4:3

        VP_640_480 = 0,
        VP_800_600 = 1,
        VP_1024_768 = 2,
        VP_1600_1200 = 3,

        //16:9

        VP_1280_720 = 4,
        VP_1600_900 = 5,
        VP_1920_1080 = 6,
        VP_2560_1440 = 7,
        VP_3840_2160 = 8,

        //16:10

        VP_1280_800 = 9,
        VP_1680_1050 = 10,
        VP_1920_1200 = 11,
        VP_2560_1600 = 12,

        //21:9

        VP_2560_1080 = 13,
        VP_3440_1440 = 14,
        VP_5120_2160 = 15,

        //32:9

        VP_3840_1080 = 16,
        VP_5120_1440 = 17
    };

    enum class ViewportType : u8
    {
        //Standard fully filled viewport, no letterboxing, no fixed resolution
        VP_FILL = 0,

        //Fixed resolution, uses one of the chosen static resolutions
        //and tries to fit it while preserving as much of the viewport as possible,
        //leaves letterboxing to where aspect ratio doesn't fit viewport
        VP_FIT = 1,

        //Same as fit, except it clamps to static viewport size,
        //so if viewport is bigger than its static size then black bars appear on all edges
        VP_CENTER = 2,

        //User-defined scissor area, the only mode where manual scissor size and offset are allowed,
        //draws black boxes to area outside of scissor range
        VP_CUSTOM = 3
    };

    class LIB_API Viewport
    {
    friend class KalaGraphics::Resources::Shader;
    friend class KalaGraphics::Resources::Mesh;
    friend class KalaGraphics::Resources::Texture;
    friend class KalaGraphics::Resources::Camera;
    friend class HitTest;
    friend class GraphicsContext;
    friend struct default_delete<Viewport>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<Viewport>& GetRegistry();

        KNODISCARD
		static string_view GetStaticName(ViewportStaticSize vpSize);
        KNODISCARD
		static vec2 GetStaticValue(ViewportStaticSize vpSize);

        //Create a blank viewport with optional viewport type toggle,
        //defaults to dynamic 100x100 viewport, must assign shaders when initializing them
        KNODISCARD
		static Viewport* Initialize(
            u32 contextID,
            ViewportType type = {},
            u32 targetViewport = {});

        KNODISCARD
		u32 GetID() const;
        KNODISCARD
		u32 GetContextID() const;
        KNODISCARD
        u32 GetHitTestID() const;

        KNODISCARD
		u32 GetPrimary3DCameraID() const;
        KNODISCARD
		u32 GetPrimary2DCameraID() const;

        KNODISCARD
		const vector<u32>& GetExtra3DCameraIDs() const;
        KNODISCARD
		const vector<u32>& GetExtra2DCameraIDs() const;

        KNODISCARD
		u32 GetPrimary3DShaderID() const;
        KNODISCARD
		u32 GetPrimary2DShaderID() const;

        KNODISCARD
		const vector<u32>& GetExtra3DShaderIDs() const;
        KNODISCARD
		const vector<u32>& GetExtra2DShaderIDs() const;

        KNODISCARD
        u32 GetTargetViewportID() const;

        KNODISCARD
        ViewportType GetType() const;
        void SetType(ViewportType newType);

        KNODISCARD
        const vec4& GetBackgroundColor() const;
        //Assign a new background color, clamped between 0 and 1
        void SetBackgroundColor(vec4&& newColor);

        KNODISCARD
        const vec4& GetLetterboxColor() const;
        //Assign a new color to the scissor area, clamped between 0 and 1
        void SetLetterboxColor(vec4&& newColor);

        //Returns true if this viewport is the primary viewport
        //for its graphics context, it cannot be destroyed
        KNODISCARD
		bool IsRootViewport() const;

        //If false then viewport is regular viewport and renders on screen,
        //offscreen viewport will draw off screen and requires a render texture
        //on an existing viewport so it can draw its result onto that
        KNODISCARD
		bool IsOffscreenViewport() const;

        //If true then this viewport scales dynamically
        //relative to the size of the graphics context size,
        //viewport size cannot be changed manually if this is true
        KNODISCARD
        bool IsDynamicResizeEnabled() const;
        void SetDynamicResizeState(bool newValue);

        //Returns current viewport size whether its been set as static or dynamic
        KNODISCARD
		vec2 GetSize() const;
        //Set static viewport size, only adjusts aspect ratio, scissor size and scissor offset
        void SetSize(ViewportStaticSize newValue);
        //Set dynamic viewport size
        void SetSize(vec2 newValue);

        KNODISCARD
		vec2 GetOffset() const;
        void SetOffset(vec2 newValue);

        KNODISCARD
		vec2 GetScissorSize() const;
        void SetScissorSize(vec2 newValue);

        KNODISCARD
		vec2 GetScissorOffset() const;
        void SetScissorOffset(vec2 newValue);

        void Destroy();
    private:
        ~Viewport();

        void Update(u32 imageIndex);

        void UpdateViewportSize();

        void _Destroy();

        u32 ID{};
        u32 contextID{};
        u32 hitTestID{};
        u32 targetViewportID{};

        u32 primary3DCameraID{};
        u32 primary2DCameraID{};

        vector<u32> extra3DCameraIDs{};
        vector<u32> extra2DCameraIDs{};

        u32 primary3DShaderID{};
        u32 primary2DShaderID{};

        vector<u32> extra3DShaderIDs{};
        vector<u32> extra2DShaderIDs{};

        //used only to prevent viewport from removing its ID from
        //graphics context viewport IDs list if the graphics context
        //destroy function called the destroy function of this viewport 
        bool isDestroyingGraphicsContext{};

        bool isRootViewport{};
        bool isOffscreenViewport{};

        bool isDynamicResizeEnabled = true;

        ViewportType viewportType{};

        vec4 viewportBackgroundColor = vec4{ 0.0f, 1.0f, 0.0f, 1.0f };
        vec4 viewportLetterboxColor = vec4{ 0.0f, 0.0f, 0.0f, 1.0f };

        ViewportStaticSize viewportStaticSize = ViewportStaticSize::VP_1920_1080;
        vec2 viewportDynamicSize = 100;
        
        //pushes the drawable area down and right if x and y are positive
        vec2 viewportOffset{};

        //cuts everything outside of this area,
        //gpu can only draw clear color there
        vec2 scissorSize{};
        //pushes the clipped area down and right if x and y are positive
        vec2 scissorOffset{};
    };
}