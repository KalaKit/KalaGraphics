//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "widgets/primitive/kg_widget_cliparea.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaGraphics::Core::KalaGraphicsCore;

using std::string;
using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::PrimitiveWidgets
{
    static KalaGraphicsRegistry<ClipArea> registry{};

    KalaGraphicsRegistry<ClipArea>& ClipArea::GetRegistry() { return registry; }

    ClipArea* ClipArea::Initialize()
    {
        unique_ptr<ClipArea> newText = make_unique<ClipArea>();
        ClipArea* textPtr = newText.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        textPtr->ID = newID;

        string err = registry.AddContent(newID, std::move(newText));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics clip area error",
				"Failed to initialize clip area! Reason: " + err);
        }

        Log::Print(
			"Created new clip area '" + to_string(newID) + "'!",
			"KG_WIDGET_CLIP_AREA",
			LogType::LOG_SUCCESS);

        return textPtr;
    }

    u32 ClipArea::GetID() const { return ID; }

    void ClipArea::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics clip area error",
                "Failed to destroy clip area '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    ClipArea::~ClipArea()
    {
        Log::Print(
            "Destroying clip area '" + to_string(ID) + "'.",
            "KG_WIDGET_CLIP_AREA",
            LogType::LOG_INFO);
    }
}