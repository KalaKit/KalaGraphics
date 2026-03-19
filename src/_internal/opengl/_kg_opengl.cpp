//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include "wglext.h"
#else
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#include <unordered_map>
#include <string>

#include "core_utils.hpp"
#include "log_utils.hpp"

#include "_internal/opengl/_kg_opengl.hpp"
#include "core/kg_context.hpp"

using std::unordered_map;
using std::to_string;

using KalaHeaders::KalaCore::ToVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::WindowContextData;

namespace KalaGraphics::Internal::OpenGL
{
	static unordered_map<u32, VSyncState> vsyncStates{};

	void OpenGL_Core::AddWindow(
		u32 windowID,
		VSyncState vsyncState)
	{
		if (vsyncStates.contains(windowID))
		{
			Log::Print(
				"Cannot add window ID '" + to_string(windowID) + "' because it has already been added!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		vsyncStates[windowID] = vsyncState;
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
		/*
		if (!Window_Global::IsInitialized())
		{
			Log::Print(
				"Cannot set vsync state because the global window manager has not been initialized!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		const GL_Windows* windowsFunc = OpenGL_Functions_Windows::GetGLWindows();

		if (!windowsFunc->wglSwapIntervalEXT)
		{
			Log::Print(
				"wglSwapIntervalEXT not supported! VSync setting ignored.",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);
				
			return;
		}
		
		vsyncState = newValue;

		windowsFunc->wglSwapIntervalEXT(newValue == VSyncState::VSYNC_ON
			? 1
			: 0);
		*/
	}
	VSyncState OpenGL_Core::GetVSyncState(u32 windowID) 
	{
		if (!vsyncStates.contains(windowID))
		{
			Log::Print(
				"Failed to get vsync state because the passed window ID '" + to_string(windowID) + "' was not found!",
				"OPENGL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return VSyncState::VSYNC_OFF;
		}
		
		return vsyncStates[windowID];
	}

	void OpenGL_Core::SwapOpenGLBuffers(u32 windowID)
	{
		if (!vsyncStates.contains(windowID))
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
		HDC context = context.context_gl;

		SwapBuffers(ToVar<HDC>(context));
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