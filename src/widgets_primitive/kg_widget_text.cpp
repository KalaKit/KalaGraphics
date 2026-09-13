//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"
#include "key_standards.hpp"

#include "widgets_primitive/kg_widget_text.hpp"
#include "import/kg_import_font.hpp"
#include "graphics/kg_context.hpp"
#include "graphics/kg_viewport.hpp"
#include "graphics/kg_shader.hpp"
#include "graphics/kg_texture.hpp"
#include "graphics/kg_material.hpp"
#include "graphics/kg_mesh.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::PosTarget;
using KalaHeaders::KalaMath::SizeTarget;
using KalaHeaders::KalaMath::Transform2D;
using KalaHeaders::KalaMath::vec3;
using KalaHeaders::KalaMath::vec2;

using KalaHeaders::KalaKeyStandards::KeyboardButton;
using KalaHeaders::KalaKeyStandards::MouseButton;
using KalaHeaders::KalaKeyStandards::GetUTFByValue;
using KalaHeaders::KalaKeyStandards::GetValueByUTF;

using KalaGraphics::Core::KalaGraphicsCore;

using KalaGraphics::Import::GlyphData;
using KalaGraphics::Import::ImportFont;

using KalaGraphics::Graphics::RootShaderTarget;
using KalaGraphics::Graphics::GraphicsContext;
using KalaGraphics::Graphics::Viewport;
using KalaGraphics::Graphics::Shader;
using KalaGraphics::Graphics::TexturePixelFormat;
using KalaGraphics::Graphics::Texture;
using KalaGraphics::Graphics::MaterialType2D;
using KalaGraphics::Graphics::Material;
using KalaGraphics::Graphics::Mesh;

using std::string;
using std::string_view;
using std::to_string;
using std::unique_ptr;
using std::make_unique;
using std::min;
using std::max;
using std::clamp;
using std::vector;

static bool isVerboseLoggingEnabled{};

//which text widget did we start dragging from
static u32 dragStartTextWidget{};

static vector<u32> StringToUTF(string_view input)
{
    vector<u32> convertedText{};

    for (size_t i = 0; i < input.size();)
    {
        const u8 firstByte = scast<u8>(input[i]);

        size_t byteCount{};

        if ((firstByte & 0x80) == 0)
        {
            byteCount = 1;
        }
        else if ((firstByte & 0xE0) == 0xC0)
        {
            byteCount = 2;
        }
        else if ((firstByte & 0xF0) == 0xE0)
        {
            byteCount = 3;
        }
        else if ((firstByte & 0xF8) == 0xF0)
        {
            byteCount = 4;
        }
        else
        {
            //invalid UTF-8 byte
            convertedText.push_back(0x003F);
            i++;
            continue;
        }

        //incomplete UTF-8 sequence
        if (i + byteCount > input.size())
        {
            convertedText.push_back(0x003F);
            break;
        }

        string_view value
        {
            input.data() + i,
            byteCount
        };

        convertedText.push_back(
            GetUTFByValue(value));

        i += byteCount;
    }

    return convertedText;
}

namespace KalaGraphics::PrimitiveWidgets
{
    static KalaGraphicsRegistry<Text> registry{};

    KalaGraphicsRegistry<Text>& Text::GetRegistry() { return registry; }

    bool Text::IsVerboseLoggingEnabled() { return isVerboseLoggingEnabled; }
    void Text::SetVerboseLoggingState(bool state) { isVerboseLoggingEnabled = state; }

    Text* Text::Initialize(
        u32 fontID,
        u32 viewportID)
    {
        ImportFont* font{};
        string err = ImportFont::GetRegistry().GetContent(fontID, font);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize text widget because its font '" 
                + to_string(fontID) + "' was invalid! Reason: " + err,
                "KG_TEXT",
                LogType::LOG_WARNING);

