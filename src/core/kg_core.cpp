//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <csignal>

#include "log_utils.hpp"

#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using std::function;
using std::string;
#if defined(KLIN_ANY)
using std::raise;
#endif

static function<void(string, string)> externalHandler{};

namespace KalaGraphics::Core
{
    static u32 globalID{};

    u32 KalaGraphicsCore::GetGlobalID() { return globalID; }
	void KalaGraphicsCore::SetGlobalID(u32 newID) { globalID = newID; }

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
#elif defined(KLIN_ANY)
			raise(SIGTRAP);
#endif
		}

		abort();
	}
}