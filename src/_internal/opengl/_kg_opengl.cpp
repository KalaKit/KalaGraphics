//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#include <vector>
#include <string>

#include "core_utils.hpp"
#include "log_utils.hpp"

#include "_internal/opengl/_kg_opengl.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using std::vector;
using std::to_string;

using KalaHeaders::KalaCore::ToVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::WindowContextData;

namespace KalaGraphics::Internal::OpenGL
{
	static OpenGL_Core_Functions coreFunc{};
#ifdef _WIN32
	static OpenGL_Windows_Functions winFunc{};
#else
	static OpenGL_Linux_Functions linFunc{};
#endif

	static vector<OpenGL_Context> contexts{};

	static bool ContainsContext(u32 windowID)
	{
		for (const auto& c : contexts)
		{
			if (c.windowID == windowID) return true;
		}
		return false;
	}

	static u32 GetContext(u32 windowID)
	{
		for (const auto& c : WindowContext::GetRegistry().runtimeContent)
		{
			if (c->GetWindowContextData().windowID == windowID)
			{
				return c->GetID();
			}
		}

		return 0;
	}

	void OpenGL_Core::AddWindow(const OpenGL_Context& newCtx)
	{
		for (const auto& c : contexts)
		{
			if (c.windowID == newCtx.windowID)
			{
				Log::Print(
					"Cannot add window ID '" + to_string(newCtx.windowID) + "' because it has already been added!",
					"GL_INTERNAL",
					LogType::LOG_ERROR,
					2);

				return;
			}
		}

		contexts.push_back(newCtx);
	}

	void OpenGL_Core::SetCoreFunctions(const OpenGL_Core_Functions& newCoreFunc)
	{
		coreFunc = newCoreFunc;
	}
	const OpenGL_Core_Functions& OpenGL_Core::GetCoreFunctions() { return coreFunc; }
#ifdef _WIN32
	void OpenGL_Core::SetWindowsFunctions(const OpenGL_Windows_Functions& newWinFunc)
	{
		winFunc = newWinFunc;
	}
	const OpenGL_Windows_Functions& OpenGL_Core::GetWindowsFunctions() { return winFunc; }
#else
	void OpenGL_Core::SetLinuxFunctions(const OpenGL_Linux_Functions& newLinFunc)
	{
		linFunc = newLinFunc;
	}
	const OpenGL_Linux_Functions& OpenGL_Core::GetLinuxFunctions() { return linFunc; }
#endif

    void OpenGL_Core::MakeContextCurrent(
		u32 windowID,
		uintptr_t context,
		uintptr_t handle)
    {
		if (!context)
		{
			Log::Print(
				"Cannot set OpenGL context because the attached context doesn't exist!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return;
		}

#ifdef _WIN32
		HGLRC storedContext = ToVar<HGLRC>(context);

		if (wglGetCurrentContext() != storedContext) wglMakeCurrent(
			ToVar<HDC>(handle),
			storedContext);
#else
		u32 ctxID = GetContext(windowID);
		if (ctxID == 0)
		{
			Log::Print(
				"Cannot make context current because the passed window ID '" + to_string(windowID) + "' was not found!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);
				
			return;
		}

		const WindowContextData& ctxData = WindowContext::GetRegistry().GetContent(ctxID)->GetWindowContextData();

		if (!ctxData.context_display)
		{
			Log::Print(
				"Cannot set VSync state because context '" + to_string(ctxID) + "' does not have a valid display!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Display* display = ToVar<Display*>(ctxData.context_display);

		GLXContext stored = ToVar<GLXContext>(context);
		GLXDrawable drawable = scast<GLXDrawable>(handle);

		if (glXGetCurrentContext() != stored
			&& !glXMakeCurrent(display, drawable, stored))
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"Failed to make OpenGL context current!");
		}
#endif
    }
    bool OpenGL_Core::IsContextValid(uintptr_t context)
    {
        if (!context)
		{
			Log::Print(
				"Cannot check OpenGL context validity because the attached context doesn't exist!",
				"OPENGL",
				LogType::LOG_ERROR,
				2);

			return false;
		}

#ifdef _WIN32
		HGLRC storedContext = ToVar<HGLRC>(context);

		HGLRC current = wglGetCurrentContext();
		if (!current)
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"Failed to get current context with 'wglGetCurrentContext'!");

			return false;
		}

		if (current != storedContext)
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"Current OpenGL context does not match stored context!");

