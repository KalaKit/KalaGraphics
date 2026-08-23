//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <vector>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Import
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::filesystem::path;
    using std::vector;
    using std::string;
    using std::string_view;
    using std::default_delete;
    
    static constexpr string_view EXT_PNG = ".png";
    static constexpr string_view EXT_KTEX = ".ktex";

    struct TextureData
    {

    };

    class LIB_API ImportTexture
    {
    friend struct default_delete<ImportTexture>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<ImportTexture>& GetRegistry();

        KNODISCARD
		static ImportTexture* Initialize(path&& texturePath);

        KNODISCARD
		u32 GetID() const;

        KNODISCARD
		const path& GetTexturePath() const;
        KNODISCARD
		const TextureData& GetTextureData() const;

        void Destroy();
    private:
        ~ImportTexture();

        KNODISCARD
		static string Init_PNG(
            vector<u8>&& binaryData,
            TextureData& outTextureData);
        KNODISCARD
		static string Init_KTEX(
            vector<u8>&& binaryData,
            TextureData& outTextureData);

        u32 ID{};

        path texturePath{};
        TextureData textureData{};
    };
}