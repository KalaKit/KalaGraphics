//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "resources/kg_hitbox.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Hitbox> registry{};

    KalaGraphicsRegistry<Hitbox>& Hitbox::GetRegistry() { return registry; }

    Hitbox* Hitbox::Initialize()
    {
        /*TODO: fill*/

        return nullptr;
    }

    u32 Hitbox::GetID() const { return ID; }

    void Hitbox::Destroy()
    {
        /*TODO: fill*/
    }

    Hitbox::~Hitbox()
    {
        Log::Print(
            "Destroying hitbox '" + to_string(ID) + "'.",
            "KG_HITBOX",
            LogType::LOG_INFO);

        /*TODO: fill*/
    }
}