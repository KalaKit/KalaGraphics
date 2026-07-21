//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Core
{
    class LIB_API Canvas
    {
    public:
        static KalaGraphicsRegistry<Canvas>& GetRegistry();

        static Canvas* Initialize();

        u32 GetID() const;

        void Destroy();

        ~Canvas();
    private:
        u32 ID{};
    };
}