//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "_internal/opengl/_kg_opengl.hpp"
#include <GL/glext.h>
#ifdef _WIN32
#include <GL/gl.h>
#else
#include <GL/glx.h>
#endif

#include <string>
#include <unordered_map>

#include "core_utils.hpp"
#include "log_utils.hpp"
#include "string_utils.hpp"

#include "_internal/opengl/_kg_opengl_flags.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaCore::EnumHash;
using KalaHeaders::KalaCore::EnumToString;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::BoolValue;

using KalaGraphics::Core::WindowContext;

using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;

constexpr string_view depth_func_state_less     = "less";
constexpr string_view depth_func_state_lequal   = "lequal";
constexpr string_view depth_func_state_greater  = "greater";
constexpr string_view depth_func_state_gequal   = "gequal";
constexpr string_view depth_func_state_equal    = "equal";
constexpr string_view depth_func_state_notequal = "notequal";
constexpr string_view depth_func_state_always   = "always";
constexpr string_view depth_func_state_never    = "never";

constexpr string_view cull_face_dir_back  = "back";
constexpr string_view cull_face_dir_front = "front";
constexpr string_view cull_face_dir_fb    = "front-and-back";

constexpr string_view front_face_dir_ccw = "ccw";
constexpr string_view front_face_dir_cw  = "cw";

constexpr string_view blend_func_source_alsat = "src_alpha_saturate";

constexpr string_view blend_func_any_zero   = "zero";
constexpr string_view blend_func_any_one    = "one";
constexpr string_view blend_func_any_scol   = "src-color";
constexpr string_view blend_func_any_omscol = "one-minus-src-color";
constexpr string_view blend_func_any_dcol   = "dst-color";
constexpr string_view blend_func_any_omdcol = "one-minus-dst-color";
constexpr string_view blend_func_any_sal    = "src-alpha";
constexpr string_view blend_func_any_omsal  = "one-minus-src-alpha";
constexpr string_view blend_func_any_dal    = "dst-alpha";
constexpr string_view blend_func_any_omdal  = "one-minus-dst-alpha";
constexpr string_view blend_func_any_ccol   = "constant-color";
constexpr string_view blend_func_any_omccol = "one-minus-constant-color";
constexpr string_view blend_func_any_cal    = "constant-alpha";
constexpr string_view blend_func_any_omcal  = "one-minus-constant-alpha";

constexpr string_view blend_func_equat_add    = "add";
constexpr string_view blend_func_equat_sub    = "subtract";
constexpr string_view blend_func_equat_revsub = "reverse-subtract";
constexpr string_view blend_func_equat_min    = "min";
constexpr string_view blend_func_equat_max    = "max";

constexpr string_view stencil_func_state_less     = "less";
constexpr string_view stencil_func_state_lequal   = "lequal";
constexpr string_view stencil_func_state_greater  = "greater";
constexpr string_view stencil_func_state_gequal   = "gequal";
constexpr string_view stencil_func_state_equal    = "equal";
constexpr string_view stencil_func_state_notequal = "notequal";
constexpr string_view stencil_func_state_always   = "always";
constexpr string_view stencil_func_state_never    = "never";

constexpr string_view stencil_op_state_keep      = "keep";
constexpr string_view stencil_op_state_zero      = "zero";
constexpr string_view stencil_op_state_replace   = "replace";
constexpr string_view stencil_op_state_incr      = "incr";
constexpr string_view stencil_op_state_incr_wrap = "incr-wrap";
constexpr string_view stencil_op_state_decr      = "decr";
constexpr string_view stencil_op_state_decr_wrap = "decr-wrap";
constexpr string_view stencil_op_state_invert    = "invert";

constexpr string_view polygon_mode_state_fill  = "fill";
constexpr string_view polygon_mode_state_line  = "line";
constexpr string_view polygon_mode_state_point = "point";

namespace KalaGraphics::Internal::OpenGL
{
	static const unordered_map<GLDepthFuncState, string_view, EnumHash<GLDepthFuncState>> depthFuncStates =
	{
		{ GLDepthFuncState::DF_LESS,     depth_func_state_less },
		{ GLDepthFuncState::DF_LEQUAL,   depth_func_state_lequal },
		{ GLDepthFuncState::DF_GREATER,  depth_func_state_greater },
		{ GLDepthFuncState::DF_GEQUAL,   depth_func_state_gequal },
        { GLDepthFuncState::DF_EQUAL,    depth_func_state_equal },
        { GLDepthFuncState::DF_NOTEQUAL, depth_func_state_notequal },
        { GLDepthFuncState::DF_ALWAYS,   depth_func_state_always },
        { GLDepthFuncState::DF_NEVER,    depth_func_state_never }
	};