            return {};
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(viewportID, vp);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize text widget because its viewport '" 
                + to_string(viewportID) + "' was invalid! Reason: " + err,
                "KG_TEXT",
                LogType::LOG_WARNING);

            return {};
        }

        Shader* fontShader{};
        err = Shader::GetRegistry().GetContent(vp->GetRootShaderID(RootShaderTarget::T_FONT), fontShader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to initialize text widget because its viewport '" 
                + to_string(viewportID) + "' root font shader was invalid! Reason: " + err);
        }

        Texture* fontTexture = Texture::Initialize(
            fontShader->GetID(),
            {
                .format = TexturePixelFormat::FORMAT_BASIC_R8
            });

        if (!fontTexture)
        {
            Log::Print(
                "Failed to initialize text widget because its texture failed to initialize!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        Mesh* fontMesh = Mesh::Initialize(fontShader->GetID());

        if (!fontMesh)
        {
            Log::Print(
                "Failed to initialize text widget because its mesh failed to initialize!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        Material* fontMeshMat{};
        err = Material::GetRegistry().GetContent(fontMesh->GetMaterialID(), fontMeshMat);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to initialize text widget because its mesh '" 
                + to_string(fontMesh->GetID()) + "' material was invalid! Reason: " + err);
        }

        fontMeshMat->SetMaterial2DType(MaterialType2D::M_FONT);
        fontMeshMat->SetBaseColorTextureID(fontTexture->GetID());
        fontMeshMat->SetBaseColor({ vec3{ 0.0f }, 1.0f });

        unique_ptr<Text> newText = make_unique<Text>();
        Text* textPtr = newText.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        textPtr->ID = newID;
        textPtr->fontID = fontID;
        textPtr->shaderID = fontShader->GetID();
        textPtr->textureID = fontTexture->GetID();
        textPtr->meshID = fontMesh->GetID();

        fontTexture->textWidgetID = newID;
        fontMesh->textWidgetID = newID;

        err = registry.AddContent(newID, std::move(newText));
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

    u32 Text::GetFontID() const { return fontID; }
    void Text::SetFontID(u32 newValue)
    {
        if (newValue == 0)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' font ID because it was empty!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (fontID == newValue)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' font ID to '" + to_string(newValue) + "' because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        ImportFont* oldFont{};
        string err = ImportFont::GetRegistry().GetContent(fontID, oldFont);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to set text widget '" + to_string(ID) + "' font ID because its old font '" 
                + to_string(fontID) + "' was invalid! Reason: " + err);
        }

        ImportFont* font{};
        err = ImportFont::GetRegistry().GetContent(newValue, font);
        if (!err.empty())
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' font ID because the new font ID '" + to_string(newValue) + "' was invalid!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        fontID = newValue;

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' font ID to '" + to_string(fontID) + "'!",
            "KG_FONT",
            LogType::LOG_SUCCESS);
    }

    u32 Text::GetShaderID() const { return shaderID; }
    u32 Text::GetTextureID() const { return textureID; }
    u32 Text::GetMeshID() const { return meshID; }

    bool Text::CanEdit() const { return canEdit; }
    void Text::SetEditState(bool newValue)
    {
        canEdit = newValue;

        if (!canEdit)
        {
            cursorData = {};
            highlightData = {};
        }

        Log::Print(
            "Set text widget '" + to_string(ID) + "' edit state to '" + (canEdit ? "true" : "false") + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    TextClipType Text::GetClipType() const { return clipType; }
    void Text::SetClipType(TextClipType newValue)
    {
        if (newValue == clipType)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' clip type because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        clipType = newValue;

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' clip type to '" 
            + string(clipType == TextClipType::C_OVERFLOW ? "overflow" : "clipped") + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    TextFieldType Text::GetFieldType() const { return fieldType; }
    void Text::SetFieldType(TextFieldType newValue)
    {
        if (fieldType == newValue)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' field type because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        fieldType = newValue;

        string fieldTypeStr{};

        switch (fieldType)
        {
        default:
        case TextFieldType::F_ANY:
            fieldTypeStr = "any";
            break;
        case TextFieldType::F_TEXT_ONLY:
            fieldTypeStr = "text only";
            break;
        case TextFieldType::F_NUMBER_ONLY:
            fieldTypeStr = "number only";

            SetNumberMin(-DBL_MAX);
            SetNumberMax(DBL_MAX);
            break;
        case TextFieldType::F_INTEGER_ONLY:
            fieldTypeStr = "integer only";

            //not true i64 limits but its not a big deal

            SetNumberMin(-FLT_MAX);
            SetNumberMax(FLT_MAX);
            break;
        case TextFieldType::F_FLOAT_ONLY:
            fieldTypeStr = "float only";

            SetNumberMin(-FLT_MAX);
            SetNumberMax(FLT_MAX);
            break;
        case TextFieldType::F_FLOAT_AND_DOUBLE_ONLY:
            fieldTypeStr = "float and double only";

            SetNumberMin(-DBL_MAX);
            SetNumberMax(DBL_MAX);
            break;
        case TextFieldType::F_PASSWORD:
            fieldTypeStr = "password";
            break;
        }

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' field type to '" + fieldTypeStr + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    TextAlignmentType Text::GetAlignmentType() const { return alignmentType; }
    void Text::SetAlignmentType(TextAlignmentType newValue)
    {
        if (alignmentType == newValue)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' alignment type because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        alignmentType = newValue;

        string alignmentTypeStr{};

        switch (alignmentType)
        {
        default:
        case TextAlignmentType::A_TOP_LEFT:
            alignmentTypeStr = "top left";
            break;
        case TextAlignmentType::A_CENTER_LEFT:
            alignmentTypeStr = "center left";
            break;
        case TextAlignmentType::A_BOTTOM_LEFT:
            alignmentTypeStr = "bottom left";
            break;

        case TextAlignmentType::A_TOP_CENTER:
            alignmentTypeStr = "top center";
            break;
        case TextAlignmentType::A_CENTER:
            alignmentTypeStr = "center";
            break;
        case TextAlignmentType::A_BOTTOM_CENTER:
            alignmentTypeStr = "bottom center";
            break;

        case TextAlignmentType::A_TOP_RIGHT:
            alignmentTypeStr = "top right";
            break;
        case TextAlignmentType::A_CENTER_RIGHT:
            alignmentTypeStr = "center right";
            break;
        case TextAlignmentType::A_BOTTOM_RIGHT:
            alignmentTypeStr = "bottom right";
            break;
        }

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' alignment type to '" + alignmentTypeStr + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    i32 Text::GetCursorPos() const { return cursorData.characterSlot; }
    void Text::SetCursorPosByUTF(
        i32 targetUTF,
        i32 targetUTFSlot)
    {
        if (targetUTF == -1
            && targetUTFSlot == -1)
        {
            cursorData = {};

            Log::Print(
                "Cleared text widget '" + to_string(ID) + "' cursor pos!",
                "KG_TEXT",
                LogType::LOG_SUCCESS);

            return;
        }

        if (targetUTF < 0)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by UTF because its target utf must be 0 or higher!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }
        if (targetUTFSlot < -1)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by UTF because its target utf slot must be -1 or higher!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        bool foundGlyph{};
        for (const GlyphRasterData& glyph : displayedText)
        {
            if (glyph.utf == scast<u32>(targetUTF))
            {
                foundGlyph = true;
                break;
            }
        }

        if (!foundGlyph)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by UTF because the text widget does not contain UTF '" + to_string(targetUTF) + "'!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        i32 targetSlot = -1;

        //pick next Nth one by utf
        if (targetUTFSlot == -1)
        {
            i32 firstValue = -1;

            bool foundOld{};

            for (size_t i = 0; i < displayedText.size(); i++)
            {
                const GlyphRasterData& glyph = displayedText[i];

                if (glyph.utf == scast<u32>(targetUTF))
                {
                    //store first found one
                    if (firstValue == -1) firstValue = scast<i32>(i);

                    //found current one, won't pick it
                    if (!foundOld
                        && scast<i32>(i) == cursorData.characterSlot)
                    {
                        foundOld = true;
                        continue;
                    }
                    //found next one in current loop, picking that
                    else
                    {
                        targetSlot = i;
                        break;
                    }
                }
            }

            //this loop ended before we could assign next
            //from old found one so pick the first found one
            if (targetSlot == -1) targetSlot = firstValue;
        }
        //pick selected character by utf slot
        else
        {
            u32 utfSlot{};
            for (size_t i = 0; i < displayedText.size(); i++)
            {
                const GlyphRasterData& glyph = displayedText[i];

                if (glyph.utf == scast<u32>(targetUTF))
                {
                    if (utfSlot == scast<u32>(targetUTFSlot))
                    {
                        targetSlot = scast<i32>(i);
                        break;
                    }

                    utfSlot++;
                }
            }
        }

        if (targetSlot == -1)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by UTF because utf '" + to_string(targetUTF) 
                + "' was not found at utf slot '" + to_string(targetUTFSlot) + "'!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        cursorData = { .characterSlot = targetSlot };

        Log::Print(
            "Set text widget '" + to_string(ID) + "' cursor pos by UTF to UTF '" 
            + to_string(targetUTF) 
            + "' at char slot '" + to_string(targetUTFSlot) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }
    void Text::SetCursorPosBySlot(i32 targetSlot)
    {
        if (targetSlot == -1)
        {
            cursorData = {};

            Log::Print(
                "Cleared text widget '" + to_string(ID) + "' cursor pos!",
                "KG_TEXT",
                LogType::LOG_SUCCESS);

            return;
        }

        if (targetSlot < -1)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by slot because its target slot must be -1 or higher!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (scast<u32>(targetSlot) > displayedText.size())
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID)
                + "' cursor pos by slot because slot '" + to_string(targetSlot) 
                + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }
        
        cursorData.characterSlot = targetSlot;

        Log::Print(
            "Set text widget '" + to_string(ID) 
            + "' cursor pos by slot to slot '" + to_string(cursorData.characterSlot) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    pair<i32, i32> Text::GetHighlightRange() const 
    { 
        return 
        { 
            highlightData.highlightStart, 
            highlightData.highlightEnd 
        };
    }
    void Text::SetHighlightRange(pair<i32, i32> newValue)
    {
        if (newValue.first == -1
            && newValue.second == -1)
        {
            highlightData = {};

            Log::Print(
                "Cleared text widget '" + to_string(ID) + "' highlighted text!",
                "KG_TEXT",
                LogType::LOG_SUCCESS);

            return;
        }

        if (newValue.first < 0
            || newValue.second < 0)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' highlighted area because first or second was below 0!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (newValue.first >= newValue.second)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' highlighted area because first cannot be equal or bigger than second!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (scast<u32>(newValue.second) > displayedText.size())
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' highlighted area because second '" + to_string(newValue.second) 
                + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        highlightData = 
        {
            .highlightStart = newValue.first,
            .highlightEnd = newValue.second
        };

        Log::Print(
            "Set text widget '" + to_string(ID) + "' highlighted area start to '" 
            + to_string(highlightData.highlightStart) + "' and end to "
            + to_string(highlightData.highlightEnd) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    f32 Text::GetTextSizeMultiplier() const { return textSizeMultiplier; }
    void Text::SetTextSizeMultiplier(f32 newValue)
    {
        if (newValue == textSizeMultiplier)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' size because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        textSizeMultiplier = clamp(newValue, MIN_TEXT_MULTIPLIER_SIZE, MAX_TEXT_MULTIPLIER_SIZE);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' text size multiplier to '" + to_string(textSizeMultiplier) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    u16 Text::GetLineWidth() const { return lineWidth; }
    void Text::SetLineWidth(u16 newValue)
    {
        if (newValue == lineWidth)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' line width because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        lineWidth = clamp(newValue, MIN_LINE_WIDTH, MAX_LINE_WIDTH);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' line width to '" + to_string(lineWidth) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    u16 Text::GetLineHeight() const { return lineHeight; }
    void Text::SetLineHeight(u16 newValue)
    {
        if (newValue == lineHeight)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' line height because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        lineHeight = clamp(newValue, scast<u16>(1), MAX_LINE_HEIGHT);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' line height to '" + to_string(lineHeight) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    u16 Text::GetMaxLines() const { return maxLines; }
    void Text::SetMaxLines(u16 newValue)
    {
        if (newValue == maxLines)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' max lines because it is already the same!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        maxLines = clamp(newValue, scast<u16>(1), MAX_LINES);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' max lines to '" + to_string(maxLines) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    u16 Text::GetMaxCharacters() const { return maxCharacters; }
    void Text::SetMaxCharacters(u16 newValue)
    {
        newValue = clamp(newValue, scast<u16>(1), MAX_CHARACTERS);

        bool needsTruncation = displayedText.size() > newValue;
        string removedStr{};

        if (needsTruncation)
        {
            u16 toBeRemoved = scast<u16>(displayedText.size() - newValue);
            RemoveText(toBeRemoved);

            removedStr = 
                " Removed '" + to_string(toBeRemoved) 
                + "' characters because new size is smaller than old total amount of characters.";
        }

        maxCharacters = newValue;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' max character count to '" + to_string(maxCharacters) + "'!" + removedStr,
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    bool Text::IsNumberField() const
    {
        return fieldType == TextFieldType::F_NUMBER_ONLY
            || fieldType == TextFieldType::F_INTEGER_ONLY
            || fieldType == TextFieldType::F_FLOAT_ONLY
            || fieldType == TextFieldType::F_FLOAT_AND_DOUBLE_ONLY;
    }

    f64 Text::GetNumberMin() const { return numberMin; }
    void Text::SetNumberMin(f64 newValue)
    {
        newValue = clamp(newValue, -DBL_MAX, numberMax);

        numberMin = newValue;

        switch (fieldType)
        {
        default: break;
        case TextFieldType::F_INTEGER_ONLY:
            numberMin = clamp(scast<i64>(numberMin), scast<i64>(INT64_MIN), scast<i64>(numberMax));
            break;
        case TextFieldType::F_FLOAT_ONLY:
            numberMin = clamp(scast<f32>(numberMin), -FLT_MAX, scast<f32>(numberMax));
            break;
        }

        Log::Print(
            "Set new text widget '" + to_string(ID) + "' min value to '" + to_string(numberMin) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    f64 Text::GetNumberMax() const { return numberMax; }
    void Text::SetNumberMax(f64 newValue)
    {
        newValue = clamp(newValue, numberMin, DBL_MAX);

        numberMax = newValue;

        switch (fieldType)
        {
        default: break;
        case TextFieldType::F_INTEGER_ONLY:
            numberMax = clamp(scast<i64>(numberMax), scast<i64>(numberMin), scast<i64>(INT64_MAX));
            break;
        case TextFieldType::F_FLOAT_ONLY:
            numberMax = clamp(scast<f32>(numberMax), scast<f32>(numberMin), FLT_MAX);
            break;
        }

        Log::Print(
            "Set new text widget '" + to_string(ID) + "' max value to '" + to_string(numberMax) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    string Text::GetText(bool getDisplayed) const
    {
        string result{};

        if (getDisplayed)
        {
            for (const GlyphRasterData& glyph : displayedText)
            {
                result += GetValueByUTF(glyph.utf);
            }
        }
        else
        {
            for (u32 utf : realText)
            {
                result += GetValueByUTF(utf);
            }
        }

        return result;
    }
    void Text::AddText(
        string_view newValue,
        u32 startChar,
        bool back,
        bool addDisplayed)
    {
        AddUTF(
            StringToUTF(newValue),
            startChar,
            back,
            addDisplayed);
    }
    void Text::RemoveText(
        u32 count,
        u32 startChar,
        bool back,
        bool removeDisplayed)
    {
        string textTypeStr = removeDisplayed ? "displayed" : "real";

        if (count == 0)
        {
            Log::Print(
                "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID) 
                + "' because removal count was 0!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (removeDisplayed)
        {
            if (fieldType == TextFieldType::F_PASSWORD)
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID) 
                    + "' because password field type doesn't allow editing displayed text!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > displayedText.size())
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID)
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (count > displayedText.size() - startChar)
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID) 
                    + "' because removal count from start character '" + to_string(count) 
                    + "' exceeds total character count '" + to_string(displayedText.size() - startChar) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (back)
            {
                displayedText.erase(
                    displayedText.end() - startChar - count, 
                    displayedText.end() - startChar);
            }
            else
            {
                displayedText.erase(
                    displayedText.begin() + startChar, 
                    displayedText.begin() + startChar + count);
            }

            isTextDirty = true;
        }
        else
        {
            if (IsNumberField())
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID) 
                    + "' because number field type doesn't allow editing real text!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > realText.size())
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID)
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(realText.size()) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (count > realText.size() - startChar)
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID) 
                    + "' because removal count from start character '" + to_string(count) 
                    + "' exceeds total character count '" + to_string(realText.size() - startChar) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (back)
            {
                realText.erase(
                    realText.end() - startChar - count, 
                    realText.end() - startChar);
            }
            else
            {
                realText.erase(
                    realText.begin() + startChar, 
                    realText.begin() + startChar + count);
            }

            if (fieldType == TextFieldType::F_PASSWORD)
            {
                isTextDirty = true;
            }
        }

        if (isVerboseLoggingEnabled)
        {
            string target = back ? "back" : "front";

            Log::Print(
                "Removed '" + to_string(count) + "' " + textTypeStr + " characters from text widget '" + to_string(ID) + "' text " + target + "!",
                "KG_TEXT",
                LogType::LOG_VERBOSE);
        }
    }
    void Text::SetText(
        string_view newValue,
        bool setDisplayed)
    {   
        SetUTF(
            StringToUTF(newValue),
            setDisplayed);
    }

    vector<u32> Text::GetUTF(bool getDisplayed) const
    {
        if (getDisplayed)
        {
            vector<u32> result{};

            result.reserve(displayedText.size());
            for (const GlyphRasterData& glyph : displayedText)
            {
                result.push_back(glyph.utf);
            }

            return result;
        }
        else return realText;
    }
    void Text::AddUTF(
        vector<u32>&& newValue,
        u32 startChar,
        bool back,
        bool addDisplayed)
    {
        string textTypeStr = addDisplayed ? "displayed" : "real";

        if (addDisplayed)
        {
            if (fieldType == TextFieldType::F_PASSWORD)
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because password field type doesn't allow editing displayed text!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (newValue.size() + displayedText.size() > maxCharacters)
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because added character count '" + to_string(newValue.size() + displayedText.size()) 
                    + "' exceeds max character count '" + to_string(maxCharacters) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > displayedText.size())
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            vector<GlyphRasterData> newData{};
            newData.reserve(newValue.size());
            for (const u32 utf : newValue)
            {
                newData.push_back({ .utf = utf });
            }

            if (back)
            {
                displayedText.insert(
                    displayedText.end() - startChar,
                    newData.begin(),
                    newData.end());
            }
            else
            {
                displayedText.insert(
                    displayedText.begin() + startChar,
                    newData.begin(),
                    newData.end());
            }

            isTextDirty = true;
        }
        else
        {
            if (IsNumberField())
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because number field type doesn't allow editing real text!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (newValue.size() + realText.size() > maxCharacters)
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because added character count '" + to_string(newValue.size() + realText.size()) 
                    + "' exceeds max character count '" + to_string(maxCharacters) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > realText.size())
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(realText.size()) + "'!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }
            if (back)
            {
                realText.insert(
                    realText.end() - startChar,
                    newValue.begin(),
                    newValue.end());
            }
            else
            {
                realText.insert(
                    realText.begin() + startChar,
                    newValue.begin(),
                    newValue.end());
            }

            if (fieldType == TextFieldType::F_PASSWORD)
            {
                isTextDirty = true;
            }
        }

        if (isVerboseLoggingEnabled)
        {
            string target = back ? "back" : "front";

            Log::Print(
                "Added new " + textTypeStr + " characters to text widget '" + to_string(ID) + "' " + target + "!",
                "KG_TEXT",
                LogType::LOG_VERBOSE);
        }
    }
    void Text::SetUTF(
        vector<u32>&& newValue,
        bool setDisplayed)
    {
        string textTypeStr = setDisplayed ? "displayed" : "real";

        if (newValue.size() > maxCharacters)
        {
            Log::Print(
                "Failed to update text widget '" + to_string(ID) 
                + "' " + textTypeStr + " characters because its character count '" + to_string(newValue.size()) 
                + "' exceeds max character count '" + to_string(maxCharacters) + "'!",
                "KG_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (setDisplayed)
        {
            if (fieldType == TextFieldType::F_PASSWORD)
            {
                Log::Print(
                    "Failed to update text widget '" + to_string(ID) 
                    + "' " + textTypeStr + " characters because password field type doesn't allow editing displayed text!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            vector<u32> existingUTFs{};
            existingUTFs.reserve(displayedText.size());
            for (const GlyphRasterData& data : displayedText)
            {
                existingUTFs.push_back(data.utf);
            }

            if (newValue == existingUTFs)
            {
                Log::Print(
                    "Failed to update text widget '" + to_string(ID) 
                    + "' " + textTypeStr + " characters because they are already the same!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            vector<GlyphRasterData> newData{};
            newData.reserve(newValue.size());
            for (const u32 utf : newValue)
            {
                newData.push_back({ .utf = utf });
            }

            displayedText = std::move(newData);

            isTextDirty = true;
        }
        else
        {
            if (IsNumberField())
            {
                Log::Print(
                    "Failed to update text widget '" + to_string(ID) 
                    + "' " + textTypeStr + " characters because number field type doesn't allow editing real text!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (newValue == realText)
            {
                Log::Print(
                    "Failed to update text widget '" + to_string(ID) 
                    + "' " + textTypeStr + " characters because they are already the same!",
                    "KG_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            realText = std::move(newValue);

            if (fieldType == TextFieldType::F_PASSWORD)
            {
                isTextDirty = true;
            }
        }

        if (isVerboseLoggingEnabled)
        {
            Log::Print(
                "Overwrote text widget '" + to_string(ID) + "' " + textTypeStr + " characters!",
                "KG_TEXT",
                LogType::LOG_VERBOSE);
        }
    }

    void Text::Update()
    {
        bool hasInputUpdate{};

        //TODO: use cursor pos to verify if text field is active
        //if (!canEdit
        //    || cursorData.characterSlot == -1)
        //{
        //    return;
        //}

        ImportFont* font{};
        string err = ImportFont::GetRegistry().GetContent(fontID, font);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID) + "' because its font '" 
                + to_string(fontID) + "' was invalid! Reason: " + err);
        }

        bool pressedEnter = ContainsValue(GraphicsContext::GetPressedKeys(), KeyboardButton::K_RETURN);

        u32 pressedChar = GraphicsContext::GetModifierChar(); 

        if (pressedChar != 0
            || GraphicsContext::GetBackspaceState()
            || GraphicsContext::GetTabState()
            || pressedEnter)
        {
            if (pressedChar != 0)
            {
                bool containsGlyph{};
                bool emptyGlyph{};
                for (const GlyphData& gd : font->GetFontData().glyphs)
                {
                    if (gd.codepoint == pressedChar)
                    {
                        containsGlyph = true;

                        if (gd.size == 0
                            && gd.codepoint != 0x0020  //space
                            && gd.codepoint != 0x00A0) //non-breaking space
                        {
                            emptyGlyph = true;
                        }

                        break;
                    }
                }

                if (containsGlyph)
                {
                    if (!emptyGlyph)
                    {
                        //Log::Print("@@@@@ found utf: " + to_string(pressedChar));

                        AddUTF({ pressedChar });
                    }
                    else
                    {
                        Log::Print("@@@@@ found empty utf: " + to_string(pressedChar));

                        AddUTF({ 0x003F }); //fallback ?
                    }
                }
                else
                {
                    Log::Print("@@@@@ did not find utf: " + to_string(pressedChar));

                    AddUTF({ 0x003F }); //fallback ?
                }

                hasInputUpdate = true;
            }

            if (GraphicsContext::GetBackspaceState()
                && displayedText.size() > 0)
            {
                RemoveText(1);
                hasInputUpdate = true;
            }
            if (GraphicsContext::GetTabState()
                && displayedText.size() + 1 < maxCharacters)
            {
                //four spaces for tab
                AddUTF(
                {
                    0x0020,
                    0x0020,
                    0x0020,
                    0x0020
                });

                hasInputUpdate = true;
            }

            //single-line fields can never add a return value
            if (pressedEnter
                && maxLines > 1)
            {
                AddUTF({ 0x0A });
                hasInputUpdate = true;
            }
        }

        Texture* tex{};
        err = Texture::GetRegistry().GetContent(textureID, tex);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID) + "' because its texture '" 
                + to_string(textureID) + "' was invalid! Reason: " + err);
        }

        Mesh* mesh{};
        err = Mesh::GetRegistry().GetContent(meshID, mesh);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID) + "' because its mesh '" 
                + to_string(meshID) + "' was invalid! Reason: " + err);
        }

        bool hasSizeUpdate{};
        vec2 meshSize = scast<Transform2D&>(mesh->GetTransform()).getsize(SizeTarget::SIZE_WORLD);
        if (tex->GetSize() != meshSize)
        {
            tex->SetSize(meshSize);
            hasSizeUpdate = true;

            Log::Print("@@@@@ text widget '" + to_string(ID) + "' size changed... updating its texture size");
        }

        if (!isTextDirty
            && !hasInputUpdate
            && !hasSizeUpdate)
        {
            return;
        }

        if (displayedText.empty()
            || (fieldType == TextFieldType::F_PASSWORD
            && realText.empty()))
        {
            tex->SetSize(100.0f);
            tex->FillColor(0.0f);

            scast<Transform2D&>(mesh->GetTransform()).setsize( 100.0f );
        }
        else
        {
            const i32 ascender = font->GetFontData().ascender;
            const i32 descender = font->GetFontData().descender;

            vec2 penPos{};
            i32 minX{};
            i32 maxX{};

            //calculate size
            for (GlyphRasterData& glyph : displayedText)
            {
                GlyphData* glyphData = font->GetGlyphData(
                    font->GetFontData(),
                    glyph.utf);

                if (!glyphData) continue;

                i32 glyphWidth = scast<i32>(fabsf(glyphData->size.x));

                i32 glyphLeft = penPos.x + scast<i32>(glyphData->bearing.x);
                i32 glyphRight = glyphLeft + glyphWidth;

                minX = min(minX, glyphLeft);
                maxX = max(maxX, glyphRight);

                /*
                Log::Print(
                    "@@@@@\n"
                    "TEXT GLYPH BOUNDS: \n"
                    "  UTF: " + to_string(glyph.utf) + "\n"
                    "  penX: " + to_string(penPos.x) + "\n"
                    "  bearingX: " + to_string(glyphData.bearing.x) + "\n"
                    "  sizeX: " + to_string(glyphData.size.x) + "\n"
                    "  glyphLeft: " + to_string(glyphLeft) + "\n"
                    "  glyphRight: " + to_string(glyphRight) + "\n"
                    "  advance: " + to_string(glyphData.advance));
                */

                glyph.penPos =
                { 
                    penPos.x,
                    0.0f
                };
                glyph.glyphPos = 
                {
                    scast<f32>(glyphLeft),
                    scast<f32>(glyphData->bearing.y + glyphData->size.y - descender)
                };

                glyph.glyphSize = glyphData->size;

                penPos.x += glyphData->advance;
            }

            maxX = max(maxX, scast<i32>(penPos.x));

            /*
            Log::Print(
                "@@@@@\n"
                "FINAL TEXT BOUNDS:\n"
                "  minX: " + to_string(minX) + "\n"
                "  maxX: " + to_string(maxX) + "\n"
                "  finalPenX: " + to_string(penPos.x) + "\n"
                "  finalWidth: " + to_string(maxX - minX));
            */

            const u32 finalWidth = scast<u32>(maxX - minX);
            const u32 finalHeight = scast<u32>(ascender - descender);

            vector<u8> finalPixels(finalWidth * finalHeight, 0);

            //copy glyphs into final texture
            for (const GlyphRasterData& glyph : displayedText)
            {
                vector<u8> glyphPixelData = font->GetGlyphPixelData(glyph.utf);

                //prevent invalid glyphs from doing any further actions
                if (glyphPixelData.empty()) continue;

                u32 glyphWidth  = scast<u32>(fabsf(glyph.glyphSize.x));
                u32 glyphHeight = scast<u32>(fabsf(glyph.glyphSize.y));

                i32 glyphX = scast<i32>(glyph.glyphPos.x) - minX;
                i32 glyphY = scast<i32>(glyph.glyphPos.y);

                for (u32 y = 0; y < glyphHeight; y++)
                {
                    i32 dstY = glyphY + scast<i32>(y);

                    if (dstY < 0
                        || dstY >= scast<i32>(finalHeight))
                    {
                        continue;
                    }

                    for (u32 x = 0; x < glyphWidth; x++)
                    {
                        i32 dstX = glyphX + scast<i32>(x);

                        if (dstX < 0
                            || dstX >= scast<i32>(finalWidth))
                        {
                            continue;
                        }

                        //dont overwrite colored pixels with transparent pixels

                        u8& dstPixel = finalPixels[scast<u32>(dstY) * finalWidth + scast<u32>(dstX)];
                        u8 srcPixel = glyphPixelData[y * glyphWidth + x];

                        if (srcPixel > dstPixel) dstPixel = srcPixel;
                    }
                }
            }

            vec2 finalSize
            {
                scast<f32>(finalWidth),
                scast<f32>(finalHeight)
            };

            tex->SetSize(finalSize);
            tex->SetPixelData(std::move(finalPixels));

            scast<Transform2D&>(mesh->GetTransform()).setsize(finalSize);
        }

        isTextDirty = false;
    }

    void Text::UpdateCursor(f64 deltaTime)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text error",
                "Failed to update text widget '" + to_string(ID) 
                + "' cursor because its mesh was invalid! Reason: " + err);
        }

        Texture* tex{};
        err = Texture::GetRegistry().GetContent(textureID, tex);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text error",
                "Failed to update text widget '" + to_string(ID) 
                + "' cursor because its texture was invalid! Reason: " + err);
        }

        i32 textureWidth = scast<i32>(tex->GetSize().x);
        i32 textureHeight = scast<i32>(tex->GetSize().y);

        auto cursor_on = [
            tex,
            textureWidth,
            textureHeight,
            this]() -> void
            {
                i32 startX = scast<i32>(cursorData.pos.x) - CURSOR_WIDTH_PX / 2;
                i32 startY = scast<i32>(cursorData.pos.y) - CURSOR_HEIGHT_PX / 2;

                startX = clamp(
                    startX, 
                    0, 
                    scast<i32>(tex->GetSize().x) - CURSOR_WIDTH_PX);

                startY = clamp(
                    startY, 
                    0, 
                    scast<i32>(tex->GetSize().y) - CURSOR_HEIGHT_PX);

                vector<u8> pixels = tex->GetPixelData();

                /*
                Log::Print(
                    "@@@@@\n"
                    "  cursor pos: "
                    + to_string(cursorData.pos.x) + ", "
                    + to_string(cursorData.pos.y) + "\n"
                    "  cursor start: "
                    + to_string(startX) + ", "
                    + to_string(startY) + "\n"
                    "  texture size: "
                    + to_string(textureWidth) + ", "
                    + to_string(textureHeight));
                */

                /*
                Log::Print(
                    "@@@@@ cursor on pixel data size: "
                    + to_string(pixels.size())
                    + ", expected R8 size: "
                    + to_string(textureWidth * textureHeight));
                */

                cursorData.cursorBackPixels.clear();
                cursorData.cursorBackPixels.reserve(
                    CURSOR_WIDTH_PX
                    * CURSOR_HEIGHT_PX);

                cursorData.cursorBackPos = 
                {
                    scast<f32>(startX),
                    scast<f32>(startY)
                };

                for (u8 y = 0; y < CURSOR_HEIGHT_PX; y++)
                {
                    for (u8 x = 0; x < CURSOR_WIDTH_PX; x++)
                    {
                        u32 pixelX = scast<u32>(startX + x);
                        u32 pixelY = scast<u32>(startY + y);

                        if (pixelX >= scast<u32>(textureWidth)
                            || pixelY >= scast<u32>(textureHeight))
                        {
                            Log::Print(
                                "Cursor pixel out of bounds: "
                                + to_string(pixelX) + ", "
                                + to_string(pixelY) + "\n"
                                + "  texture size: "
                                + to_string(textureWidth) + ", "
                                + to_string(textureHeight),
                                "KG_TEXT",
                                LogType::LOG_WARNING);

                            return;
                        }

                        u32 index = pixelY * textureWidth + pixelX;

                        if (index >= pixels.size())
                        {
                            Log::Print(
                                "Cursor index out of bounds: "
                                + to_string(index) + "\n"
                                + "  pixel count: "
                                + to_string(pixels.size()),
                                "KG_TEXT",
                                LogType::LOG_WARNING);

                            return;
                        }

                        cursorData.cursorBackPixels.push_back(pixels[index]);

                        bool border = 
                            x == 0
                            || x == CURSOR_WIDTH_PX - 1
                            || y == 0
                            || y == CURSOR_HEIGHT_PX - 1;

                        pixels[index] = border ? 0 : 255;
                    }
                }

                tex->SetPixelData(std::move(pixels));
            };

        auto cursor_off = [
            tex,
            textureWidth,
            textureHeight,
            this]() -> void
            {
                vector<u8> pixels = tex->GetPixelData();

                /*
                Log::Print(
                    "@@@@@ cursor off pixel data size: "
                    + to_string(pixels.size())
                    + ", expected R8 size: "
                    + to_string(textureWidth * textureHeight));
                */

                i32 backStartX = scast<i32>(cursorData.cursorBackPos.x);
                i32 backStartY = scast<i32>(cursorData.cursorBackPos.y);

                u32 backPixelIndex{};
                
                for (u8 y = 0; y < CURSOR_HEIGHT_PX; y++)
                {
                    for (u8 x = 0; x < CURSOR_WIDTH_PX; x++)
                    {
                        u32 pixelX = scast<u32>(backStartX + x);
                        u32 pixelY = scast<u32>(backStartY + y);

                        if (pixelX >= scast<u32>(textureWidth)
                            || pixelY >= scast<u32>(textureHeight))
                        {
                            Log::Print(
                                "Restore cursor pixel out of bounds: "
                                + to_string(pixelX) + ", "
                                + to_string(pixelY) + "\n"
                                + "  texture size: "
                                + to_string(textureWidth) + ", "
                                + to_string(textureHeight),
                                "KG_TEXT",
                                LogType::LOG_WARNING);

                            return;
                        }

                        u32 index = pixelY * textureWidth + pixelX;

                        if (index >= pixels.size())
                        {
                            Log::Print(
                                "Restore cursor index out of bounds: "
                                + to_string(index) + "\n"
                                + "  pixel count: "
                                + to_string(pixels.size()),
                                "KG_TEXT",
                                LogType::LOG_WARNING);

                            return;
                        }

                        if (backPixelIndex >= cursorData.cursorBackPixels.size())
                        {
                            Log::Print(
                                "Cursor backing pixel out of bounds: "
                                + to_string(backPixelIndex) + "\n"
                                + "  backing pixel count: "
                                + to_string(cursorData.cursorBackPixels.size()),
                                "KG_TEXT",
                                LogType::LOG_WARNING);

                            return;
                        }

                        pixels[index] = cursorData.cursorBackPixels[backPixelIndex++];
                    }
                }

                cursorData.cursorBackPixels.clear();
                cursorData.cursorBackPos = {};

                tex->SetPixelData(std::move(pixels));
            };

        //update blinking cursor
        if (cursorData.characterSlot != -1
            && !ContainsValue(GraphicsContext::GetDraggingMouseButtons(), MouseButton::M_LEFT))
        {
            cursorData.timeSinceLastStateSwitch += deltaTime;

            if (cursorData.timeSinceLastStateSwitch >= CURSOR_BLINK_INTERVAL_S)
            {
                cursorData.isCursorOn = !cursorData.isCursorOn;
                cursorData.timeSinceLastStateSwitch = 0;

                if (cursorData.isCursorOn) cursor_on();
                else                       cursor_off();
            }
        }

        if (!canEdit)
        {
            if (cursorData.pos != 0)
            {
                cursor_off();
                cursorData = {};   
            }

            return;
        }

        //clear cursor if clicked or dragged away from text field
        if (!m->IsHovered()
            && cursorData.pos != 0
            && (ContainsValue(GraphicsContext::GetPressedMouseButtons(), MouseButton::M_LEFT)
            || ContainsValue(GraphicsContext::GetDraggingMouseButtons(), MouseButton::M_LEFT)))
        {
            cursor_off();
            cursorData = {};

            return;
        }

        //dont proceed with cursor position updates
        //if dragging isnt already live for this text widget and it isnt even hovered
        if (dragStartTextWidget != ID
            && !m->IsHovered())
        {
            return;
        }

        //no mouse update logic was done
        if (!ContainsValue(GraphicsContext::GetPressedMouseButtons(), MouseButton::M_LEFT)
            && !ContainsValue(GraphicsContext::GetDraggingMouseButtons(), MouseButton::M_LEFT))
        {
            return;
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID) 
                + "' cursor because its shader was invalid! Reason: " + err);
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID) 
                + "' cursor because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
        }

        GraphicsContext* gctx{};
        err = GraphicsContext::GetRegistry().GetContent(vp->GetContextID(), gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID) 
                + "' cursor because its viewport '" + to_string(shader->viewportID) + "' graphics context was invalid! Reason: " + err);
        }

        bool clicked = ContainsValue(GraphicsContext::GetPressedMouseButtons(), MouseButton::M_LEFT);

        //place cursor
        if (clicked)
        {
            Log::Print("@@@@@ clicked on text widget...");

            //clear old cursor data
            if (cursorData.pos != 0)
            {
                cursor_off();
                cursorData = {};
            }

            dragStartTextWidget = 0;

            vec2 pos = m->finalAnchorPos;
            vec2 size = scast<Transform2D&>(m->GetTransform()).getsize(SizeTarget::SIZE_WORLD);

            vec2 mousePos = gctx->GetMousePos(true);

            vec2 meshStart = 
            {
                pos.x - size.x * 0.5f,
                pos.y - size.y * 0.5f
            };

            cursorData.pos = 
            {
                mousePos.x - meshStart.x,
                mousePos.y - meshStart.y
            };

            /*
            Log::Print(
                "@@@@@\n"
                "  mouse pos: "
                + to_string(mousePos.x) + ", "
                + to_string(mousePos.y) + "\n"
                "  mesh pos: "
                + to_string(pos.x) + ", "
                + to_string(pos.y) + "\n"
                "  mesh size: "
                + to_string(size.x) + ", "
                + to_string(size.y) + "\n"
                "  mesh start: "
                + to_string(meshStart.x) + ", "
                + to_string(meshStart.y) + "\n"
                "  cursor pos: "
                + to_string(cursorData.pos.x) + ", "
                + to_string(cursorData.pos.y));
            */

            cursorData.characterSlot = 0; //TODO: replace with actual character slot

            cursorData.isCursorOn = true;
            cursor_on();
        }
        //start highlighting
        else
        {
            Log::Print("@@@@@ dragged on text widget...");

            dragStartTextWidget = ID;

            //tbd...
        }
    }

    void Text::Destroy()
    {
        Texture* tex{};
        string err = Texture::GetRegistry().GetContent(textureID, tex);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to destroy text widget '" + to_string(ID) + "' because its texture '" 
                + to_string(textureID) + "' was invalid! Reason: " + err);
        }

        Mesh* mesh{};
        err = Mesh::GetRegistry().GetContent(meshID, mesh);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to destroy text widget '" + to_string(ID) + "' because its mesh '" 
                + to_string(meshID) + "' was invalid! Reason: " + err);
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to destroy text widget '" + to_string(ID) + "' because its shader '" 
                + to_string(shaderID) + "' was invalid! Reason: " + err);
        }

        tex->textWidgetID = 0;
        mesh->textWidgetID = 0;

        erase(shader->textWidgetIDs, ID);

        tex->Destroy();
        mesh->Destroy();

        err = registry.DestroyContent(ID);
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