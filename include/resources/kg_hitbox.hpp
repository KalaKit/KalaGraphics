//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"

#include "core/kg_registry.hpp"
#include "resources/kg_mesh.hpp"

namespace KalaGraphics::Resources
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    class LIB_API Hitbox
    {
    public:
        static KalaGraphicsRegistry<Hitbox>& GetRegistry();

        static Hitbox* Initialize();

        u32 GetID() const;

        void Destroy();

        ~Hitbox();
    private:
        u32 ID{};

        vector<Vertex> vertices{};

        Transform3D transform{};
    };
}