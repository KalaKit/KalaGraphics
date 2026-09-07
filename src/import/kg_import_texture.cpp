//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "lodepng.h"

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "import/kg_import_texture.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaHeaders::KalaMath::vec2;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Import::ImportTextureData;
using KalaGraphics::Graphics::TexturePixelFormat;

using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::unique_ptr;
using std::make_unique;
using std::filesystem::path;
    
static constexpr string_view EXT_PNG = ".png";

static string Init_PNG(
    vector<u8>&& pixelData,
    ImportTextureData& outData);

namespace KalaGraphics::Import
{
    static KalaGraphicsRegistry<ImportTexture> registry{};

    KalaGraphicsRegistry<ImportTexture>& ImportTexture::GetRegistry() { return registry; }

    ImportTexture* ImportTexture::Initialize(path&& texturePath)
    {
        if (!exists(texturePath))
        {
            Log::Print(
                "Failed to import texture '" + texturePath.string() + "' because it was not found!",
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (!is_regular_file(texturePath))
        {
            Log::Print(
                "Failed to import texture '" + texturePath.string() + "' because it is not a regular file!",
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        string ext = texturePath.extension().string();
        if (ext != EXT_PNG)
        {
            Log::Print(
                "Failed to import texture '" + texturePath.string() + "' because its extension is not supported!",
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        vector<u8> imageData{};
        string errMsg = ReadBinaryDataFromFile(
            texturePath,
            imageData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import texture '" + texturePath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        ImportTextureData textureData{};
        if (ext == EXT_PNG)
        {
            errMsg = Init_PNG(
                std::move(imageData),
                textureData);
        }

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import texture '" + texturePath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<ImportTexture> newTex = make_unique<ImportTexture>();
        ImportTexture* texPtr = newTex.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        texPtr->ID = newID;
        texPtr->texturePath = std::move(texturePath);
        texPtr->textureData = std::move(textureData);

        string err = registry.AddContent(newID, std::move(newTex));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics import texture error",
				"Failed to initialize import texture! Reason: " + err);
        }

        Log::Print(
			"Created new import texture '" + to_string(newID) 
            + "' from path '" + texPtr->texturePath.string() + "'!",
			"KG_IMPORT_TEXTURE",
			LogType::LOG_SUCCESS);

        return texPtr;
    }

    ImportTexture* ImportTexture::Initialize(vector<u8>&& imageData)
    {
        if (imageData.empty())
        {
            Log::Print(
                "Failed to import texture from image data because no image data was provided!",
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        ImportTextureData textureData{};
        string errMsg = Init_PNG(
            std::move(imageData),
            textureData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import texture from image data! Reason: " + errMsg,
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<ImportTexture> newTex = make_unique<ImportTexture>();
        ImportTexture* texPtr = newTex.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        texPtr->ID = newID;
        texPtr->textureData = std::move(textureData);

        string err = registry.AddContent(newID, std::move(newTex));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics import texture error",
				"Failed to initialize import texture! Reason: " + err);
        }

        Log::Print(
			"Created new import texture '" + to_string(newID) + "' from binary data!",
			"KG_IMPORT_TEXTURE",
			LogType::LOG_SUCCESS);

        return texPtr;
    }

    u32 ImportTexture::GetID() const { return ID; }

    const path& ImportTexture::GetTexturePath() const { return texturePath; }
    const ImportTextureData& ImportTexture::GetTextureData() const { return textureData; }

    void ImportTexture::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics import texture error",
                "Failed to destroy import texture '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    ImportTexture::~ImportTexture()
    {
        Log::Print(
            "Destroying import texture data '" + to_string(ID) + "'.",
            "KG_IMPORT_TEXTURE",
            LogType::LOG_INFO);
    }
}

string Init_PNG(
    vector<u8>&& imageData,
    ImportTextureData& outData)
{
    u32 width{}, height{};

    lodepng::State state{};

    //force bit depth to 8
    state.info_raw.bitdepth = 8;

    unsigned error = lodepng_inspect(
        &width,
        &height,
        &state,
        imageData.data(),
        imageData.size());
    
    if (error) return "Failed to inspect PNG! Reason: " + string(lodepng_error_text(error));

    LodePNGColorType colorType = state.info_png.color.colortype;
    bool isSRGB = state.info_png.srgb_defined != 0;

    switch (colorType)
    {
        default:
        case LCT_GREY:
        {
            state.info_raw.colortype = LCT_GREY;
            outData.pixelFormat = TexturePixelFormat::FORMAT_BASIC_R8;

            break;
        }
        case LCT_GREY_ALPHA:
        {
            state.info_raw.colortype = LCT_GREY_ALPHA;
            outData.pixelFormat = TexturePixelFormat::FORMAT_BASIC_R8G8;

            break;
        }
        case LCT_RGB:
        {
            state.info_raw.colortype = LCT_RGBA;
            outData.pixelFormat = isSRGB
                ? TexturePixelFormat::FORMAT_SRGB_R8G8B8A8
                : TexturePixelFormat::FORMAT_BASIC_R8G8B8A8;

            break;
        }
        case LCT_RGBA:
        {
            state.info_raw.colortype = LCT_RGBA;
            outData.pixelFormat = isSRGB
                ? TexturePixelFormat::FORMAT_SRGB_R8G8B8A8
                : TexturePixelFormat::FORMAT_BASIC_R8G8B8A8;

            break;
        }
        case LCT_PALETTE:
        {
            state.info_raw.colortype = LCT_RGBA;

            outData.pixelFormat = isSRGB
                ? TexturePixelFormat::FORMAT_SRGB_R8G8B8A8
                : TexturePixelFormat::FORMAT_BASIC_R8G8B8A8;

            break;
        }
    }

    error = lodepng::decode(
        outData.pixelData,
        width,
        height,
        state,
        imageData);

    if (error) return "Failed to decode PNG! Reason: " + string(lodepng_error_text(error));

    outData.size = 
    {
        scast<f32>(width),
        scast<f32>(height)
    };

    return "";
}