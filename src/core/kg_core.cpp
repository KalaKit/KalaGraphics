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

using std::function;
using std::string;
#ifdef __linux__
using std::raise;
#endif

static function<void(string, string)> externalHandler{};

namespace KalaGraphics::Core
{
    static u32 globalID{};

    u32 KalaGraphicsCore::GetGlobalID() { return globalID; }
	void KalaGraphicsCore::SetGlobalID(u32 newID) { globalID = newID; }

	void KalaGraphicsCore::SetExternalHandler(const function<void (string, string)>& newExternalHandler)
	{ 
		externalHandler = newExternalHandler;
	}

    void KalaGraphicsCore::ForceClose(
		string_view target,
		string_view reason)
	{
		if (externalHandler) externalHandler(string(target), string(reason));
		else
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
		}
	}
}