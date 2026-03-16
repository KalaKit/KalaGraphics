//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef __linux__
#include <csignal>
#endif

#include "log_utils.hpp"

#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

#ifdef __linux__
using std::raise;
#endif

namespace KalaGraphics::Core
{
    static bool isInitialized{};

    static uintptr_t glContext{};

    bool KalaGraphicsCore::Initialize(uintptr_t context_openGL)
    {
        if (isInitialized)
        {
            Log::Print(
                "Failed to initialize KalaGraphics because it has already been initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return false;
        }

        Log::Print(
            "Initializing KalaGraphics.", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_INFO);

        if (context_openGL == 0)
        {
            Log::Print(
                "Failed to initialize KalaGraphics because the passed OpenGL context was invalid!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return false;
        }

        glContext = context_openGL;

        isInitialized = true;

        Log::Print(
            "Finished initializing KalaGraphics!", 
            "KALAGRAPHICS_CORE",
            LogType::LOG_SUCCESS);

        return true;
    }

    bool KalaGraphicsCore::IsInitialized() { return isInitialized; }

    uintptr_t KalaGraphicsCore::GetGLContext()
    {
        if (!isInitialized)
        {
            Log::Print(
                "Cannot get GL context because KalaGraphics is not initialized!", 
                "KALAGRAPHICS_CORE",
                LogType::LOG_ERROR,
                2);

            return 0;
        }

        if (glContext == 0)
        {
            ForceClose(
                "OpenGL error", 
                "Tried to get GL context when it was invalid!");

            return 0;
        }
        
        return glContext;
    }

    void KalaGraphicsCore::ForceClose(
		string_view target,
		string_view reason)
	{
		Log::Print(
			"\n================"
			"\nFORCE CLOSE"
			"\n================\n",
			true);

		Log::Print(
			reason,
			target,
			LogType::LOG_ERROR,
			2,
			true,
			TimeFormat::TIME_NONE,
			DateFormat::DATE_NONE);

#ifdef _WIN32
		__debugbreak();
#else
		raise(SIGTRAP);
#endif

		abort();
	}
}