    static const unordered_map<GLCullFaceDirState, string_view, EnumHash<GLCullFaceDirState>> cullFaceDirs =
	{
		{ GLCullFaceDirState::CFD_BACK,           cull_face_dir_back },
		{ GLCullFaceDirState::CFD_FRONT,          cull_face_dir_front },
		{ GLCullFaceDirState::CFD_FRONT_AND_BACK, cull_face_dir_fb }
	};

    static const unordered_map<GLFrontFaceDirState, string_view, EnumHash<GLFrontFaceDirState>> frontFaceDirs =
	{
		{ GLFrontFaceDirState::FFD_CCW, front_face_dir_ccw },
		{ GLFrontFaceDirState::FFD_CW,  front_face_dir_cw }
	};

    static const unordered_map<GLBlendFuncSource, string_view, EnumHash<GLBlendFuncSource>> blendFuncSources =
	{
		{ GLBlendFuncSource::BFS_SRC_ALPHA_SATURATE, blend_func_source_alsat },

        { GLBlendFuncSource::BFS_ZERO,                     blend_func_any_zero },
        { GLBlendFuncSource::BFS_ONE,                      blend_func_any_one },
        { GLBlendFuncSource::BFS_SRC_COLOR,                blend_func_any_scol },
        { GLBlendFuncSource::BFS_ONE_MINUS_SRC_COLOR,      blend_func_any_omscol },
        { GLBlendFuncSource::BFS_DST_COLOR,                blend_func_any_dcol },
        { GLBlendFuncSource::BFS_ONE_MINUS_DST_COLOR,      blend_func_any_omdcol },
        { GLBlendFuncSource::BFS_SRC_ALPHA,                blend_func_any_sal },
        { GLBlendFuncSource::BFS_ONE_MINUS_SRC_ALPHA,      blend_func_any_omsal },
        { GLBlendFuncSource::BFS_DST_ALPHA,                blend_func_any_dal },
        { GLBlendFuncSource::BFS_ONE_MINUS_DST_ALPHA,      blend_func_any_omdal },
        { GLBlendFuncSource::BFS_CONSTANT_COLOR,           blend_func_any_ccol },
        { GLBlendFuncSource::BFS_ONE_MINUS_CONSTANT_COLOR, blend_func_any_omccol },
        { GLBlendFuncSource::BFS_CONSTANT_ALPHA,           blend_func_any_cal },
        { GLBlendFuncSource::BFS_ONE_MINUS_CONSTANT_ALPHA, blend_func_any_omcal }
	};

    static const unordered_map<GLBlendFuncDestination, string_view, EnumHash<GLBlendFuncDestination>> blendFuncDestinations =
	{
		{ GLBlendFuncDestination::BFD_ZERO,                     blend_func_any_zero },
        { GLBlendFuncDestination::BFD_ONE,                      blend_func_any_one },
        { GLBlendFuncDestination::BFD_SRC_COLOR,                blend_func_any_scol },
        { GLBlendFuncDestination::BFD_ONE_MINUS_SRC_COLOR,      blend_func_any_omscol },
        { GLBlendFuncDestination::BFD_DST_COLOR,                blend_func_any_dcol },
        { GLBlendFuncDestination::BFD_ONE_MINUS_DST_COLOR,      blend_func_any_omdcol },
        { GLBlendFuncDestination::BFD_SRC_ALPHA,                blend_func_any_sal },
        { GLBlendFuncDestination::BFD_ONE_MINUS_SRC_ALPHA,      blend_func_any_omsal },
        { GLBlendFuncDestination::BFD_DST_ALPHA,                blend_func_any_dal },
        { GLBlendFuncDestination::BFD_ONE_MINUS_DST_ALPHA,      blend_func_any_omdal },
        { GLBlendFuncDestination::BFD_CONSTANT_COLOR,           blend_func_any_ccol },
        { GLBlendFuncDestination::BFD_ONE_MINUS_CONSTANT_COLOR, blend_func_any_omccol },
        { GLBlendFuncDestination::BFD_CONSTANT_ALPHA,           blend_func_any_cal },
        { GLBlendFuncDestination::BFD_ONE_MINUS_CONSTANT_ALPHA, blend_func_any_omcal }
	};

