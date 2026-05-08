//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "ui/kg_canvas.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::MIN_POS2;
using KalaHeaders::KalaMath::MAX_POS2;
using KalaHeaders::KalaMath::MIN_SIZE2;
using KalaHeaders::KalaMath::MAX_SIZE2;
using KalaHeaders::KalaMath::PosTarget;
using KalaHeaders::KalaMath::SizeTarget;
using KalaHeaders::KalaMath::getpos;
using KalaHeaders::KalaMath::setpos;
using KalaHeaders::KalaMath::getsize;
using KalaHeaders::KalaMath::setsize;

using KalaGraphics::Core::MAX_NAME_LENGTH;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::UI
{
    static KalaGraphicsRegistry<Canvas> registry{};

    KalaGraphicsRegistry<Canvas>& Canvas::GetRegistry() { return registry; }

    Canvas* Canvas::Initialize(
        string_view name,
        vec2 pos,
        vec2 size,
        u32 graphicsContextID)
    {
        if (name.empty())
        {
            Log::Print(
                "Failed to create new canvas because its name was empty!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (name.size() > MAX_NAME_LENGTH)
        {
            Log::Print(
                "Failed to set new canvas because its name was too long!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (pos < MIN_POS2
            || pos > MAX_POS2)
        {
            Log::Print(
                "Failed to create new canvas '" + string(name) + "' because its position was out of range!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (size < MIN_SIZE2
            || size > MAX_SIZE2)
        {
            Log::Print(
                "Failed to create new canvas '" + string(name) + "' because its size was out of range!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (GraphicsContext::GetRegistry().GetContent(graphicsContextID) == nullptr)
        {
            Log::Print(
                "Failed to create new canvas '" + string(name) + "' because its window ID '" + to_string(graphicsContextID) + "' was not found!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Canvas> newCanvas = make_unique<Canvas>();
        Canvas* canvasPtr = newCanvas.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);
        canvasPtr->ID = newID;

        canvasPtr->isEnabled = true;
        canvasPtr->name = name;

        setpos(
            canvasPtr->transform, 
            {},
            PosTarget::POS_WORLD,
            pos);
        setsize(
            canvasPtr->transform,
            {},
            SizeTarget::SIZE_WORLD,
            size);

        registry.AddContent(newID, std::move(newCanvas));

        return canvasPtr;
    }

    u32 Canvas::GetID() const { return ID; }
    u32 Canvas::GetGraphicsContextID() const { return graphicsContextID; }

    string Canvas::GetName() const { return name; }
    void Canvas::SetName(string_view newName)
    {
        if (newName.empty())
        {
            Log::Print(
                "Failed to set new canvas name because it was empty!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (newName.size() > MAX_NAME_LENGTH)
        {
            Log::Print(
                "Failed to set new canvas name because it was too long!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        name = newName;
    }

    bool Canvas::IsEnabled() const { return isEnabled; }
    void Canvas::SetEnabledState(bool newValue) { isEnabled = newValue; }

    vec2 Canvas::GetPos() const 
    { 
        return getpos(
            transform, 
            PosTarget::POS_WORLD);
    }
    void Canvas::SetPos(vec2 pos)
    {
        if (pos < MIN_POS2
            || pos > MAX_POS2)
        {
            Log::Print(
                "Failed to set canvas '" + string(name) + "' position because it was out of range!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        setpos(
            transform, 
            {}, 
            PosTarget::POS_WORLD, 
            pos);
    }

    vec2 Canvas::GetSize() const
    {
        return getsize(
            transform,
            SizeTarget::SIZE_WORLD);
    }
    void Canvas::SetSize(vec2 size)
    {
        if (size < MIN_SIZE2
            || size > MAX_SIZE2)
        {
            Log::Print(
                "Failed to set canvas '" + string(name) + "' size because it was out of range!",
                "KG_CANVAS",
                LogType::LOG_ERROR,
                2);

            return;
        }

        setsize(
            transform, 
            {}, 
            SizeTarget::SIZE_WORLD, 
            size);
    }

    void Canvas::Destroy()
    {
        registry.RemoveContent(ID);
    }

    Canvas::~Canvas()
    {
        Log::Print(
			"Destroying canvas '" + name + "' with ID '" + to_string(ID) + "'.",
			"KG_CANVAS",
			LogType::LOG_INFO);

        //TODO: add buttons etc here
    }
}