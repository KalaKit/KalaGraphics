//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "objects/models/kg_model_standard.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

namespace KalaGraphics::Object
{
    void Model_Standard::Update()
    {

    }

    Model_Standard::~Model_Standard()
    {
        Log::Print(
            "Destroying model '" + name + "' with ID '" + to_string(ID) + "'.",
            "KG_MODEL_STANDARD",
            LogType::LOG_INFO);
    }
}