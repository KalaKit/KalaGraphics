//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32

#include "KalaHeaders/log_utils.hpp"

#include "core/kg_core.hpp"
#include "graphics/opengl/kg_opengl.hpp"
#include "graphics/opengl/kg_opengl_functions_core.hpp"
#include "graphics/opengl/kg_opengl_functions_windows.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;

using KalaGraphics::Core::KalaGraphicsCore;
using namespace KalaGraphics::Graphics::OpenGLFunctions;

using std::to_string;

namespace KalaGraphics::Graphics::OpenGL
{
	void OpenGL_Core::SwapOpenGLBuffers(
		u32 glID,
		uintptr_t handle)
	{
		if (glContexts.empty()
			|| !glContexts.contains(glID))
		{
			Log::Print(
				"Cannot swap OpenGL buffers for gl context '" + to_string(glID) + "' because the gl context doesn't exist!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return;
		}
		
		if (!handle)
		{
			Log::Print(
				"Cannot swap OpenGL buffers for gl the context '" + to_string(glID) + "' because the window handle (hdc) is invalid!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		SwapBuffers(ToVar<HDC>(handle));
	}
	
	void OpenGL_Core::MakeContextCurrent(
		u32 glID,
		uintptr_t handle)
	{
		if (glContexts.empty()
			|| !glContexts.contains(glID))
		{
			Log::Print(
				"Cannot make OpenGL context current for the gl context '" + to_string(glID) + "' because it doesn't exist!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return;
		}
		
		HGLRC storedHGLRC{};
		if (!handle
			|| !glContexts[glID].hglrc)
		{
			Log::Print(
				"Cannot make OpenGL context current for the gl context '" + to_string(glID) + "' because the handle (hdc) or the stored gl context (hglrc) is invalid!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		storedHGLRC = ToVar<HGLRC>(glContexts[glID].hglrc);

		if (wglGetCurrentContext() != storedHGLRC) wglMakeCurrent(
			ToVar<HDC>(handle),
			storedHGLRC);
	}
	bool OpenGL_Core::IsContextValid(u32 glID)
	{
		if (glContexts.empty()
			|| !glContexts.contains(glID))
		{
			Log::Print(
				"Cannot check OpenGL context validity for the gl context '" + to_string(glID) + "' because it doesn't exist!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		
		HGLRC storedHGLRC{};
		if (!glContexts[glID].hglrc)
		{
			Log::Print(
				"Cannot check OpenGL context validity for the gl context '" + to_string(glID) + "' because its stored context (hglrc) is unassigned!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		storedHGLRC = ToVar<HGLRC>(glContexts[glID].hglrc);

		HGLRC current = wglGetCurrentContext();
		if (!current)
		{
			Log::Print(
				"Cannot check OpenGL context validity for the gl context '" + to_string(glID) + "' because it is unassigned!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (current != storedHGLRC)
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"Current OpenGL context does not match stored context!");

			return false;
		}

		return true;
	}
	
	void OpenGL_Core::SetOpenGLLibrary()
	{
		HMODULE handle = GetModuleHandleA("opengl32.dll");
		if (!handle)
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"Failed to get handle for 'opengl32.dll'");

			return;
		}

		openGL32Lib = FromVar(handle);
	}
	
	bool OpenGL_Core::SetVSyncState(
		u32 glID,
		VSyncState newValue)
	{
		if (!glContexts.contains(glID))
		{
			Log::Print(
				"Cannot set vsync state with gl ID '" + to_string(glID) + "' because the ID is unassigned!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);
			
			return false;
		}
		
		if (!wglSwapIntervalEXT)
		{
			Log::Print(
				"wglSwapIntervalEXT not supported! VSync setting ignored.",
				"OPENGL",
				LogType::LOG_ERROR,
				2);
				
			return false;
		}
		
		glContexts[glID].vsyncState = newValue;

		wglSwapIntervalEXT(newValue == VSyncState::VSYNC_ON
			? 1
			: 0);
			
		return true;
	}
	
	string OpenGL_Core::GetError()
	{
		GLenum error{};
		string errorVal{};

		while ((error = glGetError()) != GL_NO_ERROR)
		{
			switch (error)
			{
			case GL_INVALID_ENUM:                  errorVal = "GL_INVALID_ENUM"; break;
			case GL_INVALID_VALUE:                 errorVal = "GL_INVALID_VALUE"; break;
			case GL_INVALID_INDEX:                 errorVal = "GL_INVALID_INDEX"; break;

			case GL_INVALID_OPERATION:             errorVal = "GL_INVALID_OPERATION"; break;
			case GL_STACK_OVERFLOW:                errorVal = "GL_STACK_OVERFLOW"; break;
			case GL_STACK_UNDERFLOW:               errorVal = "GL_STACK_UNDERFLOW"; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: errorVal = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;

			case GL_OUT_OF_MEMORY:                 errorVal = "GL_OUT_OF_MEMORY"; break;

			default:                               errorVal = "Unknown error"; break;
			}
		}

		return errorVal;
	}
}

#endif //_WIN32