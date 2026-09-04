//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "widgets/primitive/kg_widget_text.hpp"
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
    static KalaGraphicsRegistry<Text> registry{};

    KalaGraphicsRegistry<Text>& Text::GetRegistry() { return registry; }

    Text* Text::Initialize(u32 fontID)
    {
        unique_ptr<Text> newText = make_unique<Text>();
        Text* textPtr = newText.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        textPtr->ID = newID;

        string err = registry.AddContent(newID, std::move(newText));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics text widget error",
				"Failed to initialize text widget! Reason: " + err);
        }

        Log::Print(
			"Created new text widget '" + to_string(newID) + "'!",
			"KG_WIDGET_TEXT",
			LogType::LOG_SUCCESS);

        return textPtr;
    }

    u32 Text::GetID() const { return ID; }

    void Text::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to destroy text widget '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Text::~Text()
    {
        Log::Print(
            "Destroying text widget '" + to_string(ID) + "'.",
            "KG_WIDGET_TEXT",
            LogType::LOG_INFO);
    }
}