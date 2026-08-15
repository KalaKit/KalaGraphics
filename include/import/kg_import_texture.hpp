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

    using u8 = uint8_t;
    using u32 = uint32_t;

    static constexpr string_view EXT_PNG = ".png";
    static constexpr string_view EXT_KTEX = ".ktex";

    struct TextureData
    {

    };

    class LIB_API ImportTexture
    {
    friend struct default_delete<ImportTexture>;
    public:
        static KalaGraphicsRegistry<ImportTexture>& GetRegistry();

        static ImportTexture* Initialize(path&& texturePath);

        u32 GetID() const;

        const path& GetTexturePath() const;
        const TextureData& GetTextureData() const;

        void Destroy();
    private:
        ~ImportTexture();

        static string Init_PNG(
            vector<u8>&& binaryData,
            TextureData& outTextureData);
        static string Init_KTEX(
            vector<u8>&& binaryData,
            TextureData& outTextureData);

        u32 ID{};

        path texturePath{};
        TextureData textureData{};
    };
}