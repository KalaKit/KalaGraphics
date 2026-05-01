//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "ui/kg_widget.hpp"

namespace KalaGraphics::UI
{
    static KalaGraphicsRegistry<Widget> registry{};

    KalaGraphicsRegistry<Widget>& Widget::GetRegistry() { return registry; }

    u32 Widget::GetID() const { return ID; }
    u32 Widget::GetWindowContextID() const { return windowContextID; }

    u32 Widget::GetCanvasID() const { return canvasID; }
    void Widget::SetCanvasID(u32 newValue)
    {

    }

    string Widget::GetName() const { return name; }
    void Widget::SetName(string_view newValue)
    { 
        name = newValue;
    }

    bool Widget::IsDirty() const { return isDirty; }
    void Widget::SetDirtyState(bool newValue) { isDirty = newValue; }

    bool Widget::IsVisible() const { return isVisible; }
    void Widget::SetVisibleState(bool newValue) { isVisible = newValue; }

    bool Widget::IsInteractable() const { return isInteractable; }
    void Widget::SetInteractableState(bool newValue) { isInteractable = newValue; }

    bool Widget::Is3D() const { return is3D; }
    void Widget::Set3DState(bool newValue) { is3D = newValue; }

    DockTarget Widget::GetDockTarget() const { return dockTarget; }
    void Widget::SetDockTarget(
        DockTarget newValue,
        const Widget* target)
    {

    }
}