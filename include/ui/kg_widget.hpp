//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::UI
{
    using KalaHeaders::KalaMath::Transform3D;
    using KalaHeaders::KalaMath::vec2;
    using KalaHeaders::KalaMath::vec3;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::string;
    using std::string_view;

    enum class DockTarget : u8
    {
        T_INVALID = 0u,

        T_FREE = 1u,

        T_TOP_LEFT = 2u,
        T_TOP_RIGHT = 3u,
        T_BOTTOM_LEFT = 4u,
        T_BOTTOM_RIGHT = 5u,

        T_LEFT = 6u,
        T_RIGHT = 7u,
        T_UP = 8u,
        T_DOWN = 9u,
        T_CENTER = 10u
    };

    class LIB_API Widget
    {
    public:
        static KalaGraphicsRegistry<Widget>& GetRegistry();

        u32 GetID() const;
        u32 GetWindowContextID() const;

        u32 GetCanvasID() const;
        void SetCanvasID(u32 newValue);

        string GetName() const;
        void SetName(string_view newValue);

        bool IsDirty() const;
        void SetDirtyState(bool newValue);

        //Will not be rendered if false
        bool IsVisible() const;
        void SetVisibleState(bool newValue);

        //Has no interaction support at all if false
        bool IsInteractable() const;
        void SetInteractableState(bool newValue);

        //Works in 3D space if true, otherwise bound to 2D canvas
        bool Is3D() const;
        void Set3DState(bool newValue);

        //Dock the widget to a specific target position,
        //if target widget is not assigned then this widget is docked relative to active canvas
        DockTarget GetDockTarget() const;
        void SetDockTarget(
            DockTarget newValue,
            const Widget* target = nullptr);

        const vec3& GetPos3D() const;
        void AddPos3D(
            const vec3& newValue,
            bool local = false);
        void SetPos3D(
            const vec3& newValue,
            bool local = false);
        vec2 GetPos() const;
        void AddPos(
            vec2 newValue,
            bool isLocal = false);
        void SetPos(
            vec2 newValue,
            bool isLocal = false);

        //Returns paint order, 0 is default, unused if in 3D mode
        f32 GetDepth() const;
        void SetDepth(f32 newValue);

        const vec3& GetRot3D() const;
        void AddRot3D(
            const vec3& newValue,
            bool local = false);
        void SetRot3D(
            const vec3& newValue,
            bool local = false);
        f32 GetRot() const;
        void AddRot(
            f32 newValue,
            bool isLocal = false);
        void SetRot(
            f32 newValue,
            bool isLocal = false);

        const vec3& GetSize3D() const;
        void AddSize3D(
            const vec3& newValue,
            bool local = false);
        void SetSize3D(
            const vec3& newValue,
            bool local = false);
        vec2 GetSize() const;
        void AddSize(
            vec2 newValue,
            bool isLocal = false);
        void SetSize(
            vec2 newValue,
            bool isLocal = false);

        virtual void Update() = 0;
        virtual void Destroy() = 0;
    private:
        u32 ID{};
        u32 windowContextID{};
        u32 canvasID{};

        string name = "UNNAMED UI WIDGET";

        bool isDirty{};
        
        bool isVisible{};
        bool isInteractable{};
        bool is3D{};

        f32 depth{};

        DockTarget dockTarget{};
        Transform3D transform{};
    };
}