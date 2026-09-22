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

//cursor width is always 4 pixels
static constexpr u8 cursorWidth = 4;

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
                "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return {};
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(vp->GetRootShaderID(RootShaderTarget::T_FONT), shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to initialize text widget because its viewport '" 
                + to_string(viewportID) + "' root font shader was invalid! Reason: " + err);
        }

        Texture* tex = Texture::Initialize(
            shader->GetID(),
            {
                .format = TexturePixelFormat::FORMAT_BASIC_R8
            });
        if (!tex)
        {
            Log::Print(
                "Failed to initialize text widget because its texture failed to initialize!",
                "KG_WIDGET_TEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        Mesh* mesh = Mesh::Initialize(shader->GetID());
        if (!mesh)
        {
            Log::Print(
                "Failed to initialize text widget because its mesh failed to initialize!",
                "KG_WIDGET_TEXT",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        Material* mat{};
        err = Material::GetRegistry().GetContent(mesh->GetMaterialID(), mat);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to initialize text widget because its mesh material was invalid! Reason: " + err);
        }

        mat->SetMaterial2DType(MaterialType2D::M_FONT);
        mat->SetBaseColorTextureID(tex->GetID());
        mat->SetBaseColor({ vec3{ 0.0f }, 1.0f });

        unique_ptr<Text> newText = make_unique<Text>();
        Text* textPtr = newText.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        textPtr->ID = newID;
        textPtr->fontID = fontID;
        textPtr->shaderID = shader->GetID();
        textPtr->textureID = tex->GetID();
        textPtr->meshID = mesh->GetID();

        tex->textWidgetID = newID;
        mesh->textWidgetID = newID;
        shader->textWidgetIDs.push_back(newID);

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
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (fontID == newValue)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' font ID to '" + to_string(newValue) + "' because it is already the same!",
                "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        fontID = newValue;

        //ensure new font calculates line height again
        lineHeight = 0;

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
        if (buttonWidgetID != 0)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' edit state because it is used in a widget!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        canEdit = newValue;

        if (!canEdit)
        {
            cursorData = {};
            highlightData = {};
        }

        Log::Print(
            "Set text widget '" + to_string(ID) + "' edit state to '" + (canEdit ? "true" : "false") + "'!",
            "KG_WIDGET_TEXT",
            LogType::LOG_SUCCESS);
    }

    TextClipType Text::GetClipType() const { return clipType; }
    void Text::SetClipType(TextClipType newValue)
    {
        if (newValue == clipType)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' clip type because it is already the same!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        clipType = newValue;

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' clip type to '" 
            + string(clipType == TextClipType::C_OVERFLOW ? "overflow" : "clipped") + "'!",
            "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
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
            "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
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
            "KG_WIDGET_TEXT",
            LogType::LOG_SUCCESS);
    }

    i32 Text::GetCursorPos() const { return cursorData.characterSlot; }
    void Text::SetCursorPosByUTF(
        i32 targetUTF,
        i32 targetUTFSlot)
    {
        if (!canEdit)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by UTF because it is not editable!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (targetUTF == -1
            && targetUTFSlot == -1)
        {
            cursorData = {};

            Log::Print(
                "Cleared text widget '" + to_string(ID) + "' cursor pos!",
                "KG_WIDGET_TEXT",
                LogType::LOG_SUCCESS);

            return;
        }

        if (targetUTF < 0)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by UTF because its target utf must be 0 or higher!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }
        if (targetUTFSlot < -1)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by UTF because its target utf slot must be -1 or higher!",
                "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        cursorData = { .characterSlot = targetSlot };

        Log::Print(
            "Set text widget '" + to_string(ID) + "' cursor pos by UTF to UTF '" 
            + to_string(targetUTF) 
            + "' at char slot '" + to_string(targetUTFSlot) + "'!",
            "KG_WIDGET_TEXT",
            LogType::LOG_SUCCESS);
    }
    void Text::SetCursorPosBySlot(i32 targetSlot)
    {
        if (!canEdit)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by slot because it is not editable!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (targetSlot == -1)
        {
            cursorData = {};

            Log::Print(
                "Cleared text widget '" + to_string(ID) + "' cursor pos!",
                "KG_WIDGET_TEXT",
                LogType::LOG_SUCCESS);

            return;
        }

        if (targetSlot < -1)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' cursor pos by slot because its target slot must be -1 or higher!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (scast<u32>(targetSlot) > displayedText.size())
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID)
                + "' cursor pos by slot because slot '" + to_string(targetSlot) 
                + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }
        
        cursorData.characterSlot = targetSlot;

        Log::Print(
            "Set text widget '" + to_string(ID) 
            + "' cursor pos by slot to slot '" + to_string(cursorData.characterSlot) + "'!",
            "KG_WIDGET_TEXT",
            LogType::LOG_SUCCESS);
    }

    pair<i32, i32> Text::GetHighlightRange() const { return highlightData.highlightRange; }
    void Text::SetHighlightRange(pair<i32, i32> newValue)
    {
        if (!canEdit)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' highlight range because it is not editable!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (newValue.first == -1
            && newValue.second == -1)
        {
            highlightData = {};

            Log::Print(
                "Cleared text widget '" + to_string(ID) + "' highlighted text!",
                "KG_WIDGET_TEXT",
                LogType::LOG_SUCCESS);

            return;
        }

        if (newValue.first < 0
            || newValue.second < 0)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' highlight range because first or second was below 0!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (newValue.first >= newValue.second)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' highlight range because first cannot be equal or bigger than second!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        if (scast<u32>(newValue.second) > displayedText.size())
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' highlight range because second '" + to_string(newValue.second) 
                + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        highlightData.highlightRange = newValue;
        highlightData.isHighlightDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' highlight range start to '" 
            + to_string(highlightData.highlightRange.first) + "' and end to "
            + to_string(highlightData.highlightRange.second) + "'!",
            "KG_WIDGET_TEXT",
            LogType::LOG_SUCCESS);
    }

    f32 Text::GetTextSizeMultiplier() const { return textSizeMultiplier; }
    void Text::SetTextSizeMultiplier(f32 newValue)
    {
        if (newValue == textSizeMultiplier)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' size because it is already the same!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        textSizeMultiplier = clamp(newValue, MIN_TEXT_MULTIPLIER_SIZE, MAX_TEXT_MULTIPLIER_SIZE);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' text size multiplier to '" + to_string(textSizeMultiplier) + "'!",
            "KG_WIDGET_TEXT",
            LogType::LOG_SUCCESS);
    }

    u16 Text::GetLineWidth() const { return lineWidth; }
    void Text::SetLineWidth(u16 newValue)
    {
        if (newValue == lineWidth)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' line width because it is already the same!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        lineWidth = clamp(newValue, MIN_LINE_WIDTH, MAX_LINE_WIDTH);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' line width to '" + to_string(lineWidth) + "'!",
            "KG_WIDGET_TEXT",
            LogType::LOG_SUCCESS);
    }

    u16 Text::GetLineHeight() const { return lineHeight; }

    u16 Text::GetMaxLines() const { return maxLines; }
    void Text::SetMaxLines(u16 newValue)
    {
        if (newValue == maxLines)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' max lines because it is already the same!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        maxLines = clamp(newValue, scast<u16>(1), MAX_LINES);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' max lines to '" + to_string(maxLines) + "'!",
            "KG_WIDGET_TEXT",
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
            "KG_WIDGET_TEXT",
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
            "KG_WIDGET_TEXT",
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
            "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
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
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > displayedText.size())
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID)
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (count > displayedText.size() - startChar)
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID) 
                    + "' because removal count from start character '" + to_string(count) 
                    + "' exceeds total character count '" + to_string(displayedText.size() - startChar) + "'!",
                    "KG_WIDGET_TEXT",
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
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > realText.size())
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID)
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(realText.size()) + "'!",
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (count > realText.size() - startChar)
            {
                Log::Print(
                    "Failed to remove " + textTypeStr + " characters from text widget '" + to_string(ID) 
                    + "' because removal count from start character '" + to_string(count) 
                    + "' exceeds total character count '" + to_string(realText.size() - startChar) + "'!",
                    "KG_WIDGET_TEXT",
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

        if (cursorData.characterSlot != -1) cursorData.isCursorPosDirty = true;
        if (highlightData.highlightRange != pair{ -1, -1 }) highlightData.isHighlightDirty = true;

        if (isVerboseLoggingEnabled)
        {
            string target = back ? "back" : "front";

            Log::Print(
                "Removed '" + to_string(count) + "' " + textTypeStr + " characters from text widget '" + to_string(ID) + "' text " + target + "!",
                "KG_WIDGET_TEXT",
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
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (newValue.size() + displayedText.size() > maxCharacters)
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because added character count '" + to_string(newValue.size() + displayedText.size()) 
                    + "' exceeds max character count '" + to_string(maxCharacters) + "'!",
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > displayedText.size())
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(displayedText.size()) + "'!",
                    "KG_WIDGET_TEXT",
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
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (newValue.size() + realText.size() > maxCharacters)
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because added character count '" + to_string(newValue.size() + realText.size()) 
                    + "' exceeds max character count '" + to_string(maxCharacters) + "'!",
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (startChar > realText.size())
            {
                Log::Print(
                    "Failed to add " + textTypeStr + " characters to text widget '" + to_string(ID) 
                    + "' because start character '" + to_string(startChar) 
                    + "' exceeds total character count '" + to_string(realText.size()) + "'!",
                    "KG_WIDGET_TEXT",
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

        if (cursorData.characterSlot != -1) cursorData.isCursorPosDirty = true;
        if (highlightData.highlightRange != pair{ -1, -1 }) highlightData.isHighlightDirty = true;

        if (isVerboseLoggingEnabled)
        {
            string target = back ? "back" : "front";

            Log::Print(
                "Added new " + textTypeStr + " characters to text widget '" + to_string(ID) + "' " + target + "!",
                "KG_WIDGET_TEXT",
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
                "KG_WIDGET_TEXT",
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
                    "KG_WIDGET_TEXT",
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
                    "KG_WIDGET_TEXT",
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
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            if (newValue == realText)
            {
                Log::Print(
                    "Failed to update text widget '" + to_string(ID) 
                    + "' " + textTypeStr + " characters because they are already the same!",
                    "KG_WIDGET_TEXT",
                    LogType::LOG_WARNING);

                return;
            }

            realText = std::move(newValue);

            if (fieldType == TextFieldType::F_PASSWORD)
            {
                isTextDirty = true;
            }
        }

        if (cursorData.characterSlot != -1) cursorData.isCursorPosDirty = true;
        if (highlightData.highlightRange != pair{ -1, -1 }) highlightData.isHighlightDirty = true;

        if (isVerboseLoggingEnabled)
        {
            Log::Print(
                "Overwrote text widget '" + to_string(ID) + "' " + textTypeStr + " characters!",
                "KG_WIDGET_TEXT",
                LogType::LOG_VERBOSE);
        }
    }

    void Text::Update(VkCommandBuffer buffer)
    {
        ImportFont* font{};
        string err = ImportFont::GetRegistry().GetContent(fontID, font);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID) + "' because its font '" 
                + to_string(fontID) + "' was invalid! Reason: " + err);
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

        mesh->Update(buffer);

        bool hasSizeUpdate{};
        vec2 meshSize = scast<Transform2D&>(mesh->GetTransform()).getsize(SizeTarget::SIZE_WORLD);
        if (tex->GetSize() != meshSize)
        {
            tex->SetSize(meshSize);
            hasSizeUpdate = true;

            Log::Print("@@@@@ text widget '" + to_string(ID) + "' size changed... updating its texture size");
        }

        if (!isTextDirty
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

            for (GlyphRasterData& glyph : displayedText)
            {
                glyph.penPos.x -= minX;
            }

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

        ImportFont* font{};
        err = ImportFont::GetRegistry().GetContent(fontID, font);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to update text widget '" + to_string(ID)
                + "' cursor because its font '" + to_string(fontID) + "' was invalid! Reason: " + err);
        }

        if (lineHeight == 0)
        {
            for (const GlyphData& gd : font->GetFontData().glyphs)
            {
                lineHeight = max(
                    lineHeight, 
                    scast<u16>(fabsf(gd.bearing.y)));
            }

            //Log::Print("@@@@@ set line height to '" + to_string(lineHeight) + "'...");
        }

        //set fixed cursor height and line height

        i32 textureWidth = scast<i32>(tex->GetSize().x);
        i32 textureHeight = scast<i32>(tex->GetSize().y);

        //cursor height is derived from texture height for now...
        //TODO: make it follow line height
        u32 cursorHeight = scast<u32>(textureHeight * 0.8f);

        auto cursor_on = [
            tex,
            textureWidth,
            textureHeight,
            cursorHeight,
            this,
            font]() -> void
            {
                //position cursor from logical character slot
                if (cursorData.isCursorPosDirty)
                {
                    if (cursorData.characterSlot == 0)
                    {
                        if (displayedText.empty())
                        {
                            cursorData.pos.x = textureWidth * 0.5f;
                        }
                        else
                        {
                            cursorData.pos.x = displayedText[0].penPos.x;
                        }
                    }
                    else if (scast<u32>(cursorData.characterSlot) < displayedText.size())
                    {
                        cursorData.pos.x = displayedText[cursorData.characterSlot].penPos.x;
                    }
                    else if (!displayedText.empty())
                    {
                        const GlyphRasterData& last = displayedText.back();
                        const GlyphData* glyphData = font->GetGlyphData(
                            font->GetFontData(),
                            last.utf);

                        cursorData.pos.x = last.penPos.x + glyphData->advance;
                    }

                    cursorData.pos.y = 
                        -font->GetFontData().descender 
                        + lineHeight * 0.5f;

                    //Log::Print("@@@@@ set cursor y pos to '" + to_string(cursorData.pos.y) + "'...");

                    cursorData.isCursorPosDirty = false;
                }

                i32 startX = scast<i32>(cursorData.pos.x) - cursorWidth / 2;
                i32 startY = scast<i32>(cursorData.pos.y) - cursorHeight / 2;

                startX = clamp(
                    startX, 
                    0, 
                    scast<i32>(tex->GetSize().x) - cursorWidth);

                startY = clamp(
                    startY, 
                    0, 
                    scast<i32>(tex->GetSize().y) - scast<i32>(cursorHeight));

                vector<u8> pixels = tex->GetPixelData();

                cursorData.cursorBackPixels.clear();
                cursorData.cursorBackPixels.reserve(
                    cursorWidth
                    * cursorHeight);

                cursorData.cursorBackPos = 
                {
                    scast<f32>(startX),
                    scast<f32>(startY)
                };

                for (u32 y = 0; y < cursorHeight; y++)
                {
                    for (u32 x = 0; x < cursorWidth; x++)
                    {
                        u32 pixelX = scast<u32>(startX + x);
                        u32 pixelY = scast<u32>(startY + y);

                        if (pixelX >= scast<u32>(textureWidth)
                            || pixelY >= scast<u32>(textureHeight))
                        {
                            /*
                            Log::Print(
                                "@@@@@\n"
                                "  cursor pixel out of bounds: "
                                + to_string(pixelX) + ", "
                                + to_string(pixelY) + "\n"
                                + "  texture size: "
                                + to_string(textureWidth) + ", "
                                + to_string(textureHeight));
                            */

                            return;
                        }

                        u32 index = pixelY * textureWidth + pixelX;

                        if (index >= pixels.size())
                        {
                            /*
                            Log::Print(
                                "@@@@@\n"
                                "  cursor index out of bounds: "
                                + to_string(index) + "\n"
                                + "  pixel count: "
                                + to_string(pixels.size()));
                            */

                            return;
                        }

                        cursorData.cursorBackPixels.push_back(pixels[index]);

                        bool border = 
                            x == 0
                            || x == cursorWidth - 1
                            || y == 0
                            || y == cursorHeight - 1;

                        pixels[index] = border ? 0 : 255;
                    }
                }

                tex->SetPixelData(std::move(pixels));
            };

        auto cursor_off = [
            tex,
            textureWidth,
            textureHeight,
            cursorHeight,
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
                
                for (u32 y = 0; y < cursorHeight; y++)
                {
                    for (u32 x = 0; x < cursorWidth; x++)
                    {
                        u32 pixelX = scast<u32>(backStartX + x);
                        u32 pixelY = scast<u32>(backStartY + y);

                        if (pixelX >= scast<u32>(textureWidth)
                            || pixelY >= scast<u32>(textureHeight))
                        {
                            /*
                            Log::Print(
                                "@@@@@\n"
                                "  restore cursor pixel out of bounds: "
                                + to_string(pixelX) + ", "
                                + to_string(pixelY) + "\n"
                                + "  texture size: "
                                + to_string(textureWidth) + ", "
                                + to_string(textureHeight));
                            */

                            return;
                        }

                        u32 index = pixelY * textureWidth + pixelX;

                        if (index >= pixels.size())
                        {
                            /*
                            Log::Print(
                                "@@@@@\n"
                                "  restore cursor index out of bounds: "
                                + to_string(index) + "\n"
                                + "  pixel count: "
                                + to_string(pixels.size()));
                            */

                            return;
                        }

                        if (backPixelIndex >= cursorData.cursorBackPixels.size())
                        {
                            /*
                            Log::Print(
                                "@@@@@\n"
                                "  cursor backing pixel out of bounds: "
                                + to_string(backPixelIndex) + "\n"
                                + "  backing pixel count: "
                                + to_string(cursorData.cursorBackPixels.size()));
                            */

                            return;
                        }

                        pixels[index] = cursorData.cursorBackPixels[backPixelIndex++];
                    }
                }

                cursorData.cursorBackPixels.clear();
                cursorData.cursorBackPos = {};

                tex->SetPixelData(std::move(pixels));
            };

        auto hightlight_on = [
            tex,
            textureWidth,
            textureHeight,
            this,
            font]() -> void
            {
                if (highlightData.highlightRange.first == -1
                    || highlightData.highlightRange.second == -1)
                {
                    return;
                }

                i32 startSlot = min(
                    highlightData.highlightRange.first,
                    highlightData.highlightRange.second);

                i32 endSlot = max(
                    highlightData.highlightRange.first,
                    highlightData.highlightRange.second);

                if (startSlot == endSlot) return;

                f32 startX{};

                if (scast<u32>(startSlot) < displayedText.size())
                {
                    startX = displayedText[startSlot].penPos.x;
                }
                else
                {
                    const GlyphRasterData& last = displayedText.back();

                    const GlyphData* glyphData = font->GetGlyphData(
                        font->GetFontData(),
                        last.utf);
                        
                    startX = last.penPos.x + glyphData->advance;
                }

                f32 endX{};

                if (scast<u32>(endSlot) < displayedText.size())
                {
                    endX = displayedText[endSlot].penPos.x;
                }
                else
                {
                    const GlyphRasterData& last = displayedText.back();

                    const GlyphData* glyphData = font->GetGlyphData(
                        font->GetFontData(),
                        last.utf);
                        
                    endX = last.penPos.x + glyphData->advance;
                }

                i32 highlightStartX = scast<i32>(startX);
                i32 highlightStartY = -font->GetFontData().descender;

                u32 highlightWidth = scast<u32>(fabsf(endX - startX));
                u32 highlightHeight = lineHeight;

                if (highlightWidth == 0
                    || highlightHeight == 0)
                {
                    return;
                }

                highlightStartX = clamp(
                    highlightStartX,
                    0,
                    scast<i32>(textureWidth) - 1);

                highlightStartY = clamp(
                    highlightStartY,
                    0,
                    scast<i32>(textureHeight) - 1);

                highlightWidth = min(
                    highlightWidth,
                    textureWidth - scast<u32>(highlightStartX));

                highlightHeight = min(
                    highlightHeight,
                    textureHeight - scast<u32>(highlightStartY));

                vector<u8> pixels = tex->GetPixelData();

                highlightData.highlightBackPixels.clear();
                highlightData.highlightBackPixels.reserve(
                    highlightWidth 
                    * highlightHeight);

                highlightData.highlightBackPos =
                {
                    scast<f32>(highlightStartX),
                    scast<f32>(highlightStartY)
                };

                highlightData.highlightBackSize = 
                {
                    scast<f32>(highlightWidth),
                    scast<f32>(highlightHeight)
                };

                for (u32 y = 0; y < highlightHeight; y++)
                {
                    for (u32 x = 0; x < highlightWidth; x++)
                    {
                        u32 pixelX = scast<u32>(highlightStartX) + x;
                        u32 pixelY = scast<u32>(highlightStartY) + y;

                        u32 index = pixelY * textureWidth + pixelX;

                        highlightData.highlightBackPixels.push_back(pixels[index]);

                        //temporary visual highlight:
                        //invert whatever is already underneath
                        pixels[index] = 255 - pixels[index];
                    }
                }

                tex->SetPixelData(std::move(pixels));
            };

        auto hightlight_off = [
            tex,
            textureWidth,
            textureHeight,
            this]() -> void
            {
                if (highlightData.highlightBackPixels.empty()) return;

                i32 startX = scast<i32>(highlightData.highlightBackPos.x);
                i32 startY = scast<i32>(highlightData.highlightBackPos.y);

                u32 width = scast<u32>(highlightData.highlightBackSize.x);
                u32 height = scast<u32>(highlightData.highlightBackSize.y);

                vector<u8> pixels = tex->GetPixelData();

                u32 backIndex{};

                for (u32 y = 0; y < height; y++)
                {
                    for (u32 x = 0; x < width; x++)
                    {
                        i32 pixelX = startX + x;
                        i32 pixelY = startY + y;

                        if (pixelX < 0
                            || pixelY < 0
                            || pixelX >= scast<i32>(textureWidth)
                            || pixelY >= scast<i32>(textureHeight))
                        {
                            backIndex++;
                            continue;
                        }

                        u32 index = 
                            scast<u32>(pixelY) * textureWidth 
                            + scast<u32>(pixelX);

                        if (backIndex < highlightData.highlightBackPixels.size())
                        {
                            pixels[index] = highlightData.highlightBackPixels[backIndex];
                        }

                        backIndex++;
                    }
                }

                highlightData.highlightBackPixels.clear();
                highlightData.highlightBackPos = {};
                highlightData.highlightBackSize = {};

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

        if (m->ignoreHover
            || !canEdit)
        {
            if (cursorData.characterSlot != -1)
            {
                cursor_off();
                cursorData = {};   
            }

            if (highlightData.highlightRange != pair{ -1, -1 })
            {
                hightlight_off();
                highlightData = {};
            }

            return;
        }

        if (highlightData.highlightRange.first != -1
            && highlightData.highlightRange.second != -1
            && cursorData.characterSlot != -1)
        {
            cursor_off();
            cursorData = {};
        }

        if (highlightData.isHighlightDirty)
        {
            hightlight_off();
            hightlight_on();

            highlightData.isHighlightDirty = false;
        }

        bool isCursorActive = cursorData.characterSlot != -1;
        bool wasHighlighted = highlightData.highlightRange != pair{ -1, -1 };

        if (isCursorActive
            || wasHighlighted)
        {
            bool heldLeftCtrl = ContainsValue(GraphicsContext::GetHeldKeys(), KeyboardButton::K_LEFT_CTRL);
            bool heldLeftShift = ContainsValue(GraphicsContext::GetHeldKeys(), KeyboardButton::K_LEFT_SHIFT);

            bool pressedEnter = ContainsValue(GraphicsContext::GetPressedKeys(), KeyboardButton::K_RETURN);

            u32 pressedChar = GraphicsContext::GetModifierChar(); 

            if (pressedChar != 0
                || GraphicsContext::GetBackspaceState()
                || GraphicsContext::GetTabState()
                || GraphicsContext::GetLeftArrowState()
                || GraphicsContext::GetRightArrowState()
                || GraphicsContext::GetUpArrowState()
                || GraphicsContext::GetDownArrowState()
                || pressedEnter)
            {
                auto delete_highlight_range = [this, hightlight_off]() -> void
                    {
                        i32 highlightStart = min(
                            highlightData.highlightRange.first,
                            highlightData.highlightRange.second);

                        cursorData.characterSlot = highlightStart;

                        u32 highlightCount = scast<u32>(abs(
                            highlightData.highlightRange.second
                            - highlightData.highlightRange.first));

                        hightlight_off();
                        highlightData = {};

                        RemoveText(
                            highlightCount,
                            highlightStart,
                            false);
                    };

                if (pressedChar != 0
                    && !heldLeftCtrl)
                {
                    cursor_off();

                    if (wasHighlighted) delete_highlight_range();

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

                            AddUTF(
                                { pressedChar },
                                cursorData.characterSlot,
                                false);
                        }
                        else
                        {
                            Log::Print(
                                "Found empty utf '" + to_string(pressedChar) + "', replaced with placeholder '?'.",
                                "KG_WIDGET_TEXT",
                                LogType::LOG_WARNING);

                            //fallback ?
                            AddUTF(
                                { 0x003F },
                                cursorData.characterSlot,
                                false);
                        }
                    }
                    else
                    {
                        Log::Print(
                            "Did not find utf '" + to_string(pressedChar) + "', replaced with placeholder '?'.",
                            "KG_WIDGET_TEXT",
                            LogType::LOG_WARNING);

                        //fallback ?
                        AddUTF(
                            { 0x003F },
                            cursorData.characterSlot,
                            false);
                    }

                    cursorData.characterSlot++;
                    cursorData.isCursorPosDirty = true;

                    cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
                    cursorData.isCursorOn = false;
                }

                if (GraphicsContext::GetBackspaceState()
                    && displayedText.size() > 0
                    && (isCursorActive
                    || wasHighlighted))
                {
                    cursor_off();

                    if (wasHighlighted) delete_highlight_range();
                    else
                    {
                        RemoveText(
                            1, 
                            cursorData.characterSlot - 1,
                            false);

                        cursorData.characterSlot--;
                    }

                    cursorData.isCursorPosDirty = true;

                    cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
                    cursorData.isCursorOn = false;
                }

                if (GraphicsContext::GetTabState()
                    && displayedText.size() + 4 <= maxCharacters)
                {
                    cursor_off();

                    if (wasHighlighted)
                    {
                        delete_highlight_range();
                    }

                    //four spaces for tab
                    AddUTF(
                        {
                            0x0020,
                            0x0020,
                            0x0020,
                            0x0020
                        },
                        cursorData.characterSlot,
                        false);

                    cursorData.isCursorPosDirty = true;
                    cursorData.characterSlot += 4;

                    cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
                    cursorData.isCursorOn = false;
                }

                auto is_space = [](u32 utf) -> bool
                    {
                        return utf == 0x0020;
                    };
                auto is_dot = [](u32 utf) -> bool
                    {
                        return utf == 0x002E;
                    };

                //space or dot
                auto is_separator = [is_space, is_dot](u32 utf) -> bool
                    {
                        return is_space(utf) || is_dot(utf);
                    };

                //move cursor left
                if (GraphicsContext::GetLeftArrowState()
                    && (isCursorActive
                    || wasHighlighted))
                {
                    cursor_off();

                    if (wasHighlighted
                        && !(heldLeftCtrl
                        && heldLeftShift))
                    {
                        i32 highlightStart = min(
                            highlightData.highlightRange.first,
                            highlightData.highlightRange.second);

                        cursorData.characterSlot = highlightStart;

                        hightlight_off();
                        highlightData = {};
                    }

                    if (!heldLeftCtrl)
                    {
                        if (!wasHighlighted) cursorData.characterSlot--;
                    }
                    else
                    {
                        i32 startSlot = wasHighlighted
                            && heldLeftShift
                            ? highlightData.highlightRange.second
                            : cursorData.characterSlot;

                        i32 targetSlot{};
                        i32 i = startSlot - 1;

                        //skip all spaces between words
                        while (i >= 0
                            && is_space(displayedText[i].utf))
                        {
                            i--;
                        }

                        //skip one adjacent dot
                        if (i >= 0
                            && is_dot(displayedText[i].utf))
                        {
                            i--;
                        }

                        for (; i >= 0; i--)
                        {
                            if (is_separator(displayedText[i].utf))
                            {
                                targetSlot = i + 1;
                                break;
                            }
                        }

                        if (!heldLeftShift) cursorData.characterSlot = targetSlot;
                        else
                        {
                            if (!wasHighlighted)
                            {
                                highlightData.highlightRange = 
                                {
                                    cursorData.characterSlot,
                                    targetSlot
                                };

                                cursorData = {};
                            }
                            else highlightData.highlightRange.second = targetSlot;

                            highlightData.isHighlightDirty = true;
                        }
                    }

                    if (!(heldLeftCtrl
                        && heldLeftShift))
                    {
                        cursorData.isCursorPosDirty = true;

                        cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
                        cursorData.isCursorOn = false;
                    }
                }

                //highlight everything
                if (heldLeftCtrl
                    && ContainsValue(GraphicsContext::GetHeldKeys(), KeyboardButton::K_A)
                    && !displayedText.empty()
                    && highlightData.highlightRange != pair{ 0, displayedText.size() })
                {
                    if (isCursorActive)
                    {
                        cursor_off();
                        cursorData = {};
                    }

                    highlightData.highlightRange = pair{ 0, displayedText.size() };
                    highlightData.isHighlightDirty = true;
                }

                //move cursor right
                if (GraphicsContext::GetRightArrowState()
                    && (scast<u32>(cursorData.characterSlot) < displayedText.size()
                    || wasHighlighted))
                {
                    cursor_off();

                    if (wasHighlighted
                        && !(heldLeftCtrl
                        && heldLeftShift))
                    {
                        i32 highlightEnd = max(
                            highlightData.highlightRange.first,
                            highlightData.highlightRange.second);

                        cursorData.characterSlot = highlightEnd;

                        hightlight_off();
                        highlightData = {};
                    }

                    if (!heldLeftCtrl)
                    {
                        if (!wasHighlighted) cursorData.characterSlot++;
                    }
                    else
                    {
                        i32 startSlot = wasHighlighted
                            && heldLeftShift
                            ? highlightData.highlightRange.second
                            : cursorData.characterSlot;

                        i32 targetSlot = displayedText.size();
                        i32 i = startSlot;

                        //skip all spaces between words
                        while (i < scast<i32>(displayedText.size())
                            && is_space(displayedText[i].utf))
                        {
                            i++;
                        }

                        //skip one adjacent dot
                        if (i < scast<i32>(displayedText.size())
                            && is_dot(displayedText[i].utf))
                        {
                            i++;
                        }

                        for (; i < scast<i32>(displayedText.size()); i++)
                        {
                            if (is_separator(displayedText[i].utf))
                            {
                                targetSlot = i;
                                break;
                            }
                        }

                        if (!heldLeftShift) cursorData.characterSlot = targetSlot;
                        else
                        {
                            if (!wasHighlighted)
                            {
                                highlightData.highlightRange = 
                                {
                                    cursorData.characterSlot,
                                    targetSlot
                                };

                                cursorData = {};
                            }
                            else highlightData.highlightRange.second = targetSlot;

                            highlightData.isHighlightDirty = true;
                        }
                    }

                    if (!(heldLeftCtrl
                        && heldLeftShift))
                    {
                        cursorData.isCursorPosDirty = true;

                        cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
                        cursorData.isCursorOn = false;
                    }
                }

                //move cursor to above line or start of current line
                if (GraphicsContext::GetUpArrowState()
                    && (isCursorActive
                    || wasHighlighted))
                {
                    cursor_off();

                    if (wasHighlighted)
                    {
                        hightlight_off();
                        highlightData = {};
                    }

                    if (maxLines == 1
                        || cursorData.line == 1)
                    {
                        cursorData.characterSlot = 0;
                    }
                    else
                    {
                        //TODO: go to above line
                    }

                    cursorData.isCursorPosDirty = true;

                    cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
                    cursorData.isCursorOn = false;
                }

                //move cursor to below line or end of current line
                if (GraphicsContext::GetDownArrowState()
                    && (scast<u32>(cursorData.characterSlot) < displayedText.size()
                    || wasHighlighted))
                {
                    cursor_off();

                    if (wasHighlighted)
                    {
                        hightlight_off();
                        highlightData = {};
                    }

                    if (cursorData.line == maxLines)
                    {
                        cursorData.characterSlot = displayedText.size();
                    }
                    else
                    {
                        //TODO: go to below line
                    }

                    cursorData.isCursorPosDirty = true;

                    cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
                    cursorData.isCursorOn = false;
                }

                //single-line fields can never add a return value
                if (pressedEnter
                    && maxLines > 1)
                {
                    AddUTF({ 0x0A });
                }
            }   
        }

        //clear cursor if clicked or dragged away from text field
        if (!m->IsHovered()
            && isCursorActive
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

        bool dragging = ContainsValue(GraphicsContext::GetDraggingMouseButtons(), MouseButton::M_LEFT);

        //place cursor
        if (clicked)
        {
            //Log::Print("@@@@@ clicked on text widget...");

            if (wasHighlighted)
            {
                hightlight_off();
                highlightData = {};
            }

            dragStartTextWidget = 0;
            highlightData.dragStartMousePos = gctx->GetMousePos(true);

            //clear old cursor data
            if (isCursorActive)
            {
                cursor_off();
                cursorData = {};
            }
            cursorData.isCursorPosDirty = true;

            vec2 pos = m->finalAnchorPos;
            vec2 size = scast<Transform2D&>(m->GetTransform()).getsize(SizeTarget::SIZE_WORLD);

            vec2 meshStart = 
            {
                pos.x - size.x * 0.5f,
                pos.y - size.y * 0.5f
            };

            vec2 clickPos = 
            {
                highlightData.dragStartMousePos.x - meshStart.x,
                highlightData.dragStartMousePos.y - meshStart.y
            };

            if (displayedText.empty())
            {
                cursorData.characterSlot = 0;
            }
            else
            {
                f32 clickX = clickPos.x;
                f32 closestDistance = FLT_MAX;

                for (u32 i = 0; i < displayedText.size(); i++)
                {
                    f32 distance = fabsf(clickX - displayedText[i].penPos.x);

                    if (distance < closestDistance)
                    {
                        closestDistance = distance;

                        cursorData.characterSlot = scast<i32>(i);
                    }
                }

                const GlyphRasterData& last = displayedText.back();

                const GlyphData* glyphData = font->GetGlyphData(
                    font->GetFontData(),
                    last.utf);

                f32 endX = last.penPos.x + glyphData->advance;
                f32 distance = fabsf(clickX - endX);

                if (distance < closestDistance)
                {
                    cursorData.characterSlot = scast<i32>(displayedText.size());
                }
            }

            cursorData.timeSinceLastStateSwitch = CURSOR_BLINK_INTERVAL_S;
            cursorData.isCursorOn = false;
        }
        //start highlighting
        else if (dragging)
        {
            if (displayedText.empty()) return;

            vec2 mousePos = gctx->GetMousePos(true);
            f32 dragDistance = length(mousePos - highlightData.dragStartMousePos);

            if (dragDistance < DRAG_THRESHOLD_PX) return;

            //Log::Print("@@@@@ dragged on text widget...");

            dragStartTextWidget = ID;

            vec2 pos = m->finalAnchorPos;
            vec2 size = scast<Transform2D&>(m->GetTransform()).getsize(SizeTarget::SIZE_WORLD);

            vec2 meshStart = 
            {
                pos.x - size.x * 0.5f,
                pos.y - size.y * 0.5f
            };

            const GlyphRasterData& last = displayedText.back();

            const GlyphData* glyphData = font->GetGlyphData(
                font->GetFontData(), 
                last.utf);

            f32 endX = last.penPos.x + glyphData->advance;

            if (highlightData.highlightRange.first == -1)
            {
                vec2 clickPos = 
                {
                    highlightData.dragStartMousePos.x - meshStart.x,
                    highlightData.dragStartMousePos.y - meshStart.y
                };
                
                f32 clickX = clickPos.x;
                f32 closestDistance = FLT_MAX;

                for (u32 i = 0; i < displayedText.size(); i++)
                {
                    f32 distance = fabsf(clickX - displayedText[i].penPos.x);

                    if (distance < closestDistance)
                    {
                        closestDistance = distance;

                        highlightData.highlightRange.first = scast<i32>(i);
                    }
                }

                f32 distance = fabsf(clickX - endX);

                if (distance < closestDistance)
                {
                    highlightData.highlightRange.first = scast<i32>(displayedText.size());
                }

                cursor_off();
                cursorData = {};
            }

            vec2 dragPos = 
            {
                mousePos.x - meshStart.x,
                mousePos.y - meshStart.y
            };

            f32 dragX = dragPos.x;
            f32 closestDistance = FLT_MAX;

            for (u32 i = 0; i < displayedText.size(); i++)
            {
                f32 distance = fabsf(dragX - displayedText[i].penPos.x);

                if (distance < closestDistance)
                {
                    closestDistance = distance;

                    highlightData.highlightRange.second = scast<i32>(i);
                }
            }

            f32 distance = fabsf(dragX - endX);

            if (distance < closestDistance)
            {
                highlightData.highlightRange.second = scast<i32>(displayedText.size());
            }

            /*
            Log::Print(
                "@@@@@ start: " + to_string(highlightData.highlightRange.first) 
                + ", end: " + to_string(highlightData.highlightRange.second));
            */

            highlightData.isHighlightDirty = true;
        }
    }

    void Text::Destroy()
    {
        if (buttonWidgetID != 0)
        {
            Log::Print(
                "Failed to destroy text widget '" + to_string(ID) 
                + "' because it is used in a widget!",
                "KG_WIDGET_TEXT",
                LogType::LOG_WARNING);

            return;
        }

        Texture* tex{};
        string err = Texture::GetRegistry().GetContent(textureID, tex);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to destroy text widget '" + to_string(ID) 
                + "' because its texture was invalid! Reason: " + err);
        }

        Mesh* mesh{};
        err = Mesh::GetRegistry().GetContent(meshID, mesh);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to destroy text widget '" + to_string(ID) 
                + "' because its mesh was invalid! Reason: " + err);
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics text widget error",
                "Failed to destroy text widget '" + to_string(ID) 
                + "' because its shader was invalid! Reason: " + err);
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
                "Failed to destroy text widget! Reason: " + err);
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