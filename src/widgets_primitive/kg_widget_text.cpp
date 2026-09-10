//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"
#include "key_standards.hpp"

#include "widgets_primitive/kg_widget_text.hpp"
#include "import/kg_import_font.hpp"
#include "graphics/kg_viewport.hpp"
#include "graphics/kg_shader.hpp"
#include "graphics/kg_texture.hpp"
#include "graphics/kg_material.hpp"
#include "graphics/kg_mesh.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::Transform2D;
using KalaHeaders::KalaMath::vec3;
using KalaHeaders::KalaMath::vec2;

using KalaHeaders::KalaKeyStandards::GetUTFByValue;
using KalaHeaders::KalaKeyStandards::GetValueByUTF;

using KalaGraphics::Core::KalaGraphicsCore;

using KalaGraphics::Import::GlyphData;
using KalaGraphics::Import::ImportFont;

using KalaGraphics::Graphics::RootShaderTarget;
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

static vector<u32> StringToUTF(string&& input)
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
                LogType::LOG_ERROR,
                2);

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
                LogType::LOG_ERROR,
                2);

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
        if (fontID == 0)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' font ID because it was empty!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (fontID == newValue)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) 
                + "' font ID to '" + to_string(newValue) + "' because it is already the same!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

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
                LogType::LOG_ERROR,
                2);

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

    ClipType Text::GetClipType() const { return clipType; }
    void Text::SetClipType(ClipType newValue)
    {
        if (newValue == clipType)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' clip type because it is already the same!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        clipType = newValue;

        isTextDirty = true;

        Log::Print(
            "Set text widget clip type to '" + string(clipType == ClipType::C_OVERFLOW ? "overflow" : "clipped") + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    FieldType Text::GetFieldType() const { return fieldType; }
    void Text::SetFieldType(FieldType newValue)
    {
        if (fieldType == newValue)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' field type because it is already the same!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        fieldType = newValue;

        string fieldTypeStr{};

        switch (fieldType)
        {
        default:
        case FieldType::F_ANY:
            fieldTypeStr = "any";
            break;
        case FieldType::F_TEXT_ONLY:
            fieldTypeStr = "text only";
            break;
        case FieldType::F_NUMBER_ONLY:
            fieldTypeStr = "number only";

            SetNumberMin(-DBL_MAX);
            SetNumberMax(DBL_MAX);
            break;
        case FieldType::F_INTEGER_ONLY:
            fieldTypeStr = "integer only";

            SetNumberMin(INT64_MIN);
            SetNumberMax(INT64_MAX);
            break;
        case FieldType::F_FLOAT_ONLY:
            fieldTypeStr = "float only";

            SetNumberMin(-FLT_MAX);
            SetNumberMax(FLT_MAX);
            break;
        case FieldType::F_FLOAT_AND_DOUBLE_ONLY:
            fieldTypeStr = "float and double only";

            SetNumberMin(-DBL_MAX);
            SetNumberMax(DBL_MAX);
            break;
        case FieldType::F_PASSWORD:
            fieldTypeStr = "password";
            break;
        }

        isTextDirty = true;

        Log::Print(
            "Set text widget field type to '" + fieldTypeStr + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    f32 Text::GetTextSize() const { return textSize; }
    void Text::SetTextSize(f32 newValue)
    {
        if (newValue == textSize)
        {
            Log::Print(
                "Failed to set text widget '" + to_string(ID) + "' size because it is already the same!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        textSize = clamp(newValue, MIN_TEXT_SIZE, MAX_TEXT_SIZE);

        isTextDirty = true;

        Log::Print(
            "Set text widget '" + to_string(ID) + "' text size to '" + to_string(textSize) + "'!",
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
                LogType::LOG_ERROR,
                2);

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
                LogType::LOG_ERROR,
                2);

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
                LogType::LOG_ERROR,
                2);

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

        bool needsTruncation = text.size() > newValue;
        string removedStr{};

        if (needsTruncation)
        {
            u16 toBeRemoved = scast<u16>(text.size() - newValue);
            RemoveText(toBeRemoved);

            removedStr = 
                " Removed '" + to_string(toBeRemoved) 
                + "' characters because new size is smaller than old total amount of characters.";
        }

        maxCharacters = newValue;

        Log::Print(
            "Set text widget max character count to '" + to_string(maxCharacters) + "'!" + removedStr,
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    f64 Text::GetNumberMin() const { return numberMin; }
    void Text::SetNumberMin(f64 newValue)
    {
        newValue = clamp(newValue, -DBL_MAX, numberMax);

        numberMin = newValue;

        switch (fieldType)
        {
        default: break;
        case FieldType::F_INTEGER_ONLY:
            numberMin = clamp(scast<i64>(numberMin), scast<i64>(INT64_MIN), scast<i64>(numberMax));
            break;
        case FieldType::F_FLOAT_ONLY:
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
        case FieldType::F_INTEGER_ONLY:
            numberMax = clamp(scast<i64>(numberMax), scast<i64>(numberMin), scast<i64>(INT64_MAX));
            break;
        case FieldType::F_FLOAT_ONLY:
            numberMax = clamp(scast<f32>(numberMax), scast<f32>(numberMin), FLT_MAX);
            break;
        }

        Log::Print(
            "Set new text widget '" + to_string(ID) + "' max value to '" + to_string(numberMax) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    string Text::GetText() const
    {
        string result{};

        for (u32 utf : text)
        {
            result += GetValueByUTF(utf);
        }

        return result;
    }
    void Text::AddText(
        string&& newValue,
        bool back)
    {
        vector<u32> convertedText = StringToUTF(std::move(newValue));

        if (convertedText.size() + text.size() > maxCharacters)
        {
            Log::Print(
                "Failed to add characters to text widget '" + to_string(ID) 
                + "' because added character count exceeds max character count!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        string target = back ? "back" : "front";

        if (back)
        {
            //append to back
            text.insert(
                text.end(),
                convertedText.begin(),
                convertedText.end());
        }
        else
        {
            //prepend to front
            text.insert(
                text.begin(),
                convertedText.begin(),
                convertedText.end());
        }

        isTextDirty = true;

        Log::Print(
            "Added new characters to " + target + " of text widget '" + to_string(ID) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }
    void Text::RemoveText(
        u32 count,
        bool back)
    {
        if (count == 0)
        {
            Log::Print(
                "Failed to remove characters from text widget '" + to_string(ID) 
                + "' because removal count was 0!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (count > text.size())
        {
            Log::Print(
                "Failed to remove characters from text widget '" + to_string(ID) 
                + "' because removal count was bigger than total character count!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (back) text.erase(text.end() - count, text.end());
        else      text.erase(text.begin(), text.begin() + count);

        isTextDirty = true;

        string target = back ? "back" : "front";

        Log::Print(
            "Removed '" + to_string(count) + "' characters from text widget '" + to_string(ID) + "' text " + target + "!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }
    void Text::SetText(string&& newValue)
    {   
        vector<u32> convertedText = StringToUTF(std::move(newValue));

        if (convertedText == text)
        {
            Log::Print(
                "Failed to update text widget '" + to_string(ID) 
                + "' characters because they are already the same!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (convertedText.size() > maxCharacters)
        {
            Log::Print(
                "Failed to update text widget '" + to_string(ID) 
                + "' characters because its character count exceeds max character count!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        text = std::move(convertedText);

        isTextDirty = true;

        Log::Print(
            "Overwrote text widget '" + to_string(ID) + "' characters!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    const vector<u32>& Text::GetUTF() const { return text; }
    void Text::AddUTF(
        vector<u32>&& newValue,
        bool back)
    {
        if (newValue.size() + text.size() > maxCharacters)
        {
            Log::Print(
                "Failed to add characters to text widget '" + to_string(ID) 
                + "' because added character count exceeds max character count!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        string target = back ? "back" : "front";

        if (back)
        {
            //append to back
            text.insert(
                text.end(),
                newValue.begin(),
                newValue.end());
        }
        else
        {
            //prepend to front
            text.insert(
                text.begin(),
                newValue.begin(),
                newValue.end());
        }

        isTextDirty = true;

        Log::Print(
            "Added new characters to " + target + " of text widget '" + to_string(ID) + "'!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }
    void Text::SetUTF(vector<u32>&& newValue)
    {
        if (newValue == text)
        {
            Log::Print(
                "Failed to update text widget '" + to_string(ID) 
                + "' characters because they are already the same!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.size() > maxCharacters)
        {
            Log::Print(
                "Failed to update text widget '" + to_string(ID) 
                + "' characters because its character count exceeds max character count!",
                "KG_TEXT",
                LogType::LOG_ERROR,
                2);

            return;
        }

        text = std::move(newValue);

        isTextDirty = true;

        Log::Print(
            "Overwrote text widget '" + to_string(ID) + "' characters!",
            "KG_TEXT",
            LogType::LOG_SUCCESS);
    }

    void Text::Update()
    {
        if (!isTextDirty) return;

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

        if (text.empty())
        {
            Shader* first = Shader::GetRegistry().GetAllContent().front();
            Texture* rootTex{};
            string err = Texture::GetRegistry().GetContent(first->GetRootTextureID(), rootTex);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics text widget error",
                    "Failed to update text widget '" + to_string(ID) + "' because the shader '" 
                    + to_string(first->GetID()) + "' root texture was invalid! Reason: " + err);
            }

            tex->SetSize({ 100.0f });
            tex->SetPixelData( vector<u8>{ rootTex->GetPixelData() });

            scast<Transform2D&>(mesh->GetTransform()).setsize({ 100.0f });
        }
        else
        {
            const i32 ascender = font->GetFontData().ascender;
            const i32 descender = font->GetFontData().descender;

            i32 penX{};
            i32 minX{};
            i32 maxX{};

            //calculate width
            for (u32 utf : text)
            {
                GlyphData& glyphData = font->GetGlyphData(
                    font->GetFontData(),
                    utf);

                i32 glyphWidth = scast<i32>(fabsf(glyphData.size.x));

                i32 glyphLeft = penX + scast<i32>(glyphData.bearing.x);
                i32 glyphRight = glyphLeft + glyphWidth;

                minX = min(minX, glyphLeft);
                maxX = max(maxX, glyphRight);

                penX += glyphData.advance;
            }

            maxX = max(maxX, penX);

            const u32 finalWidth = scast<u32>(maxX - minX);
            const u32 finalHeight = scast<u32>(ascender - descender);

            vector<u8> finalPixels(finalWidth * finalHeight, 0);

            penX = 0;

            //copy glyphs into final texture
            for (u32 utf : text)
            {
                vector<u8> glyphPixelData = font->GetGlyphPixelData(utf);

                GlyphData& glyphData = font->GetGlyphData(
                    font->GetFontData(),
                    utf);

                u32 glyphWidth  = scast<u32>(fabsf(glyphData.size.x));
                u32 glyphHeight = scast<u32>(fabsf(glyphData.size.y));

                i32 glyphBottom = scast<i32>(glyphData.bearing.y + glyphData.size.y);

                i32 glyphX = penX + scast<i32>(glyphData.bearing.x) - minX;
                i32 glyphY = glyphBottom - descender;

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

                        finalPixels[scast<u32>(dstY) * finalWidth + scast<u32>(dstX)]
                            = glyphPixelData[y * glyphWidth + x];
                    }
                }

                penX += glyphData.advance;
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