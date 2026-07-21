//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "resources/kg_text.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Text> registry{};

    KalaGraphicsRegistry<Text>& Text::GetRegistry() { return registry; }

    Text* Text::Initialize()
    {
        /*TODO: fill*/

        return nullptr;
    }

    u32 Text::GetID() const { return ID; }

    void Text::Destroy()
    {
        /*TODO: fill*/
    }

    Text::~Text()
    {
        Log::Print(
            "Destroying text '" + to_string(ID) + "'.",
            "KG_TEXT",
            LogType::LOG_INFO);

        /*TODO: fill*/
    }
}