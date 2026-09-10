//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>
#include <cfloat>

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

    enum class ClipType : u8
    {
        //when a glyph/text exceeds line width,
        //allow it to continue beyond the line width and max lines
        C_OVERFLOW = 0,
        //if word width + existing line content exceeds line width
        //but words own width is less than line width then word gets pushed to next line,
        //otherwise the word is not drawn at all, word is not drawn if it exceeds max lines
        C_CLIPPED = 1
    };

    enum class FieldType : u8
    {
        //supports all characters, including emojis
        F_ANY                    = 0,
        //only supports text and numbers
        F_TEXT_ONLY              = 1,
        //only supports integers, floats and doubles
        F_NUMBER_ONLY            = 2,
        //only supports integers
        F_INTEGER_ONLY           = 3,
        //only supports floats
        F_FLOAT_ONLY             = 4,
        //only supports floats and doubles
        F_FLOAT_AND_DOUBLE_ONLY = 5,
        //displays written value as stars, supports all characters
        F_PASSWORD               = 6
    };

    static constexpr u16 MAX_CHARACTERS = 1024;
    static constexpr u16 MAX_LINES = 1024;
    static constexpr u16 MAX_LINE_WIDTH = 8192;
    static constexpr u16 MIN_LINE_WIDTH = 32;
    static constexpr u16 MAX_LINE_HEIGHT = 128;
    static constexpr f32 MAX_TEXT_SIZE = 100.0f;
    static constexpr f32 MIN_TEXT_SIZE = 0.01f;

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

        KNODISCARD
        ClipType GetClipType() const;
        void SetClipType(ClipType newValue);

        KNODISCARD
        FieldType GetFieldType() const;
        void SetFieldType(FieldType newValue);

        KNODISCARD
        f32 GetTextSize() const;
        //Set new text size multiplier
        void SetTextSize(f32 newValue);

        KNODISCARD
        u16 GetLineWidth() const;
        //Set new line width, ignored if clip type is overflow
        void SetLineWidth(u16 newValue);

        KNODISCARD
        u16 GetLineHeight() const;
        void SetLineHeight(u16 newValue);

        KNODISCARD
        u16 GetMaxLines() const;
        //Set new max line count, ignored if clip type is overflow
        void SetMaxLines(u16 newValue);

        //Get max allowed characters of this text
        KNODISCARD
        u16 GetMaxCharacters() const;
        //Set max allowed characters of this text, clamped from 1 to MAX_TEXT_LENGTH
        void SetMaxCharacters(u16 newValue);

        //Get the smallest allowed value of this numerical field,
        //only applies to integers, floats and doubles,
        //cannot be set bigger than max
        KNODISCARD
        f64 GetNumberMin() const;
        void SetNumberMin(f64 newValue);

        //Get the highest allowed value of this numerical field,
        //only applies to integers, floats and doubles
        //cannot be set lower than min
        KNODISCARD
        f64 GetNumberMax() const;
        void SetNumberMax(f64 newValue);

        //Get the full stored value as string
        KNODISCARD
        string GetText() const;
        //Directly append or prepend string to this text widget
        void AddText(
            string&& newValue,
            bool back = true);
        //Remove amount of characters from front or back
        void RemoveText(
            u32 count,
            bool back = true);
        //Overwrite existing string with new value
        void SetText(string&& newValue);

        //Get the full stored value as UTF vector
        KNODISCARD
        const vector<u32>& GetUTF() const;
        //Directly append or prepend UTF to this text widget
        void AddUTF(
            vector<u32>&& newValue,
            bool back = true);
        //Overwrite existing UTF with new value
        void SetUTF(vector<u32>&& newValue);
    
        void Destroy();
    private:
        ~Text();

        void Update();

        u32 ID{};
        u32 fontID{};
        u32 shaderID{};
        u32 textureID{};
        u32 meshID{};

        bool isTextDirty = true;

        ClipType clipType{};

        FieldType fieldType{};

        f32 textSize = 1.0f;

        u16 lineWidth = 256;
        u16 lineHeight = 32;
        u16 maxLines = 1;

        u16 maxCharacters = 32;

        f64 numberMin = -DBL_MAX;
        f64 numberMax = DBL_MAX;

        vector<u32> text{};
    };
}