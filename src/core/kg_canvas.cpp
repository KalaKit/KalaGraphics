//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "core/kg_canvas.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

namespace KalaGraphics::Core
{
    static KalaGraphicsRegistry<Canvas> registry{};

    KalaGraphicsRegistry<Canvas>& Canvas::GetRegistry() { return registry; }

    Canvas* Canvas::Initialize()
    {
        /*TODO: fill*/

        return nullptr;
    }

    u32 Canvas::GetID() const { return ID; }

    void Canvas::Destroy()
    {
        /*TODO: fill*/
    }

    Canvas::~Canvas()
    {
        Log::Print(
            "Destroying canvas '" + to_string(ID) + "'.",
            "KG_CANVAS",
            LogType::LOG_INFO);

        /*TODO: fill*/
    }
}