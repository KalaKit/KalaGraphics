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

    class Widget;

    enum class Anchor : u8
    {
        A_INVALID = 0u,

        A_FREE = 1u,

        A_TOP_LEFT = 2u,
        A_TOP_RIGHT = 3u,
        A_BOTTOM_LEFT = 4u,
        A_BOTTOM_RIGHT = 5u,

        A_LEFT = 6u,
        A_RIGHT = 7u,
        A_UP = 8u,
        A_DOWN = 9u,
        A_CENTER = 10u
    };

    struct Widget_Core
    {
        friend class Widget;

        Widget* widget{};

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

    private:
        string name = "Widget";

        bool isDirty{};
        
        bool isVisible{};
        bool isInteractable{};
        bool is3D{};
    };
    
    struct Widget_Transform
    {
        friend class Widget;

        Widget* widget{};

        //Returns paint order, 0 is default, unused if in 3D mode
        f32 GetDepth() const;
        void SetDepth(f32 newValue);

        //Anchor the widget to a specific target position,
        //if target widget is not assigned then this widget is anchored relative to active canvas
        Anchor GetAnchorTarget() const;
        void SetAnchorTarget(
            Anchor newValue,
            const Widget* target = nullptr);

        vec3 GetPos(bool local = false) const;
        void AddPos(
            const vec3& newValue,
            bool local = false);
        void SetPos(
            const vec3& newValue,
            bool local = false);

        vec3 GetRot(bool local = false) const;
        void AddRot(
            const vec3& newValue,
            bool local = false);
        void SetRot(
            const vec3& newValue,
            bool local = false);

        vec3 GetSize(bool local = false) const;
        void AddSize(
            const vec3& newValue,
            bool local = false);
        void SetSize(
            const vec3& newValue,
            bool local = false);
    private:
        f32 depth{};

        Anchor anchorTarget{};
        Transform3D transform{};
    };

    struct Widget_Event
    {
        friend class Widget;

        Widget* widget{};
    };

    class LIB_API Widget
    {
    public:
        static KalaGraphicsRegistry<Widget>& GetRegistry();

        //Create a new 2D widget
        static Widget* Initialize(
            u32 graphicsContextID,
            string_view name,
            const f32 depth,
            const vec2 pos = {},
            const f32 rot = {},
            const vec2 size = vec3(100),
            bool isVisible = true,
            bool isInteractable = false);

        //Create a new 3D widget
        static Widget* Initialize(
            u32 graphicsContextID,
            string_view name,
            const vec3& pos = {},
            const vec3& rot = {},
            const vec3& size = vec3(1),
            bool isVisible = true,
            bool isInteractable = false);

        u32 GetID() const;
        u32 GetGraphicsContextID() const;

        Widget_Core GetCore();
        Widget_Transform GetTransform();
        Widget_Event GetEvent();

        void Destroy();

        ~Widget();
    private:
        static Widget* Initialize(
            u32 graphicsContextID,
            string_view name,
            const f32 depth,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            bool is3D,
            bool isVisible,
            bool isInteractable);

        u32 ID{};
        u32 graphicsContextID{};

        Widget_Core core{};
        Widget_Transform transform{};
        Widget_Event event{};
    };
}