			return false;
		}
#else
		GLXContext stored = ToVar<GLXContext>(context);
		GLXContext current = glXGetCurrentContext();
#endif

		if (!current)
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"No context is currently bound!");

			return false;
		}

		if (current != stored)
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"Current OpenGL context does not match stored context!");

			return false;
		}

		return true;
    }

	void OpenGL_Core::Update(u32 windowID)
    {

    }

    void OpenGL_Core::ResizeUpdate(u32 windowID)
    {

    }

    void OpenGL_Core::SetVSyncState(
		u32 windowID,
		VSyncState newValue)
	{		
		for (auto& c : contexts)
		{
			if (c.windowID == windowID)
			{
				u32 ctxID = GetContext(windowID);
				if (ctxID == 0)
				{
					Log::Print(
						"Cannot set VSync state because the passed window ID '" + to_string(windowID) + "' was not found!",
						"GL_INTERNAL",
						LogType::LOG_ERROR,
						2);
						
					return;
				}

#ifdef _WIN32
				if (!winFunc.wglSwapIntervalEXT)
				{
					Log::Print(
						"Cannot set VSync state because wglSwapIntervalEXT has not been assigned!",
						"GL_INTERNAL",
						LogType::LOG_ERROR,
						2);
						
					return;
				}
				
				winFunc.wglSwapIntervalEXT(newValue == VSyncState::VSYNC_ON
					? 1
					: 0);
#else
				if (!linFunc.glXSwapIntervalEXT)
				{
					Log::Print(
						"Cannot set VSync state because glXSwapIntervalEXT has not been assigned!",
						"GL_INTERNAL",
						LogType::LOG_ERROR,
						2);
						
					return;
				}

				const WindowContextData& ctxData = WindowContext::GetRegistry().GetContent(ctxID)->GetWindowContextData();

				if (!ctxData.context_display)
				{
					Log::Print(
						"Cannot set VSync state because context '" + to_string(ctxID) + "' does not have a valid display!",
						"GL_INTERNAL",
						LogType::LOG_ERROR,
						2);

					return;
				}
				if (!ctxData.context_window)
				{
					Log::Print(
						"Cannot set VSync state because context '" + to_string(ctxID) + "' does not have a valid window!",
						"GL_INTERNAL",
						LogType::LOG_ERROR,
						2);

					return;
				}

				Display* display = ToVar<Display*>(ctxData.context_display);
				Window window = ToVar<Window>(ctxData.context_window);

				linFunc.glXSwapIntervalEXT(
					display, 
					window, 
					(newValue == VSyncState::VSYNC_ON ? 1 : 0));
#endif

				c.state = newValue;

				break;
			}
		}

		Log::Print(
			"Cannot set VSync state because the passed window ID '" + to_string(windowID) + "' was not found!",
			"GL_INTERNAL",
			LogType::LOG_ERROR,
			2);
	}
	VSyncState OpenGL_Core::GetVSyncState(u32 windowID) 
	{
		for (const auto& c : contexts)
		{
			if (c.windowID == windowID) return c.state;
		}

		Log::Print(
			"Failed to get vsync state because the passed window ID '" + to_string(windowID) + "' has not beed added!",
			"OPENGL_INTERNAL",
			LogType::LOG_ERROR,
			2);

		return VSyncState::VSYNC_OFF;
	}

	void OpenGL_Core::SwapOpenGLBuffers(u32 windowID)
	{
		if (!ContainsContext(windowID))
		{
			Log::Print(
				"Cannot swap OpenGL buffers because the passed window ID '" + to_string(windowID) + "' was not found!",
				"OPENGL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		WindowContext* c = WindowContext::GetRegistry().GetContent(windowID);
		const WindowContextData& context = c->GetWindowContextData();

		if (context.context_gl == 0)
		{
			Log::Print(
				"Failed to get vsync state because passed window ID '" + to_string(windowID) + "' has no attached GL context!",
				"OPENGL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

#ifdef _WIN32
		SwapBuffers(ToVar<HDC>(*context.context_gl));
#else
		Display* display = ToVar<Display*>(context.context_display);
		Window window = ToVar<Window>(context.context_window);

		glXSwapBuffers(display, window);
#endif
	}

	void OpenGL_Core::Shutdown(u32 windowID)
    {
        
    }
}