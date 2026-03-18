//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"

namespace KalaGraphics::Internal::Software
{
    class LIB_API Software_Core
    {
    public:
        //Main update draw call for Software renderer
        static void Update();

        static void ResizeUpdate();

        //Clean all Software renderer resources
        static void Shutdown();
    };
}