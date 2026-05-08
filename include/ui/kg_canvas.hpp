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
    using KalaHeaders::KalaMath::vec2;
    using KalaHeaders::KalaMath::Transform2D;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::string;
    using std::string_view;

    class LIB_API Canvas
    {
    public:
        static KalaGraphicsRegistry<Canvas>& GetRegistry();

        static Canvas* Initialize(
            string_view name,
            vec2 pos,
            vec2 size,
            u32 graphicsContextID);

        u32 GetID() const;
        u32 GetGraphicsContextID() const;

        string GetName() const;
        void SetName(string_view newValue);

        bool IsEnabled() const;
        void SetEnabledState(bool newValue);

        vec2 GetPos() const;
        void SetPos(vec2 newValue);

        vec2 GetSize() const;
        void SetSize(vec2 newValue);

        void Destroy();

        ~Canvas();
    private:
        bool isEnabled{};

        u32 ID{};
        u32 graphicsContextID{};

        string name = "Canvas";

        Transform2D transform{};
    };
}