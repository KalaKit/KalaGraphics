//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"

namespace KalaGraphics::Internal::Vulkan
{
    class LIB_API Vulkan_Core
    {
    public:
        //Main update draw call for Vulkan
        static void Update();

        static void ResizeUpdate();

        //Clean all Vulkan resources
        static void Shutdown();
    };
}