    static const unordered_map<GLBlendEquationState, string_view, EnumHash<GLBlendEquationState>> blendFuncEquations =
	{
		{ GLBlendEquationState::BE_ADD,              blend_func_equat_add },
        { GLBlendEquationState::BE_SUBTRACT,         blend_func_equat_sub },
        { GLBlendEquationState::BE_REVERSE_SUBTRACT, blend_func_equat_revsub },
        { GLBlendEquationState::BE_MIN,              blend_func_equat_min },
        { GLBlendEquationState::BE_MAX,              blend_func_equat_max }
	};

    static const unordered_map<GLStencilFuncState, string_view, EnumHash<GLStencilFuncState>> stencilFuncStates =
	{
		{ GLStencilFuncState::SF_LESS,     stencil_func_state_less },
		{ GLStencilFuncState::SF_LEQUAL,   stencil_func_state_lequal },
		{ GLStencilFuncState::SF_GREATER,  stencil_func_state_greater },
		{ GLStencilFuncState::SF_GEQUAL,   stencil_func_state_gequal },
        { GLStencilFuncState::SF_EQUAL,    stencil_func_state_equal },
        { GLStencilFuncState::SF_NOTEQUAL, stencil_func_state_notequal },
        { GLStencilFuncState::SF_ALWAYS,   stencil_func_state_always },
        { GLStencilFuncState::SF_NEVER,    stencil_func_state_never }
	};

    static const unordered_map<GLStencilOpState, string_view, EnumHash<GLStencilOpState>> stencilOpStates =
	{
		{ GLStencilOpState::SO_KEEP,      stencil_op_state_keep },
		{ GLStencilOpState::SO_ZERO,      stencil_op_state_zero },
		{ GLStencilOpState::SO_REPLACE,   stencil_op_state_replace },
		{ GLStencilOpState::SO_INCR,      stencil_op_state_incr },
        { GLStencilOpState::SO_INCR_WRAP, stencil_op_state_incr_wrap },
        { GLStencilOpState::SO_DECR,      stencil_op_state_decr },
        { GLStencilOpState::SO_DECR_WRAP, stencil_op_state_decr_wrap },
        { GLStencilOpState::SO_INVERT,    stencil_op_state_invert }
	};

    static const unordered_map<GLPolygonModeState, string_view, EnumHash<GLPolygonModeState>> polygonModeStates =
	{
		{ GLPolygonModeState::PM_FILL,  polygon_mode_state_fill },
		{ GLPolygonModeState::PM_LINE,  polygon_mode_state_line },
		{ GLPolygonModeState::PM_POINT, polygon_mode_state_point }
	};

    static bool ContextExists(u32 contextID)
    {
        return WindowContext::GetRegistry().createdContent.contains(contextID);
    } 

    void OpenGL_Flags::SetGLDepthTestState(
        u32 contextID,
        bool state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL depth test state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        OpenGL_Core::MakeContextCurrent(contextID);

        if (state) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);

