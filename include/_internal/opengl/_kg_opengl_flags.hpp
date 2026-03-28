//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"

namespace KalaGraphics::Internal::OpenGL
{
    using u8 = uint8_t;
    using u32 = uint32_t;
    using f32 = float;
    using f64 = double;

    enum class GLDepthFuncState : u8
    {
        DF_INVALID = 0u,

        DF_LESS = 1u,
        DF_LEQUAL = 2u,
        DF_GREATER = 3u,
        DF_GEQUAL = 4u,
        DF_EQUAL = 5u,
        DF_NOTEQUAL = 6u,
        DF_ALWAYS = 7u,
        DF_NEVER = 8u
    };

    enum class GLCullFaceDirState : u8
    {
        CFD_INVALID = 0u,

        CFD_BACK = 1u,
        CFD_FRONT = 2u,
        CFD_FRONT_AND_BACK = 3u
    };

    enum class GLFrontFaceDirState : u8
    {
        FFD_INVALID = 0u,

        FFD_CCW = 1u,
        FFD_CW = 2u
    };

    enum class GLBlendFuncSource : u8
    {
        BFS_INVALID = 0u,

        //source-only
        BFS_SRC_ALPHA_SATURATE = 1u,

        BFS_ZERO = 2u,
        BFS_ONE = 3u,
        BFS_SRC_COLOR = 4u,
        BFS_ONE_MINUS_SRC_COLOR = 5u,
        BFS_DST_COLOR = 6u,
        BFS_ONE_MINUS_DST_COLOR = 7u,
        BFS_SRC_ALPHA = 8u,
        BFS_ONE_MINUS_SRC_ALPHA = 9u,
        BFS_DST_ALPHA = 10u,
        BFS_ONE_MINUS_DST_ALPHA = 11u,
        BFS_CONSTANT_COLOR = 12u,
        BFS_ONE_MINUS_CONSTANT_COLOR = 13u,
        BFS_CONSTANT_ALPHA = 14u,
        BFS_ONE_MINUS_CONSTANT_ALPHA = 15u
    };
    enum class GLBlendFuncDestination : u8
    {
        BFD_INVALID = 0u,

        BFD_ZERO = 1u,
        BFD_ONE = 2u,
        BFD_SRC_COLOR = 3u,
        BFD_ONE_MINUS_SRC_COLOR = 4u,
        BFD_DST_COLOR = 5u,
        BFD_ONE_MINUS_DST_COLOR = 6u,
        BFD_SRC_ALPHA = 7u,
        BFD_ONE_MINUS_SRC_ALPHA = 8u,
        BFD_DST_ALPHA = 9u,
        BFD_ONE_MINUS_DST_ALPHA = 10u,
        BFD_CONSTANT_COLOR = 11u,
        BFD_ONE_MINUS_CONSTANT_COLOR = 12u,
        BFD_CONSTANT_ALPHA = 13u,
        BFD_ONE_MINUS_CONSTANT_ALPHA = 14u
    };

    enum class GLBlendEquationState : u8
    {
        BE_INVALID = 0u,

        BE_ADD = 1u,
        BE_SUBTRACT = 2u,
        BE_REVERSE_SUBTRACT = 3u,
        BE_MIN = 4u,
        BE_MAX = 5u
    };

    enum class GLStencilFuncState : u8
    {
        SF_INVALID = 0u,

        SF_LESS = 1u,
        SF_LEQUAL = 2u,
        SF_GREATER = 3u,
        SF_GEQUAL = 4u,
        SF_EQUAL = 5u,
        SF_NOTEQUAL = 6u,
        SF_ALWAYS = 7u,
        SF_NEVER = 8u
    };

    enum class GLStencilOpState : u8
    {
        SO_INVALID = 0u,

        SO_KEEP = 1u,
        SO_ZERO = 2u,
        SO_REPLACE = 3u,
        SO_INCR = 4u,
        SO_INCR_WRAP = 5u,
        SO_DECR = 6u,
        SO_DECR_WRAP = 7u,
        SO_INVERT = 8u
    };

    enum class GLPolygonModeState : u8
    {
        PM_INVALID = 0u,

        PM_FILL = 1u,
        PM_LINE = 2u,
        PM_POINT = 3u
    };

    class LIB_API OpenGL_Flags
    {
    public:
        //Turn depth testing on/off
        static void SetGLDepthTestState(
            u32 contextID,
            bool state);
        //How to test
        static void SetGLDepthFuncState(
            u32 contextID,
            GLDepthFuncState state);
        //Whether to write the result
        static void SetGLDepthWriteMaskState(
            u32 contextID,
            bool state);

        //Turn face culling on/off
        static void SetGLCullFaceState(
            u32 contextID,
            bool state);
        //Which face to cull
        static void SetGLCullFaceDirState(
            u32 contextID,
            GLCullFaceDirState state);
        //Which winding order is front-facing
        static void SetGLFrontFaceDirState(
            u32 contextID,
            GLFrontFaceDirState state);

        //Turn blending on/off
        static void SetGLBlendState(
            u32 contextID,
            bool state);
        //What to blend with
        static void SetGLBlendFuncState(
            u32 contextID,
            GLBlendFuncSource source,
            GLBlendFuncDestination dest);
        //How to blend
        static void SetGLBlendEquationState(
            u32 contextID,
            GLBlendEquationState state);

        //Turn stencil testing on/off
        static void SetGLStencilTestState(
            u32 contextID,
            bool state);
        //How to test
        static void SetGLStencilFuncState(
            u32 contextID,
            GLStencilFuncState state,
            u8 ref,
            u8 mask);
        //What to do with the result
        static void SetGLStencilOpState(
            u32 contextID,
            GLStencilOpState sFail,
            GLStencilOpState dpFail,
            GLStencilOpState dpPass);

        //Which color channels get written
        static void SetGLColorWriteMaskState(
            u32 contextID,
            bool r,
            bool g,
            bool b,
            bool a);

        //How to rasterize polygons
        static void SetGLPolygonModeState(
            u32 contextID,
            GLPolygonModeState state);

        //Turn polygon offset on/off
        static void SetGLPolygonOffsetState(
            u32 contextID,
            bool state,
            GLPolygonModeState type);
        //Set polygon offset factor and units
        static void SetGLPolygonOffsetValues(
            u32 contextID,
            f32 factor,
            f32 units);

        //Turn scissor testing on/off
        static void SetGLScissorTestState(
            u32 contextID,
            bool state);
        //Define the scissor region
        static void SetGLScissorRectState(
            u32 contextID,
            u32 x,
            u32 y,
            u32 width,
            u32 height);

        //Width of rasterized lines
        static void SetGLLineWidth(
            u32 contextID,
            f32 width);
        //Size of rasterized points
        static void SetGLPointSize(
            u32 contextID,
            f32 size);

        //Set the color used when clearing the color buffer
        static void SetGLClearColor(
            u32 contextID,
            f32 r,
            f32 g,
            f32 b,
            f32 a);
        //Set the depth value used when clearing the depth buffer
        static void SetGLClearDepth(
            u32 contextID,
            f64 value);
        //Set the stencil value used when clearing the stencil buffer
        static void SetGLClearStencil(
            u32 contextID,
            u8 value);
        //Clear one or more buffers
        static void ClearBuffers(
            u32 contextID,
            bool color,
            bool depth = false,
            bool stencil = false);
    };
}