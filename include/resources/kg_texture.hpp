//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <array>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

struct VkBuffer_T;
using VkBuffer = VkBuffer_T*;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

struct VkDescriptorSet_T;
using VkDescriptorSet = VkDescriptorSet_T*;

struct VkSampler_T;
using VkSampler = VkSampler_T*;

struct VkImage_T;
using VkImage = VkImage_T*;

struct VkImageView_T;
using VkImageView = VkImageView_T*;

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;

namespace KalaGraphics::Resources
{
    using KalaHeaders::KalaMath::vec2;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::array;
    using std::default_delete;

    //Pixel data for a fallback 16x16 sized checkerboard texture with pink and black tiles
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
        FORMAT_HDR_R32G32B32A32_FLOAT = 13  //4 channels, 32-bit float
    };

    enum class TextureType : u8
    {
        TYPE_2D            = 0, //single, flat image, layer count always 1
        TYPE_2D_ARRAY      = 1, //N independent 2D layers
        TYPE_CUBEMAP       = 2, //always 6 layers, one per cube face
        TYPE_CUBEMAP_ARRAY = 3, //6 layers per cubemap, one per cube face
        TYPE_3D            = 4  //volumetric, layerCount = depth
    };

    enum class TextureFilterMode : u8
    {
        FILTER_LINEAR = 0, //standard blended filter mode, good for realistic textures
        FILTER_NEAREST = 1 //unfiltered texture, good for pixel games
    };

    enum class TextureShadowMapMode : u8
    {
        //comparison always passes,
        //default if not using shadow mapping
        MODE_ALWAYS = 0,

        //standard shadow map comparison,
        //passes if the fragments depth is less
        //than the stored shadow map depth
        MODE_LESS = 1,
        //same as less, but also passes on exact equality,
        //sometimes used to avoid self-shadowing artifacts
        //at the exact depth boundary
        MODE_LESS_OR_EQUAL = 2
    };

    enum class TextureWrapMode : u8
    {
        WRAP_REPEAT = 0,
        WRAP_MIRRORED_REPEAT = 1,
        WRAP_CLAMP_TO_EDGE = 2,
        WRAP_CLAMP_TO_BORDER = 3
    };

    enum class TextureBorderColor : u8
    {
        COLOR_TRANSPARENT_BLACK = 0,
        COLOR_OPAQUE_BLACK = 1,
        COLOR_OPAQUE_WHITE = 2
    };

    //Defaults to 1x1 white pixel unless overwritten
    struct TextureData
    {
        //default pixel data is always a 1x1 white pixel
        vector<u8> pixelData = 
        { 
            0xFF, 
            0xFF, 
            0xFF, 
            0xFF
        };

        PixelFormat format = PixelFormat::FORMAT_BASIC_R8G8B8A8;
        TextureType type = TextureType::TYPE_2D;
        TextureFilterMode filterMode = TextureFilterMode::FILTER_LINEAR;
        TextureShadowMapMode shadowMode = TextureShadowMapMode::MODE_ALWAYS;
        TextureWrapMode wrapMode = TextureWrapMode::WRAP_REPEAT;
        //only used if wrap mode is WRAP_CLAMP_TO_BORDER
        TextureBorderColor borderColor = TextureBorderColor::COLOR_OPAQUE_BLACK;

        //disabled by default, quality improvement for textures in 3D spaces when viewed at steep angles,
        //should not be used for UI textures or if using FILTER_NEAREST
        bool useAnisotropy{};

        vec2 size = 1;       //width and height
        u16 depth = 1;       //only used for TextureType::TYPE_3D
        u32 layerCount = 1;  //only changed if not using TextureType::TYPE_2D
        u32 mipMapCount = 1; //how many downsampled textures are allowed
    };

    class LIB_API Texture
    {
    friend class Shader;
    friend class Mesh;
    friend struct default_delete<Texture>;
    public:
        static KalaGraphicsRegistry<Texture>& GetRegistry();

        //Either initialize a blank white 1x1 texture, or pass data via textureData
        static Texture* Initialize(
            u32 shaderID,
            TextureData&& textureData = {});

        u32 GetID() const;

        u32 GetShaderID() const;
        void SetShaderID(u32 newID);

        const vector<u32>& GetMeshIDs() const;

        const vector<u8>& GetPixelData() const;
        void SetPixelData(vector<u8>&& newPixelData);

        PixelFormat GetPixelFormat() const;
        void SetPixelFormat(PixelFormat newFormat);

        TextureType GetTextureType() const;
        void SetTextureType(TextureType newType);

        TextureFilterMode GetFilterMode() const;
        void SetFilterMode(TextureFilterMode newFilter);

        TextureShadowMapMode GetShadowMapMode() const;
        void SetShadowMapMode(TextureShadowMapMode newMode);

        TextureWrapMode GetWrapMode() const;
        void SetWrapMode(TextureWrapMode newWrap);

        TextureBorderColor GetBorderColor() const;
        void SetBorderColor(TextureBorderColor newColor);

        bool IsAnisotropyEnabled() const;
        void SetAnisotropyState(bool newValue);

        vec2 GetSize() const;
        void SetSize(vec2 newSize);

        u32 GetDepth() const;
        void SetDepth(u32 newDepth);

        u32 GetLayerCount() const;
        void SetLayerCount(u32 newCount);

        u8 GetMipMapCount() const;
        void SetMipMapCount(u8 newCount);

        //Should be called after updating texture data
        void UpdateTextureData();

        void Destroy();
    private:
        ~Texture();

        void UploadPixelData(VkCommandBuffer vkCommandBuffer);
        void GenerateMipMaps(VkCommandBuffer vkCommandBuffer);

        u32 ID{};
        u32 shaderID{};

        //meshes that contain this texture
        vector<u32> meshIDs{};

        //set to true if any texture-breaking data was adjusted
        bool isDirty{};

        vector<u8> pixelData{};

        PixelFormat format{};
        TextureType type{};
        TextureFilterMode filterMode{};
        TextureShadowMapMode shadowMode{};
        TextureWrapMode wrapMode{};
        TextureBorderColor borderColor{};

        bool useAnisotropy{};

        vec2 size{};
        u32 depth{};
        u32 layerCount{};
        u8 mipMapCount{};

        VkBuffer vkTexBuffer{};
        u64 pixelDataSize{};
        VmaAllocation vmaTexAllocation{};
        void* texMappedPtr{};

        VkSampler vkSampler{};
        VkImage vkImage{};
        VkImageView vkImageView{};
        VmaAllocation vmaImageAllocation{};

        VkDescriptorSet vkDescriptorSet{};
    };
}