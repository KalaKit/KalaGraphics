//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "resources/kg_animation.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Animation> registry{};

    KalaGraphicsRegistry<Animation>& Animation::GetRegistry() { return registry; }

    Animation* Animation::Initialize()
    {
        /*TODO: fill*/

        return nullptr;
    }

    u32 Animation::GetID() const { return ID; }

    void Animation::Destroy()
    {
        /*TODO: fill*/
    }

    Animation::~Animation()
    {
        Log::Print(
            "Destroying animation '" + to_string(ID) + "'.",
            "KG_ANIMATION",
            LogType::LOG_INFO);

        /*TODO: fill*/
    }
}