//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Resources
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using u32 = uint32_t;

    class LIB_API Text
    {
    public:
        static KalaGraphicsRegistry<Text>& GetRegistry();

        static Text* Initialize();

        u32 GetID() const;

        void Destroy();

        ~Text();
    private:
        u32 ID{};
    };
}