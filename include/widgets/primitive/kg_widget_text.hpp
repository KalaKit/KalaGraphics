//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::PrimitiveWidgets
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::string;
    using std::default_delete;

    class LIB_API Text
    {
    friend struct default_delete<Text>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<Text>& GetRegistry();

        KNODISCARD
        static Text* Initialize(u32 fontID);

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

        //Returns a text block where each new string represents a new line for this text
        KNODISCARD
        vector<string>& GetText();
    
        void Destroy();
    private:
        ~Text();

        vector<string> text{};

        u32 ID{};

        u32 fontID{};

        u32 shaderID{};
        u32 textureID{};
        u32 meshID{};
    };
}