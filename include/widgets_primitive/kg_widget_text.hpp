//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Graphics
{
    class Viewport;
}

namespace KalaGraphics::PrimitiveWidgets
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::string;
    using std::default_delete;

    class LIB_API Text
    {
    friend class KalaGraphics::Graphics::Viewport;
    friend struct default_delete<Text>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<Text>& GetRegistry();

        KNODISCARD
        static Text* Initialize(
            u32 fontID,
            u32 viewportID);

        KNODISCARD
        u32 GetID() const;

        KNODISCARD
        u32 GetFontID() const;
        void SetFontID(u32 newValue);

        KNODISCARD
        u32 GetShaderID() const;
        KNODISCARD
        u32 GetTextureID() const;
        KNODISCARD
        u32 GetMeshID() const;

        //Returns the full text used by this text widget, supports \n and \t
        KNODISCARD
        const string& GetText() const;
        void SetText(string&& newValue);
    
        void Destroy();
    private:
        ~Text();

        void Update();

        bool isTextDirty = true;

        string text{};

        u32 ID{};
        u32 fontID{};
        u32 shaderID{};
        u32 textureID{};
        u32 meshID{};
    };
}