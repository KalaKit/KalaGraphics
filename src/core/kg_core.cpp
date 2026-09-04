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
using std::filesystem::path;
#if defined(KLIN_ANY)
using std::raise;
#endif

static path exePath{};

static function<void(string, string)> externalHandler{};

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

	void KalaGraphicsCore::SetExternalHandler(function<void (string, string)>&& newExternalHandler)
	{ 
		externalHandler = std::move(newExternalHandler);
	}

    void KalaGraphicsCore::ForceClose(
		string&& target,
		string&& reason)
	{
		if (externalHandler) externalHandler(std::move(target), std::move(reason));
		else
		{
			Log::Print(
				"\n================"
				"\nFORCE CLOSE"
				"\n================\n",
				true);

			Log::Print(
				std::move(reason),
				std::move(target),
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