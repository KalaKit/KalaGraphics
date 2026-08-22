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

    class LIB_API Viewport
    {
    friend class KalaGraphics::Resources::Shader;
    friend class KalaGraphics::Resources::Mesh;
    friend class KalaGraphics::Resources::Texture;
    friend class KalaGraphics::Resources::Camera;
    friend class GraphicsContext;
    friend struct default_delete<Viewport>;
    public:
        static KalaGraphicsRegistry<Viewport>& GetRegistry();

        static string_view GetViewportStaticName(ViewportStaticSize vpSize);
        static vec2 GetViewportStaticValue(ViewportStaticSize vpSize);

        static Viewport* Initialize(
            u32 contextID,
            bool isStatic = false);

        u32 GetID() const;
        u32 GetContextID() const;
        const vector<u32>& GetShaderIDs() const;

        //Returns true if this viewport is the primary viewport
        //for its graphics context, it cannot be destroyed
        bool IsRootViewport() const;

        bool IsStaticViewport() const;
        void SetStaticViewportState(bool state);

        //Gets either static or dynamic viewport size depending on the bool state
        vec2 GetViewportSize(bool getStatic) const;
        //Sets static viewport size, cannot be set if viewport is dynamic
        void SetViewportSize(ViewportStaticSize vpSize);
        //Sets dynamic viewport size, cannot be set if viewport is static
        void SetViewportSize(vec2 vpSize);

        //If true then viewport resizes dynamically with the true window size
        bool IsDynamicViewport() const;
        void SetDynamicViewportState(bool newValue);

        vec2 GetViewportOffset() const;
        void SetViewportOffset(vec2 newSize);

        vec2 GetScissorSize() const;
        void SetScissorSize(vec2 newOffset);

        vec2 GetScissorOffset() const;
        void SetScissorOffset(vec2 newSize);

        void Destroy();
    private:
        ~Viewport();

        void _Destroy();

        //used only to prevent viewport from removing its ID from
        //graphics context viewport IDs list if the graphics context
        //destroy function called the destroy function of this viewport 
        bool isDestroyingGraphicsContext{};

        u32 ID{};
        u32 contextID{};

        //shaders that use this viewport
        vector<u32> shaderIDs{};

        bool isRootViewport{};

        bool isStaticViewport = true;

        ViewportStaticSize viewportStaticSize = ViewportStaticSize::VP_1920_1080;
        vec2 viewportDynamicSize{};
        
        //pushes the drawable area down and right if x and y are positive
        vec2 viewportOffset{};
        //min and max depth
        vec2 viewportDepth = vec2(0, 1);

        //pushes the clipped area down and right if x and y are positive
        vec2 scissorOffset{};
        //cuts everything outside of this area,
        //gpu can only draw clear color there
        vec2 scissorSize{};
    };
}