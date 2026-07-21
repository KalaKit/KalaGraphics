//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <functional>

#include "core_utils.hpp"

namespace KalaGraphics::Core
{
    using std::string;
    using std::string_view;
    using std::function;

    using u8 = uint8_t;
    using u32 = uint32_t;

    constexpr u8 MAX_NAME_LENGTH = 64;

    class LIB_API KalaGraphicsCore
    {
    public:
        static u32 GetGlobalID();
		static void SetGlobalID(u32 newID);

        //External handler for force close, overrides local version so external version can do its own action
        static void SetExternalHandler(function<void(string, string)>&& externalHandler);

        //Force-closes the application and gives a breakpoint, good for hard stops or bad user errors,
        //assigning a callback via SetExternalHandler will always use whatever is assigned there,
        //if the callback is unassigned or invalid then it falls back to the local setup
		static void ForceClose(
			string&& title,
			string&& reason);
    };
}