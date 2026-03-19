//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>
#include <string>

#include "log_utils.hpp"

#include "_internal/software/_kg_software_framebuffer.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::toint;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::WindowContextData;

using std::unique_ptr;
using std::make_unique;
using std::to_string;

namespace KalaGraphics::Internal::Software
{
    static KalaGraphicsRegistry<Software_Framebuffer> registry{};

    KalaGraphicsRegistry<Software_Framebuffer>& Software_Framebuffer::GetRegistry() { return registry; }

    Software_Framebuffer* Software_Framebuffer::Initialize(u32 windowID)
    {
        Log::Print(
            "Initializing KalaGraphics.", 
            "SW_FRAMEBUFFER_INTERNAL",
            LogType::LOG_INFO);
    }
}