//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "import/kg_import_texture.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaGraphics::Core::KalaGraphicsCore;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Import
{
    static KalaGraphicsRegistry<ImportTexture> registry{};

    KalaGraphicsRegistry<ImportTexture>& ImportTexture::GetRegistry() { return registry; }

    ImportTexture* ImportTexture::Initialize(path&& texturePath)
    {
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
        if (ext != EXT_PNG
            && ext != EXT_KTEX)
        {
            Log::Print(
                "Failed to import texture '" + texturePath.string() + "' because its extension is not supported!",
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        vector<u8> outData{};
        string errMsg = ReadBinaryDataFromFile(
            texturePath,
            outData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import texture '" + texturePath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        TextureData textureData{};
        if (ext == ".png")
        {
            errMsg = Init_PNG(
                std::move(outData),
                textureData);
        }
        else
        {
            errMsg = Init_KTEX(
                std::move(outData),
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
			"Created new import texture '" + to_string(newID) + "'!",
			"KG_IMPORT_TEXTURE",
			LogType::LOG_SUCCESS);

        return texPtr;
    }

    u32 ImportTexture::GetID() const { return ID; }

    const path& ImportTexture::GetTexturePath() const { return texturePath; }
    const TextureData& ImportTexture::GetTextureData() const { return textureData; }

    string ImportTexture::Init_PNG(
        vector<u8>&& binaryData,
        TextureData& outTextureData)
    {
        return "init png";
    }

    string ImportTexture::Init_KTEX(
        vector<u8>&& binaryData,
        TextureData& outTextureData)
    {
        return "init ktex";
    }

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