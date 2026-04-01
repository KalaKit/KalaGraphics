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

#include <string>

#include "core_utils.hpp"
#include "log_utils.hpp"

#include "_internal/opengl/_kg_opengl.hpp"
#include "_internal/opengl/_kg_opengl_flags.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using std::to_string;

using KalaHeaders::KalaCore::ToVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::WindowContextData;
using KalaGraphics::Core::VSyncState;
using KalaGraphics::Internal::OpenGL::OpenGL_Flags;

namespace KalaGraphics::Internal::OpenGL
{
	static OpenGL_Core_Functions coreFunc{};
#ifdef _WIN32
	static OpenGL_Windows_Functions winFunc{};
#else
	static OpenGL_Linux_Functions linFunc{};
#endif

	static bool ContainsContext(u32 contextID)
	{
		for (const auto& c : WindowContext::GetRegistry().runtimeContent)
		{
			if (c->GetID() == contextID) return true;
		}
		return false;
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

    void OpenGL_Core::MakeContextCurrent(u32 contextID)
    {
        if (!WindowContext::GetRegistry().GetContent(contextID))
		{
			Log::Print(
				"Cannot check OpenGL context current because the passed context ID '" + to_string(contextID) + "' was not found!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		uintptr_t context = *WindowContext::GetRegistry().createdContent[contextID]->GetWindowContextData().context_gl;

#ifdef _WIN32
		HGLRC storedContext = ToVar<HGLRC>(context);

		if (wglGetCurrentContext() != storedContext) wglMakeCurrent(
			ToVar<HDC>(handle),
			storedContext);
#else
		WindowContextData& ctx = WindowContext::GetRegistry().createdContent[contextID]->GetWindowContextData();
		uintptr_t displayPtr = ctx.context_display;

		if (!displayPtr)
		{
			Log::Print(
				"Cannot set VSync state because context '" + to_string(contextID) + "' does not have a valid display!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Display* display = ToVar<Display*>(displayPtr);
		Window window = ToVar<Window>(ctx.context_window);

		GLXContext stored = ToVar<GLXContext>(context);
		GLXDrawable drawable = scast<GLXDrawable>(window);

		if (glXGetCurrentContext() != stored
			&& !glXMakeCurrent(display, drawable, stored))
		{
			KalaGraphicsCore::ForceClose(
				"OpenGL error",
				"Failed to make OpenGL context current!");
		}
#endif
    }
    bool OpenGL_Core::IsContextValid(u32 contextID)
    {
        auto* ctx = WindowContext::GetRegistry().GetContent(contextID);
		if (!ctx)
		{
			Log::Print(
				"Failed to get OpenGL context state because the context ID was not found!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		uintptr_t context = *ctx->GetWindowContextData().context_gl;

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

	void OpenGL_Core::Update(u32 contextID)
    {
		OpenGL_Flags::SetGLClearColor(
			contextID,
			0.31f,
			0.84f,
			0.48f,
			1);
		OpenGL_Flags::ClearBuffers(contextID, true);

		//handle content here...

		SwapOpenGLBuffers(contextID);
    }

    void OpenGL_Core::ResizeUpdate(u32 contextID)
    {

    }

    bool OpenGL_Core::SetVSyncState(
		u32 contextID,
		u8 newValue)
	{		
		auto* ctx = WindowContext::GetRegistry().GetContent(contextID);
		if (!ctx)
		{
			Log::Print(
				"Failed to set OpenGL vsync state because the context ID was not found!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);
				
			return false;
		}

		if (newValue != scast<u8>(VSyncState::VSYNC_ON)
			&& newValue != scast<u8>(VSyncState::VSYNC_OFF))
		{
			Log::Print(
				"Cannot set VSync state because the passed vsync state value '" + to_string(newValue) + "' is not a valid state!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);
				
			return false;
		}
		VSyncState state = scast<VSyncState>(newValue);

#ifdef _WIN32
		if (!winFunc.wglSwapIntervalEXT)
		{
			Log::Print(
				"Cannot set VSync state because wglSwapIntervalEXT has not been assigned!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);
				
			return false;
		}
		
		winFunc.wglSwapIntervalEXT(state == VSyncState::VSYNC_ON
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
				
			return false;
		}

		const WindowContextData& ctxData = WindowContext::GetRegistry().GetContent(contextID)->GetWindowContextData();

		if (!ctxData.context_display)
		{
			Log::Print(
				"Cannot set VSync state because context '" + to_string(contextID) + "' does not have a valid display!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (!ctxData.context_window)
		{
			Log::Print(
				"Cannot set VSync state because context '" + to_string(contextID) + "' does not have a valid window!",
				"GL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		Display* display = ToVar<Display*>(ctxData.context_display);
		Window window = ToVar<Window>(ctxData.context_window);

		linFunc.glXSwapIntervalEXT(
			display, 
			window, 
			(state == VSyncState::VSYNC_ON ? 1 : 0));
#endif

		ctx->context.state = state;

		return true;
	}

	void OpenGL_Core::SwapOpenGLBuffers(u32 contextID)
	{
		if (!ContainsContext(contextID))
		{
			Log::Print(
				"Cannot swap OpenGL buffers because the passed context ID '" + to_string(contextID) + "' was not found!",
				"OPENGL_INTERNAL",
				LogType::LOG_ERROR,
				2);

			return;
		}

		WindowContext* c = WindowContext::GetRegistry().GetContent(contextID);
		const WindowContextData& context = c->GetWindowContextData();

		if (context.context_gl == 0)
		{
			Log::Print(
				"Failed to get vsync state because passed context ID '" + to_string(contextID) + "' has no attached GL context!",
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

	void OpenGL_Core::Shutdown(u32 contextID)
    {
        
    }
}