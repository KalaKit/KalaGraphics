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
        //Standard viewport
        VP_NORMAL = 0,

        //Offscreen viewport, required to always be static, requires target viewport to render to
        VP_OFFSCREEN = 1
    };

    class LIB_API Viewport
    {
    friend class KalaGraphics::Resources::Shader;
    friend class KalaGraphics::Resources::Mesh;
    friend class KalaGraphics::Resources::Texture;
    friend class KalaGraphics::Resources::Camera;
    friend class GraphicsContext;
    friend struct default_delete<Viewport>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<Viewport>& GetRegistry();

        KNODISCARD
		static string_view GetViewportStaticName(ViewportStaticSize vpSize);
        KNODISCARD
		static vec2 GetViewportStaticValue(ViewportStaticSize vpSize);

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
		u32 GetPrimary3DCameraID() const;

        KNODISCARD
		const vector<u32>& GetExtra3DCameraIDs() const;
        KNODISCARD
		const vector<u32>& GetExtra2DCameraIDs() const;

        KNODISCARD
		u32 GetPrimary2DCameraID() const;

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
        ViewportType GetViewportType() const;

        //Returns true if this viewport is the primary viewport
        //for its graphics context, it cannot be destroyed
        KNODISCARD
		bool IsRootViewport() const;

        //If false then viewport resizes dynamically with the true os window size
        KNODISCARD
		bool IsStaticViewport() const;
        void SetStaticViewportState(bool newValue);

        KNODISCARD
		vec2 GetViewportSize(bool isStatic) const;
        //Set static viewport size, only adjusts aspect ratio, scissor size and scissor offset
        void SetViewportSize(ViewportStaticSize newValue);
        //Set dynamic viewport size
        void SetViewportSize(vec2 newValue);

        KNODISCARD
		vec2 GetViewportOffset() const;
        void SetViewportOffset(vec2 newValue);

        KNODISCARD
		vec2 GetScissorSize() const;
        void SetScissorSize(vec2 newValue);

        KNODISCARD
		vec2 GetScissorOffset() const;
        void SetScissorOffset(vec2 newValue);

        void Destroy();
    private:
        ~Viewport();

        //Initialize root viewport, separate from regular viewport initialization
        //that requires an already existing graphics context
        static Viewport* _Initialize();

        void Update(u32 imageIndex);

        void _Destroy();

        u32 ID{};
        u32 contextID{};
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

        bool isStaticViewport = true;

        ViewportType viewportType{};

        ViewportStaticSize viewportStaticSize = ViewportStaticSize::VP_1920_1080;
        vec2 viewportDynamicSize = 100;
        
        //pushes the drawable area down and right if x and y are positive
        vec2 viewportOffset{};

        //pushes the clipped area down and right if x and y are positive
        vec2 scissorOffset{};
        //cuts everything outside of this area,
        //gpu can only draw clear color there
        vec2 scissorSize{};
    };
}