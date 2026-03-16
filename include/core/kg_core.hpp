//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"

#include <string_view>

namespace KalaGraphics::Core
{
    using std::string_view;

    class LIB_API KalaGraphicsCore
    {
    public:
        static bool Initialize(uintptr_t context_openGL);
        static bool IsInitialized();

        static uintptr_t GetGLContext();

        //Use this when you absolutely need a hard crash at this very moment.
		//Aborts and doesn't clean up data.
		static void ForceClose(
			string_view title,
			string_view reason);
    };
}