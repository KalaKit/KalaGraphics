//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "core_utils.hpp"
#include "objects/models/kg_model.hpp"

namespace KalaGraphics::Object
{
    class LIB_API Model_Standard : public Model
    {
    public:
        void Update() override;

        ~Model_Standard();
    };
}