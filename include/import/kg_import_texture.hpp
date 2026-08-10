//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <vector>
#include <array>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Import
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::filesystem::path;
    using std::vector;
    using std::array;
    using std::string;
    using std::string_view;

    using u8 = uint8_t;
    using u32 = uint32_t;

    static constexpr string_view EXT_PNG = ".png";
    static constexpr string_view EXT_KTEX = ".ktex";

    static constexpr array<u8, 16 * 16 * 4> FALLBACK_TEXTURE = []
    {
        array<u8, 16 * 16 * 4> data{};
    
        constexpr u8 pink[4] = { 255, 0, 255, 255 };
        constexpr u8 black[4] = { 0, 0, 0, 255 };

        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                //2x2 tile index: flips every 2 pixels in each axis
                bool tileParity = ((x / 2) + (y / 2)) % 2 == 0;
                const u8* color = tileParity ? pink : black;

                int pixelIndex = (y * 16 + x) * 4;
                data[pixelIndex + 0] = color[0];
                data[pixelIndex + 1] = color[1];
                data[pixelIndex + 2] = color[2];
                data[pixelIndex + 3] = color[3];
            }
        }

        return data;
    }();

    struct TextureData
    {

    };

    class LIB_API ImportTexture
    {
    public:
        static KalaGraphicsRegistry<ImportTexture>& GetRegistry();

        static ImportTexture* Initialize(path&& texturePath);

        u32 GetID() const;

        const path& GetTexturePath() const;
        const TextureData& GetTextureData() const;

        void Destroy();

        ~ImportTexture();
    private:
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