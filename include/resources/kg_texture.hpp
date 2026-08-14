//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Resources
{
    using KalaHeaders::KalaMath::vec2;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::default_delete;

    using u8 = uint8_t;
    using u32 = uint32_t;

    enum class PixelFormat : u8
    {
        FORMAT_BASIC_R8               = 0,  //1 channel,  8-bit UNORM
        FORMAT_BASIC_R8G8             = 1,  //2 channels, 8-bit UNORM
        FORMAT_BASIC_R8G8B8           = 2,  //3 channels, 8-bit UNORM
        FORMAT_BASIC_R8G8B8A8         = 3,  //4 channels, 8-bit UNORM

        FORMAT_SRGB_R8G8B8            = 4,  //3 channels, 8-bit sRGB-encoded
        FORMAT_SRGB_R8G8B8A8          = 5,  //4 channels, 8-bit sRGB-encoded

        FORMAT_HDR_R16_FLOAT          = 6,  //1 channel,  16-bit float
        FORMAT_HDR_R16G16_FLOAT       = 7,  //2 channels, 16-bit float
        FORMAT_HDR_R16G16B16_FLOAT    = 8,  //3 channels, 16-bit float
        FORMAT_HDR_R16G16B16A16_FLOAT = 9,  //4 channels, 16-bit float
        FORMAT_HDR_R32_FLOAT          = 10, //1 channel,  32-bit float
        FORMAT_HDR_R32G32_FLOAT       = 11, //2 channels, 32-bit float
        FORMAT_HDR_R32G32B32_FLOAT    = 12, //3 channels, 32-bit float
        FORMAT_HDR_R32G32B32A32_FLOAT = 13, //4 channels, 32-bit float

        FORMAT_COUNT                  = 14
    };

    enum class TextureType : u8
    {
        TYPE_2D       = 0, //single, flat image, layer count always 1
        TYPE_2D_ARRAY = 1, //N independent 2D layers
        TYPE_CUBEMAP  = 2, //always 6 layers, one per cube face
        TYPE_3D       = 3, //volumetric, layerCount = depth

        TYPE_COUNT    = 4
    };

    struct TextureData
    {
        vector<u8> pixelData{};

        vec2 size{};
        PixelFormat format{};
        TextureType type{};
        u8 layerCount{};
    };

    class LIB_API Texture
    {
    friend struct default_delete<Texture>;
    public:
        static KalaGraphicsRegistry<Texture>& GetRegistry();

        static Texture* Initialize(TextureData&& textureData);

        u32 GetID() const;

        vector<u8>& GetPixelData();

        vec2 GetSize() const;
        PixelFormat GetPixelFormat() const;
        TextureType GetTextureType() const;
        u8 GetLayerCount() const;

        //Should be called after updating any texture data
        void UpdateTextureData();

        void Destroy();
    private:
        ~Texture();

        u32 ID{};

        vector<u8> pixelData{};

        vec2 size{};
        PixelFormat format{};
        TextureType type{};
        u8 layerCount{};
    };
}