//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/kg_core.hpp"

#if defined(KWIN_ANY)
#include <windows.h>
#else
#include <X11/X.h>
#include <linux/limits.h>
#include <unistd.h>
#include <csignal>
#endif

#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using std::function;
using std::string;
using std::to_string;
using std::filesystem::path;
using std::source_location;
#if defined(KLIN_ANY)
using std::raise;
#endif

static path exePath{};
static f64 deltaTime{};

static function<void(string, string, const source_location&)> externalHandler{};

namespace KalaGraphics::Core
{
    static u32 globalID{};

    u32 KalaGraphicsCore::GetGlobalID() { return globalID; }
	void KalaGraphicsCore::SetGlobalID(u32 newID) { globalID = newID; }

	path KalaGraphicsCore::GetExePath()
	{
		if (!exePath.empty()) return exePath;

		if (exePath.empty())
		{
#if defined(KWIN_ANY)
			wchar_t buffer[MAX_PATH]{};
			DWORD length = GetModuleFileNameW(
				nullptr,
				buffer,
				MAX_PATH);

			if (length > 0
				&& length < MAX_PATH)
			{	
				exePath = path(buffer);
			}
			else
			{
				ForceClose(
					"KalaGraphics viewport error",
					"Failed to get path to executable!");
			}
#else
			char buffer[PATH_MAX]{};
			ssize_t length = readlink(
				"/proc/self/exe",
				buffer,
				sizeof(buffer) - 1);

			if (length > 0)
			{
				buffer[length] = '\0';
				exePath = path(buffer);
			}
			else
			{
				KalaGraphicsCore::ForceClose(
					"KalaGraphics viewport error",
					"Failed to get path to executable!");
			}
#endif
		}

		return exePath;
	}

	f64 KalaGraphicsCore::GetDeltaTime() { return deltaTime; }
	void KalaGraphicsCore::SetDeltaTime(f64 newValue) { deltaTime = newValue; }

	void KalaGraphicsCore::SetExternalHandler(function<void (string, string, const source_location&)>&& newExternalHandler)
	{ 
		externalHandler = std::move(newExternalHandler);
	}

    void KalaGraphicsCore::ForceClose(
		string_view title,
		string_view reason,
		const source_location& location)
	{
		if (externalHandler) externalHandler(string(title), string(reason), location);
		else
		{
			Log::Print(
				"\n================"
				"\nFORCE CLOSE"
				"\n================\n",
				true);

            string reasonStr = string(reason);
            reasonStr 
                += "\n\nfile: " + string(location.file_name())
                + "\nfunction: " + string(location.function_name())
                + "\nline: " + to_string(location.line())
                + "\ncolumn: " + to_string(location.column());

			Log::Print(
				reasonStr,
				title,
				LogType::LOG_ERROR,
				2,
				true,
				TimeFormat::TIME_NONE,
				DateFormat::DATE_NONE);

#if defined(KWIN_ANY)
			__debugbreak();
#else
			raise(SIGTRAP);
#endif
		}

        _Exit(1);
	}
}