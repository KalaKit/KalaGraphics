//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "ui/kg_widget.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::MIN_POS3;
using KalaHeaders::KalaMath::MAX_POS3;
using KalaHeaders::KalaMath::MIN_SIZE3;
using KalaHeaders::KalaMath::MAX_SIZE3;
using KalaHeaders::KalaMath::PosTarget;
using KalaHeaders::KalaMath::RotTarget;
using KalaHeaders::KalaMath::SizeTarget;
using KalaHeaders::KalaMath::getpos3d;
using KalaHeaders::KalaMath::addpos3d;
using KalaHeaders::KalaMath::setpos3d;
using KalaHeaders::KalaMath::getroteuler;
using KalaHeaders::KalaMath::addrot3d;
using KalaHeaders::KalaMath::getsize3d;
using KalaHeaders::KalaMath::addsize3d;
using KalaHeaders::KalaMath::setsize3d;

using KalaGraphics::Core::MAX_NAME_LENGTH;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::UI
{
    static KalaGraphicsRegistry<Widget> registry{};

    KalaGraphicsRegistry<Widget>& Widget::GetRegistry() { return registry; }

    Widget* Widget::Initialize(
        u32 graphicsContextID,
        string_view name,
        const f32 depth,
        const vec2 pos,
        const f32 rot,
        const vec2 size,
        bool isVisible,
        bool isInteractable)
    {
        return Initialize(
            graphicsContextID,
            name,
            depth,
            pos,
            rot,
            size,
            false,
            isVisible,
            isInteractable);
    }
    Widget* Widget::Initialize(
        u32 graphicsContextID,
        string_view name,
        const vec3& pos,
        const vec3& rot,
        const vec3& size,
        bool isVisible,
        bool isInteractable)
    {
        return Initialize(
            graphicsContextID,
            name,
            0.0f,
            pos,
            rot,
            size,
            true,
            isVisible,
            isInteractable);
    }
    Widget* Widget::Initialize(
        u32 graphicsContextID,
        string_view name,
        const f32 depth,
        const vec3& pos,
        const vec3& rot,
        const vec3& size,
        bool is3D,
        bool isVisible,
        bool isInteractable)
    {
        if (name.empty())
        {
            Log::Print(
                "Failed to create new widget because its name was empty!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (name.size() > MAX_NAME_LENGTH)
        {
            Log::Print(
                "Failed to set new widget because its name was too long!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (pos < MIN_POS3
            || pos > MAX_POS3)
        {
            Log::Print(
                "Failed to create new widget '" + string(name) + "' because its position was out of range!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }
        if (size < MIN_SIZE3
            || size > MAX_SIZE3)
        {
            Log::Print(
                "Failed to create new widget '" + string(name) + "' because its size was out of range!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (GraphicsContext::GetRegistry().GetContent(graphicsContextID) == nullptr)
        {
            Log::Print(
                "Failed to create new widget '" + string(name) + "' because its window ID '" + to_string(graphicsContextID) + "' was not found!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Widget> newWidget = make_unique<Widget>();
        Widget* widgetPtr = newWidget.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);
        widgetPtr->ID = newID;

        widgetPtr->core.widget = widgetPtr;
        widgetPtr->transform.widget = widgetPtr;
        widgetPtr->event.widget = widgetPtr;

        widgetPtr->core.isVisible = isVisible;
        widgetPtr->core.isInteractable = isInteractable;

        setpos3d(
            widgetPtr->transform.transform,
            {},
            PosTarget::POS_WORLD,
            pos);
        setsize3d(
            widgetPtr->transform.transform,
            {},
            SizeTarget::SIZE_WORLD,
            size);

        if (!is3D)
        {
            setroteuler(
                widgetPtr->transform.transform,
                {},
                RotTarget::ROT_WORLD,
                rot);

            widgetPtr->transform.depth = depth;
        }
        else
        {
            setroteuler(
                widgetPtr->transform.transform,
                {},
                RotTarget::ROT_WORLD,
                rot);
        }

        registry.AddContent(newID, std::move(newWidget));

        return widgetPtr;
    }

    u32 Widget::GetID() const { return ID; }
    u32 Widget::GetGraphicsContextID() const { return graphicsContextID; }

    string Widget_Core::GetName() const { return name; }
    void Widget_Core::SetName(string_view newName)
    {
        if (newName.empty())
        {
            Log::Print(
                "Failed to set new widget name because it was empty!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (newName.size() > MAX_NAME_LENGTH)
        {
            Log::Print(
                "Failed to set new widget name because it was too long!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return;
        }

        name = newName;
    }

    bool Widget_Core::IsDirty() const { return isDirty; }
    void Widget_Core::SetDirtyState(bool newValue) { isDirty = newValue; }

    bool Widget_Core::IsVisible() const { return isVisible; }
    void Widget_Core::SetVisibleState(bool newValue) { isVisible = newValue; }

    bool Widget_Core::IsInteractable() const { return isInteractable; }
    void Widget_Core::SetInteractableState(bool newValue) { isInteractable = newValue; }

    bool Widget_Core::Is3D() const { return is3D; }
    void Widget_Core::Set3DState(bool newValue) { is3D = newValue; }

    Anchor Widget_Transform::GetAnchorTarget() const { return anchorTarget; }
    void Widget_Transform::SetAnchorTarget(
        Anchor newValue,
        const Widget* target)
    {

    }

    //
    // POS
    //

    vec3 Widget_Transform::GetPos(bool local) const
    {
        return getpos3d(
            transform,
            local
                ? PosTarget::POS_LOCAL
                : PosTarget::POS_COMBINED);
    }
    void Widget_Transform::AddPos(
        const vec3& pos,
        bool local)
    {
        if (pos < MIN_POS3
            || pos > MAX_POS3)
        {
            Log::Print(
                "Failed to add to widget '" + string(widget->GetCore().GetName()) + "' position because it was out of range!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return;
        }

        addpos3d(
            transform,
            {},
            local ? PosTarget::POS_LOCAL : PosTarget::POS_COMBINED,
            pos);
    }
    void Widget_Transform::SetPos(
        const vec3& pos,
        bool local)
    {
        if (pos < MIN_POS3
            || pos > MAX_POS3)
        {
            Log::Print(
                "Failed to set widget '" + string(widget->GetCore().GetName()) + "' position because it was out of range!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return;
        }

        setpos3d(
            transform,
            {},
            local ? PosTarget::POS_LOCAL : PosTarget::POS_COMBINED,
            pos);
    }

    //
    // ROT
    //

    vec3 Widget_Transform::GetRot(bool local) const
    {
        return getroteuler(
            transform,
            local
                ? RotTarget::ROT_LOCAL
                : RotTarget::ROT_COMBINED);
    }
    void Widget_Transform::AddRot(
        const vec3& rot,
        bool local)
    {
        addrot3d(
            transform,
            {},
            local ? RotTarget::ROT_LOCAL : RotTarget::ROT_COMBINED,
            rot);
    }
    void Widget_Transform::SetRot(
        const vec3& rot,
        bool local)
    {
        setroteuler(
            transform,
            {},
            local ? RotTarget::ROT_LOCAL : RotTarget::ROT_COMBINED,
            rot);
    }

    //
    // SIZE
    //

    vec3 Widget_Transform::GetSize(bool local) const
    {
        return getsize3d(
            transform,
            local
                ? SizeTarget::SIZE_LOCAL
                : SizeTarget::SIZE_COMBINED);
    }
    void Widget_Transform::AddSize(
        const vec3& size,
        bool local)
    {
        if (size < MIN_SIZE3
            || size > MAX_SIZE3)
        {
            Log::Print(
                "Failed to add to widget '" + string(widget->GetCore().GetName()) + "' size because it was out of range!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return;
        }

        addsize3d(
            transform,
            {},
            local ? SizeTarget::SIZE_LOCAL : SizeTarget::SIZE_COMBINED,
            size);
    }
    void Widget_Transform::SetSize(
        const vec3& size,
        bool local)
    {
        if (size < MIN_POS3
            || size > MAX_POS3)
        {
            Log::Print(
                "Failed to set widget '" + string(widget->GetCore().GetName()) + "' size because it was out of range!",
                "KG_WIDGET",
                LogType::LOG_ERROR,
                2);

            return;
        }

        setsize3d(
            transform,
            {},
            local ? SizeTarget::SIZE_LOCAL : SizeTarget::SIZE_COMBINED,
            size);
    }

    void Widget::Destroy()
    {
        registry.RemoveContent(ID);
    }

    Widget::~Widget()
    {
        Log::Print(
			"Destroying widget '" + core.name + "' with ID '" + to_string(ID) + "'.",
			"KG_WIDGET",
			LogType::LOG_INFO);
    }
}