        Log::Print(
            "Set GL depth test state for context '" + to_string(contextID) + "' to '" + string(BoolValue(state)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLDepthFuncState(
        u32 contextID,
        GLDepthFuncState state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL depth func state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        switch (state)
        {
            default:
            case GLDepthFuncState::DF_INVALID:
            {
                Log::Print(
                    "Failed to set GL depth func state because the passed state value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLDepthFuncState::DF_LESS:
            {
                glDepthFunc(GL_LESS);
                break;
            }
            case GLDepthFuncState::DF_LEQUAL:
            {
                glDepthFunc(GL_LEQUAL);
                break;
            }
            case GLDepthFuncState::DF_GREATER:
            {
                glDepthFunc(GL_GREATER);
                break;
            }
            case GLDepthFuncState::DF_GEQUAL:
            {
                glDepthFunc(GL_GEQUAL);
                break;
            }
            case GLDepthFuncState::DF_EQUAL:
            {
                glDepthFunc(GL_EQUAL);
                break;
            }
            case GLDepthFuncState::DF_NOTEQUAL:
            {
                glDepthFunc(GL_NOTEQUAL);
                break;
            }
            case GLDepthFuncState::DF_ALWAYS:
            {
                glDepthFunc(GL_ALWAYS);
                break;
            }
            case GLDepthFuncState::DF_NEVER:
            {
                glDepthFunc(GL_NEVER);
                break;
            }
        }

        string_view result{};
        EnumToString(state, depthFuncStates, result);

        Log::Print(
            "Set GL depth func state for context '" + to_string(contextID) + "' to '" + string(result) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLDepthWriteMaskState(
        u32 contextID,
        bool state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL depth write mask state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        OpenGL_Core::MakeContextCurrent(contextID);

        if (state) glDepthMask(GL_TRUE);
        else glDepthMask(GL_FALSE);

        Log::Print(
            "Set GL depth write mask state for context '" + to_string(contextID) + "' to '" + string(BoolValue(state)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLCullFaceState(
        u32 contextID,
        bool state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL cull face state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        OpenGL_Core::MakeContextCurrent(contextID);

        if (state) glEnable(GL_CULL_FACE);
        else glDisable(GL_CULL_FACE);

        Log::Print(
            "Set GL cull face state for context '" + to_string(contextID) + "' to '" + string(BoolValue(state)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLCullFaceDirState(
        u32 contextID,
        GLCullFaceDirState state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL cull face dir because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        switch (state)
        {
            default:
            case GLCullFaceDirState::CFD_INVALID:
            {
                Log::Print(
                    "Failed to set GL cull face dir because the passed state value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLCullFaceDirState::CFD_BACK:
            {
                glCullFace(GL_BACK);
                break;
            }
            case GLCullFaceDirState::CFD_FRONT:
            {
                glCullFace(GL_FRONT);
                break;
            }
            case GLCullFaceDirState::CFD_FRONT_AND_BACK:
            {
                glCullFace(GL_FRONT_AND_BACK);
                break;
            }
        }

        string_view result{};
        EnumToString(state, cullFaceDirs, result);

        Log::Print(
            "Set GL cull face dir for context '" + to_string(contextID) + "' to '" + string(result) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLFrontFaceDirState(
        u32 contextID,
        GLFrontFaceDirState state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL front face dir because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        switch (state)
        {
            default:
            case GLFrontFaceDirState::FFD_INVALID:
            {
                Log::Print(
                    "Failed to set GL front face dir because the passed state value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLFrontFaceDirState::FFD_CCW:
            {
                glFrontFace(GL_CCW);
                break;
            }
            case GLFrontFaceDirState::FFD_CW:
            {
                glFrontFace(GL_CW);
                break;
            }
        }

        string_view result{};
        EnumToString(state, frontFaceDirs, result);

        Log::Print(
            "Set GL front face dir for context '" + to_string(contextID) + "' to '" + string(result) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLBlendState(
        u32 contextID,
        bool state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL blend state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        OpenGL_Core::MakeContextCurrent(contextID);

        if (state) glEnable(GL_BLEND);
        else glDisable(GL_BLEND);

        Log::Print(
            "Set GL blend state for context '" + to_string(contextID) + "' to '" + string(BoolValue(state)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLBlendFuncState(
        u32 contextID,
        GLBlendFuncSource source,
        GLBlendFuncDestination dest)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL blend func state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        OpenGL_Core::MakeContextCurrent(contextID);

        GLenum src{};
        GLenum dst{};

        switch (source)
        {
            default:
            case GLBlendFuncSource::BFS_INVALID:
            {
                Log::Print(
                    "Failed to set GL blend func state because the passed source value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLBlendFuncSource::BFS_SRC_ALPHA_SATURATE:
            {
                src = GL_SRC_ALPHA_SATURATE;
                break;
            }

            case GLBlendFuncSource::BFS_ZERO:
            {
                src = GL_ZERO;
                break;
            }
            case GLBlendFuncSource::BFS_ONE:
            {
                src = GL_ONE;
                break;
            }
            case GLBlendFuncSource::BFS_SRC_COLOR:
            {
                src = GL_SRC_COLOR;
                break;
            }
            case GLBlendFuncSource::BFS_ONE_MINUS_SRC_COLOR:
            {
                src = GL_ONE_MINUS_SRC_COLOR;
                break;
            }
            case GLBlendFuncSource::BFS_DST_COLOR:
            {
                src = GL_DST_COLOR;
                break;
            }
            case GLBlendFuncSource::BFS_ONE_MINUS_DST_COLOR:
            {
                src = GL_ONE_MINUS_DST_COLOR;
                break;
            }
            case GLBlendFuncSource::BFS_SRC_ALPHA:
            {
                src = GL_SRC_ALPHA;
                break;
            }
            case GLBlendFuncSource::BFS_ONE_MINUS_SRC_ALPHA:
            {
                src = GL_ONE_MINUS_SRC_ALPHA;
                break;
            }
            case GLBlendFuncSource::BFS_DST_ALPHA:
            {
                src = GL_DST_ALPHA;
                break;
            }
            case GLBlendFuncSource::BFS_ONE_MINUS_DST_ALPHA:
            {
                src = GL_ONE_MINUS_DST_ALPHA;
                break;
            }
            case GLBlendFuncSource::BFS_CONSTANT_COLOR:
            {
                src = GL_CONSTANT_COLOR;
                break;
            }
            case GLBlendFuncSource::BFS_ONE_MINUS_CONSTANT_COLOR:
            {
                src = GL_ONE_MINUS_CONSTANT_COLOR;
                break;
            }
            case GLBlendFuncSource::BFS_CONSTANT_ALPHA:
            {
                src = GL_CONSTANT_ALPHA;
                break;
            }
            case GLBlendFuncSource::BFS_ONE_MINUS_CONSTANT_ALPHA:
            {
                src = GL_ONE_MINUS_CONSTANT_ALPHA;
                break;
            }
        }

        switch (dest)
        {
            default:
            case GLBlendFuncDestination::BFD_INVALID:
            {
                Log::Print(
                    "Failed to set GL blend func state because the passed destination value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLBlendFuncDestination::BFD_ZERO:
            {
                dst = GL_ZERO;
                break;
            }
            case GLBlendFuncDestination::BFD_ONE:
            {
                dst = GL_ONE;
                break;
            }
            case GLBlendFuncDestination::BFD_SRC_COLOR:
            {
                dst = GL_SRC_COLOR;
                break;
            }
            case GLBlendFuncDestination::BFD_ONE_MINUS_SRC_COLOR:
            {
                dst = GL_ONE_MINUS_SRC_COLOR;
                break;
            }
            case GLBlendFuncDestination::BFD_DST_COLOR:
            {
                dst = GL_DST_COLOR;
                break;
            }
            case GLBlendFuncDestination::BFD_ONE_MINUS_DST_COLOR:
            {
                dst = GL_ONE_MINUS_DST_COLOR;
                break;
            }
            case GLBlendFuncDestination::BFD_SRC_ALPHA:
            {
                dst = GL_SRC_ALPHA;
                break;
            }
            case GLBlendFuncDestination::BFD_ONE_MINUS_SRC_ALPHA:
            {
                dst = GL_ONE_MINUS_SRC_ALPHA;
                break;
            }
            case GLBlendFuncDestination::BFD_DST_ALPHA:
            {
                dst = GL_DST_ALPHA;
                break;
            }
            case GLBlendFuncDestination::BFD_ONE_MINUS_DST_ALPHA:
            {
                dst = GL_ONE_MINUS_DST_ALPHA;
                break;
            }
            case GLBlendFuncDestination::BFD_CONSTANT_COLOR:
            {
                dst = GL_CONSTANT_COLOR;
                break;
            }
            case GLBlendFuncDestination::BFD_ONE_MINUS_CONSTANT_COLOR:
            {
                dst = GL_ONE_MINUS_CONSTANT_COLOR;
                break;
            }
            case GLBlendFuncDestination::BFD_CONSTANT_ALPHA:
            {
                dst = GL_CONSTANT_ALPHA;
                break;
            }
            case GLBlendFuncDestination::BFD_ONE_MINUS_CONSTANT_ALPHA:
            {
                dst = GL_ONE_MINUS_CONSTANT_ALPHA;
                break;
            }
        }

        glBlendFunc(src, dst);

        string_view srcResult{};
        EnumToString(source, blendFuncSources, srcResult);

        string_view dstResult{};
        EnumToString(dest, blendFuncDestinations, dstResult);

        Log::Print(
            "Set GL blend func state for context '" 
            + to_string(contextID) + "' to '" 
            + string(srcResult) + "' and '" 
            + string(dstResult) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLBlendEquationState(
        u32 contextID,
        GLBlendEquationState state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL blend equation state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        OpenGL_Core::MakeContextCurrent(contextID);

        switch (state)
        {
            default:
            case GLBlendEquationState::BE_INVALID:
            {
                Log::Print(
                    "Failed to set GL blend equation state because the passed state value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLBlendEquationState::BE_ADD:
            {
                glBlendEquation(GL_FUNC_ADD);
                break;
            }
            case GLBlendEquationState::BE_SUBTRACT:
            {
                glBlendEquation(GL_FUNC_SUBTRACT);
                break;
            }
            case GLBlendEquationState::BE_REVERSE_SUBTRACT:
            {
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                break;
            }
            case GLBlendEquationState::BE_MIN:
            {
                glBlendEquation(GL_MIN);
                break;
            }
            case GLBlendEquationState::BE_MAX:
            {
                glBlendEquation(GL_MAX);
                break;
            }
        }

        string_view result{};
        EnumToString(state, blendFuncEquations, result);

        Log::Print(
            "Set GL blend func equation for context '" + to_string(contextID) + "' to '" + string(result) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLStencilTestState(
        u32 contextID,
        bool state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL stencil test state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        OpenGL_Core::MakeContextCurrent(contextID);

        if (state) glEnable(GL_STENCIL_TEST);
        else glDisable(GL_STENCIL_TEST);

        Log::Print(
            "Set GL stencil test state for context '" + to_string(contextID) + "' to '" + string(BoolValue(state)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLStencilFuncState(
        u32 contextID,
        GLStencilFuncState state,
        u8 ref,
        u8 mask)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL stencil func state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        switch (state)
        {
            default:
            case GLStencilFuncState::SF_INVALID:
            {
                Log::Print(
                    "Failed to set GL stencil func state because the passed state value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLStencilFuncState::SF_LESS:
            {
                glStencilFunc(GL_LESS, ref, mask);
                break;
            }
            case GLStencilFuncState::SF_LEQUAL:
            {
                glStencilFunc(GL_LEQUAL, ref, mask);
                break;
            }
            case GLStencilFuncState::SF_GREATER:
            {
                glStencilFunc(GL_GREATER, ref, mask);
                break;
            }
            case GLStencilFuncState::SF_GEQUAL:
            {
                glStencilFunc(GL_GEQUAL, ref, mask);
                break;
            }
            case GLStencilFuncState::SF_EQUAL:
            {
                glStencilFunc(GL_EQUAL, ref, mask);
                break;
            }
            case GLStencilFuncState::SF_NOTEQUAL:
            {
                glStencilFunc(GL_NOTEQUAL, ref, mask);
                break;
            }
            case GLStencilFuncState::SF_ALWAYS:
            {
                glStencilFunc(GL_ALWAYS, ref, mask);
                break;
            }
            case GLStencilFuncState::SF_NEVER:
            {
                glStencilFunc(GL_NEVER, ref, mask);
                break;
            }
        }

        string_view result{};
        EnumToString(state, stencilFuncStates, result);

        Log::Print(
            "Set GL stencil func state for context '" + to_string(contextID) + "' to '" 
            + string(result) + "', '" 
            + to_string(ref) + "' and '" 
            + to_string(mask) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLStencilOpState(
        u32 contextID,
        GLStencilOpState sFail,
        GLStencilOpState dpFail,
        GLStencilOpState dpPass)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL stencil op state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        auto set_stencil_op_enum = [&contextID](GLStencilOpState state, string_view targetType) -> GLenum
            {
                switch (state)
                {
                    default:
                    case GLStencilOpState::SO_INVALID:
                    {
                        Log::Print(
                            "Failed to set GL stencil op state for context '" + to_string(contextID) + "' because the passed " + string(targetType) + " value was invalid!",
                            "KG_GL_FLAGS",
                            LogType::LOG_ERROR,
                            2);

                        return GL_INVALID_ENUM;
                    }

                    case GLStencilOpState::SO_KEEP:      return GL_KEEP;
                    case GLStencilOpState::SO_ZERO:      return GL_ZERO;
                    case GLStencilOpState::SO_REPLACE:   return GL_REPLACE;
                    case GLStencilOpState::SO_INCR:      return GL_INCR;
                    case GLStencilOpState::SO_INCR_WRAP: return GL_INCR_WRAP;
                    case GLStencilOpState::SO_DECR:      return GL_DECR;
                    case GLStencilOpState::SO_DECR_WRAP: return GL_DECR_WRAP;
                    case GLStencilOpState::SO_INVERT:    return GL_INVERT;
                }
            };

        GLenum sFailValue = set_stencil_op_enum(sFail, "sFail");
        GLenum dpFailValue = set_stencil_op_enum(dpFail, "dpFail");
        GLenum dpPassValue = set_stencil_op_enum(dpPass, "dpPass");

        if (sFailValue != GL_INVALID_ENUM
            && dpFailValue != GL_INVALID_ENUM
            && dpPassValue != GL_INVALID_ENUM)
        {
            glStencilOp(sFailValue, dpFailValue, dpPassValue);

            string_view sFailStr{};
            EnumToString(sFail, stencilOpStates, sFailStr);

            string_view dpFailStr{};
            EnumToString(dpFail, stencilOpStates, dpFailStr);

            string_view dpPassStr{};
            EnumToString(dpPass, stencilOpStates, dpPassStr);

            Log::Print(
                "Set GL stencil op state for context '" + to_string(contextID) + "' to '" 
                + string(sFailStr) + "', '" 
                + string(dpFailStr) + "' and '"
                + string(dpPassStr) + "'.",
                "KG_GL_FLAGS",
                LogType::LOG_SUCCESS);
        }
    }

    void OpenGL_Flags::SetGLColorWriteMaskState(
        u32 contextID,
        bool r,
        bool g,
        bool b,
        bool a)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL color write mask state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        glColorMask(
            r ? GL_TRUE : GL_FALSE,
            g ? GL_TRUE : GL_FALSE,
            b ? GL_TRUE : GL_FALSE,
            a ? GL_TRUE : GL_FALSE);

        Log::Print(
            "Set GL color write mask state for context '" + to_string(contextID) + "' to '" 
            + string(BoolValue(r)) + "', '" 
            + string(BoolValue(g)) + "', '" 
            + string(BoolValue(b)) + "' and '"
            + string(BoolValue(a)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLPolygonModeState(
        u32 contextID,
        GLPolygonModeState state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL polygon mode state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        switch (state)
        {
            default:
            case GLPolygonModeState::PM_INVALID:
            {
                Log::Print(
                    "Failed to set GL polygon mode state because the passed state value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLPolygonModeState::PM_FILL:
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                break;
            }
            case GLPolygonModeState::PM_LINE:
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                break;
            }
            case GLPolygonModeState::PM_POINT:
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
                break;
            }
        }

        string_view result{};
        EnumToString(state, polygonModeStates, result);

        Log::Print(
            "Set GL polygon mode state for context '" + to_string(contextID) + "' to '" 
            + string(result) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLPolygonOffsetState(
        u32 contextID,
        bool state,
        GLPolygonModeState type)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL polygon offset state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        switch (type)
        {
            default:
            case GLPolygonModeState::PM_INVALID:
            {
                Log::Print(
                    "Failed to set GL polygon offset state because the passed state value was invalid!",
                    "KG_GL_FLAGS",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            case GLPolygonModeState::PM_FILL:
            {
                if (state) glEnable(GL_POLYGON_OFFSET_FILL);
                else glDisable(GL_POLYGON_OFFSET_FILL);
                break;
            }
            case GLPolygonModeState::PM_LINE:
            {
                if (state) glEnable(GL_POLYGON_OFFSET_LINE);
                else glDisable(GL_POLYGON_OFFSET_LINE);
                break;
            }
            case GLPolygonModeState::PM_POINT:
            {
                if (state) glEnable(GL_POLYGON_OFFSET_POINT);
                else glDisable(GL_POLYGON_OFFSET_POINT);
                break;
            }
        }

        string_view result{};
        EnumToString(type, polygonModeStates, result);

        Log::Print(
            "Set GL polygon offset state for context '" + to_string(contextID) + "' to '" 
            + string(BoolValue(state)) + "' and '" 
            + string(result) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLPolygonOffsetValues(
        u32 contextID,
        f32 factor,
        f32 units)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL polygon offset values because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        glPolygonOffset(factor, units);

        Log::Print(
            "Set GL polygon offset values for context '" + to_string(contextID) + "' to '" 
            + to_string(factor) + "' and '" 
            + to_string(units) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLScissorTestState(
        u32 contextID,
        bool state)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL scissor test state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        if (state) glEnable(GL_SCISSOR_TEST);
        else glDisable(GL_SCISSOR_TEST);

        Log::Print(
            "Set GL scissor test state for context '" + to_string(contextID) + "' to '" 
            + string(BoolValue(state)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLScissorRectState(
        u32 contextID,
        u32 x,
        u32 y,
        u32 width,
        u32 height)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL scissor rect state because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        glScissor(x, y, width, height);

        Log::Print(
            "Set GL scissor rect state for context '" + to_string(contextID) + "' to '" 
            + to_string(x) + "', '"
            + to_string(y) + "', '"
            + to_string(width) + "' and '"
            + to_string(height) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLLineWidth(
        u32 contextID,
        f32 width)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL line width because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        if (width < 1.0f)
        {
            Log::Print(
                "Failed to set GL line width because the passed width was too small! It must be 1.0 or higher.",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        glLineWidth(width);

        Log::Print(
            "Set GL line width for context '" + to_string(contextID) + "' to '" 
            + to_string(width) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }
    void OpenGL_Flags::SetGLPointSize(
        u32 contextID,
        f32 size)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL point size because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        if (size < 1.0f)
        {
            Log::Print(
                "Failed to set GL point size because the passed size was too small! It must be 1.0 or higher.",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        glPointSize(size);

        Log::Print(
            "Set GL point size for context '" + to_string(contextID) + "' to '" 
            + to_string(size) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
    }

    void OpenGL_Flags::SetGLClearColor(
        u32 contextID,
        f32 r,
        f32 g,
        f32 b,
        f32 a)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL clear color because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        auto is_valid_range = [&contextID](f32 value, string_view target) -> bool
            {
                if (value < 0.0f)
                {
                    Log::Print(
                        "Failed to set GL clear color for context '" + to_string(contextID) + "' because the '" + string(target) + "' value was below 0.0!",
                        "KG_GL_FLAGS",
                        LogType::LOG_ERROR,
                        2);

                    return false;
                }
                if (value > 1.0f)
                {
                    Log::Print(
                        "Failed to set GL clear color for context '" + to_string(contextID) + "' because the '" + string(target) + "' value was above 1.0!",
                        "KG_GL_FLAGS",
                        LogType::LOG_ERROR,
                        2);

                    return false;
                }

                return true;
            };

        if (!is_valid_range(r, "r")
            || !is_valid_range(g, "g")
            || !is_valid_range(b, "b")
            || !is_valid_range(a, "a"))
        {
            return;
        }

        glClearColor(r, g, b, a);

        /*
        Log::Print(
            "Set GL clear color for context '" + to_string(contextID) + "' to '" 
            + to_string(r) + "', '"
            + to_string(g) + "', '"
            + to_string(b) + "' and '"
            + to_string(a) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
        */
    }
    void OpenGL_Flags::SetGLClearDepth(
        u32 contextID,
        f64 value)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL clear depth because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        if (value < 0.0)
        {
            Log::Print(
                "Failed to set GL clear depth for context '" + to_string(contextID) + "' because the value was below 0.0!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (value > 1.0)
        {
            Log::Print(
                "Failed to set GL clear depth for context '" + to_string(contextID) + "' because the value was above 1.0!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        glClearDepth(value);

        /*
        Log::Print(
            "Set GL clear depth for context '" + to_string(contextID) + "' to '" 
            + to_string(value) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
        */
    }
    void OpenGL_Flags::SetGLClearStencil(
        u32 contextID,
        u8 value)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL clear stencil because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        glClearStencil(value);

        /*
        Log::Print(
            "Set GL clear stencil for context '" + to_string(contextID) + "' to '" 
            + to_string(value) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
        */
    }
    void OpenGL_Flags::ClearBuffers(
        u32 contextID,
        bool color,
        bool depth,
        bool stencil)
    {
        if (!ContextExists(contextID))
        {
            Log::Print(
                "Failed to set GL clear buffers because the passed context ID '" + to_string(contextID) + "' was not found!",
                "KG_GL_FLAGS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        
        OpenGL_Core::MakeContextCurrent(contextID);

        if (!color
            && !depth
            && !stencil)
        {
            Log::Print(
                "GL clear buffers was called for context ID '" + to_string(contextID) + "' with color, depth and stencil turned off!",
                "KG_GL_FLAGS",
                LogType::LOG_WARNING);

            return;
        }

        GLbitfield mask{};
        if (color) mask |= GL_COLOR_BUFFER_BIT;
        if (depth) mask |= GL_DEPTH_BUFFER_BIT;
        if (stencil) mask |= GL_STENCIL_BUFFER_BIT;
        glClear(mask);

        /*
        Log::Print(
            "Set GL clear buffers for context '" + to_string(contextID) + "' to '" 
            + string(BoolValue(color)) + "', '" 
            + string(BoolValue(depth)) + "' and '"
            + string(BoolValue(stencil)) + "'.",
            "KG_GL_FLAGS",
            LogType::LOG_SUCCESS);
        */
    }
}