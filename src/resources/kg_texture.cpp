//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "core/kg_core.hpp"

#include "vulkan/vulkan_core.h"
KG_VK_MEM_ALLOC_IGNORE_PUSH
#include "vma/vk_mem_alloc.h"
KG_VK_MEM_ALLOC_IGNORE_POP

#include "log_utils.hpp"

#include "resources/kg_texture.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_mesh.hpp"
#include "core/kg_context.hpp"
#include "core/kg_viewport.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::Viewport;
using KalaGraphics::Resources::PixelFormat;
using KalaGraphics::Resources::TextureType;
using KalaGraphics::Resources::TextureFilterMode;
using KalaGraphics::Resources::TextureShadowMapMode;
using KalaGraphics::Resources::TextureWrapMode;
using KalaGraphics::Resources::TextureBorderColor;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

static bool retreivedProps{};
static VkPhysicalDeviceProperties props{};

static VkFormat ToVkFormat(PixelFormat pf)
{
    switch (pf)
    {
    default:
    case PixelFormat::FORMAT_BASIC_R8:
        return VkFormat::VK_FORMAT_R8_UNORM;
    case PixelFormat::FORMAT_BASIC_R8G8:
        return VkFormat::VK_FORMAT_R8G8_UNORM;
    case PixelFormat::FORMAT_BASIC_R8G8B8:
        return VkFormat::VK_FORMAT_R8G8B8_UNORM;
    case PixelFormat::FORMAT_BASIC_R8G8B8A8:
        return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;

    case PixelFormat::FORMAT_SRGB_R8G8B8:
        return VkFormat::VK_FORMAT_R8G8B8_SRGB;
    case PixelFormat::FORMAT_SRGB_R8G8B8A8:
        return VkFormat::VK_FORMAT_R8G8B8A8_SRGB;

    case PixelFormat::FORMAT_HDR_R16_FLOAT:
        return VkFormat::VK_FORMAT_R16_SFLOAT;
    case PixelFormat::FORMAT_HDR_R16G16_FLOAT:
        return VkFormat::VK_FORMAT_R16G16_SFLOAT;
    case PixelFormat::FORMAT_HDR_R16G16B16_FLOAT:
        return VkFormat::VK_FORMAT_R16G16B16_SFLOAT;
    case PixelFormat::FORMAT_HDR_R16G16B16A16_FLOAT:
        return VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
    case PixelFormat::FORMAT_HDR_R32_FLOAT:
        return VkFormat::VK_FORMAT_R32_SFLOAT;
    case PixelFormat::FORMAT_HDR_R32G32_FLOAT:
        return VkFormat::VK_FORMAT_R32G32_SFLOAT;
    case PixelFormat::FORMAT_HDR_R32G32B32_FLOAT:
        return VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
    case PixelFormat::FORMAT_HDR_R32G32B32A32_FLOAT:
        return VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;
    };
}

static VkImageType ToVkImageType(TextureType tt)
{
    switch (tt)
    {
    default:
    case TextureType::TYPE_2D:
    case TextureType::TYPE_2D_ARRAY:
    case TextureType::TYPE_CUBEMAP:
    case TextureType::TYPE_CUBEMAP_ARRAY:
        return VkImageType::VK_IMAGE_TYPE_2D;
    case TextureType::TYPE_3D:
        return VkImageType::VK_IMAGE_TYPE_3D;
    }
}

static VkImageViewType ToVkImageViewType(TextureType tt)
{
    switch (tt)
    {
    default:
    case TextureType::TYPE_2D:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
    case TextureType::TYPE_2D_ARRAY:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case TextureType::TYPE_CUBEMAP:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE;
    case TextureType::TYPE_CUBEMAP_ARRAY:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case TextureType::TYPE_3D:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
    }
}

static VkFilter ToVkFilter(TextureFilterMode tf)
{
    return tf == TextureFilterMode::FILTER_LINEAR
        ? VkFilter::VK_FILTER_LINEAR
        : VkFilter::VK_FILTER_NEAREST;
}

static VkSamplerMipmapMode ToVkMipMapMode(TextureFilterMode tf)
{
    return tf == TextureFilterMode::FILTER_LINEAR
        ? VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR
        : VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

static VkCompareOp ToVkCompareOp(TextureShadowMapMode ts)
{
    switch (ts)
    {
    default:
    case TextureShadowMapMode::MODE_ALWAYS:
        return VkCompareOp::VK_COMPARE_OP_ALWAYS;
    case TextureShadowMapMode::MODE_LESS:
        return VkCompareOp::VK_COMPARE_OP_LESS;
    case TextureShadowMapMode::MODE_LESS_OR_EQUAL:
        return VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
    }
}

static VkSamplerAddressMode ToVkSamplerAddressMode(TextureWrapMode tw)
{
    switch (tw)
    {
    default:
    case TextureWrapMode::WRAP_REPEAT:
        return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case TextureWrapMode::WRAP_MIRRORED_REPEAT:
        return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case TextureWrapMode::WRAP_CLAMP_TO_EDGE:
        return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case TextureWrapMode::WRAP_CLAMP_TO_BORDER:
        return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
}

static VkBorderColor ToVkBorderColor(TextureBorderColor tc)
{
    switch (tc)
    {
    default:
    case TextureBorderColor::COLOR_TRANSPARENT_BLACK:
        return VkBorderColor::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case TextureBorderColor::COLOR_OPAQUE_BLACK:
        return VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case TextureBorderColor::COLOR_OPAQUE_WHITE:
        return VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
}

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Texture> registry{};

    KalaGraphicsRegistry<Texture>& Texture::GetRegistry() { return registry; }

    Texture* Texture::Initialize(
        u32 shaderID,
        TextureData&& textureData)
    {
        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            Log::Print(
                "Failed to create texture because the shader was invalid! Reason: " + err,
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        //TODO: figure out a better solution
        if (shader->descriptorSetLayouts.empty())
        {
            Log::Print(
                "Failed to create texture because the shader '" 
                + to_string(shaderID) + "' had no shader data!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (textureData.pixelData.empty())
        {
            Log::Print(
                "Failed to create texture because pixel data was empty!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Texture> newTexture = make_unique<Texture>();
        Texture* texPtr = newTexture.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        texPtr->ID = newID;
        texPtr->shaderID = shaderID;

        //shader references this texture
        shader->textureIDs.push_back(newID);

        texPtr->SetPixelData(std::move(textureData.pixelData));

        texPtr->SetPixelFormat(textureData.format);
        texPtr->SetTextureType(textureData.type);
        texPtr->SetFilterMode(textureData.filterMode);
        texPtr->SetShadowMapMode(textureData.shadowMode);
        texPtr->SetWrapMode(textureData.wrapMode);
        texPtr->SetBorderColor(textureData.borderColor);

        texPtr->SetAnisotropyState(textureData.useAnisotropy);

        texPtr->SetSize(textureData.size);
        texPtr->SetDepth(textureData.depth);
        texPtr->SetLayerCount(textureData.layerCount);
        texPtr->SetMipMapCount(textureData.mipMapCount);

        texPtr->isDirty = true;
        texPtr->UpdateTextureData();

        err = registry.AddContent(newID, std::move(newTexture));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics texture error",
				"Failed to initialize texture! Reason: " + err);
        }

        Log::Print(
			"Created new texture '" + to_string(newID) 
            + "' for shader '" + to_string(shaderID) + "'!",
			"KG_TEXTURE",
			LogType::LOG_SUCCESS);

        return texPtr;
    }

    u32 Texture::GetID() const { return ID; }

    u32 Texture::GetShaderID() const { return shaderID; }
    void Texture::SetShaderID(u32 newValue)
    {
        if (shaderID == newValue)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) 
                + "' shader ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Shader* oldShader{};
        string err = Shader::GetRegistry().GetContent(shaderID, oldShader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to set shader ID for texture '" 
                + to_string(ID) + "' because of invalid old shader! Reason: " + err);
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(newValue, shader);
        if (!err.empty())
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) 
                + "' shader ID because it was invalid! Reason: " + err,
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        shaderID = newValue;

        if (oldShader)
        {
            erase(
                oldShader->textureIDs,
                ID);
        }
        shader->textureIDs.push_back(ID);

        //detach all meshes
        for (u32 mID : meshIDs)
        {
            Mesh* m{};
            string err = Mesh::GetRegistry().GetContent(mID, m);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics texture error",
                    "Failed to set texture '" + to_string(ID) 
                    + "' shader ID because the texture's mesh was invalid! Reason: " + err);
            }

            m->textureID = 0;

            Log::Print(
                "Mesh '" + to_string(mID) + "' texture '" + to_string(ID) 
                + "' was detached because the texture's shader was changed.",
                "KG_TEXTURE",
                LogType::LOG_WARNING);
        }
        meshIDs.clear();

        Log::Print(
            "Set texture '" + to_string(ID) 
            + "' shader ID to '" + to_string(shaderID) + "'!",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    const vector<u32>& Texture::GetMeshIDs() const { return meshIDs; }

    const vector<u8>& Texture::GetPixelData() const { return pixelData; }
    void Texture::SetPixelData(vector<u8>&& newPixelData)
    {
        //ignore if already the same
        if (pixelData == newPixelData) return;

        if (newPixelData.empty())
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) 
                + "' pixel data because it cannot be empty!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        pixelData = std::move(newPixelData);
    }

    PixelFormat Texture::GetPixelFormat() const { return format; }
    void Texture::SetPixelFormat(PixelFormat newFormat)
    {
        //ignore if already the same
        if (newFormat == format) return;

        format = newFormat;

        isDirty = true;

        string texFormat{};
        switch (format)
        {
        default:
        case PixelFormat::FORMAT_BASIC_R8:
            texFormat = "R8";
            break;
        case PixelFormat::FORMAT_BASIC_R8G8:
            texFormat = "r8g8";
            break;
        case PixelFormat::FORMAT_BASIC_R8G8B8:
            texFormat = "r8g8b8";
            break;
        case PixelFormat::FORMAT_BASIC_R8G8B8A8:
            texFormat = "r8g8b8a8";
            break;

        case PixelFormat::FORMAT_SRGB_R8G8B8:
            texFormat = "srgb_r8g8b8";
            break;
        case PixelFormat::FORMAT_SRGB_R8G8B8A8:
            texFormat = "srgb_r8g8b8a8";
            break;

        case PixelFormat::FORMAT_HDR_R16_FLOAT:
            texFormat = "hdr_r16";
            break;
        case PixelFormat::FORMAT_HDR_R16G16_FLOAT:
            texFormat = "hdr_r16g16";
            break;
        case PixelFormat::FORMAT_HDR_R16G16B16_FLOAT:
            texFormat = "hdr_r16g6b16";
            break;
        case PixelFormat::FORMAT_HDR_R16G16B16A16_FLOAT:
            texFormat = "hdr_r16g16b16a16";
            break;
        case PixelFormat::FORMAT_HDR_R32_FLOAT:
            texFormat = "hdr_r32";
            break;
        case PixelFormat::FORMAT_HDR_R32G32_FLOAT:
            texFormat = "hdr_r32g32";
            break;
        case PixelFormat::FORMAT_HDR_R32G32B32_FLOAT:
            texFormat = "hdr_r32g32b32";
            break;
        case PixelFormat::FORMAT_HDR_R32G32B32A32_FLOAT:
            texFormat = "hdr_r32g32b32a32";
            break;
        }

        Log::Print(
            "Set texture '" + to_string(ID) + "' format to '" + texFormat + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    TextureType Texture::GetTextureType() const { return type; }
    void Texture::SetTextureType(TextureType newType)
    {
        //ignore if already the same
        if (newType == type) return;

        if ((newType == TextureType::TYPE_CUBEMAP
            || newType == TextureType::TYPE_CUBEMAP_ARRAY)
            && size.x != size.y)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' type to "
                "'cubemap' or 'cubemap array' because its width and height are not equal!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        type = newType;

        isDirty = true;

        string texType{};
        switch (type)
        {
        default:
        case TextureType::TYPE_2D:
            texType = "2D";
            break;
        case TextureType::TYPE_2D_ARRAY:
            texType = "2D array";
            break;
        case TextureType::TYPE_CUBEMAP:
            texType = "cubemap";
            break;
            case TextureType::TYPE_CUBEMAP_ARRAY:
            texType = "cubemap array";
            break;
        case TextureType::TYPE_3D:
            texType = "3D";
            break;
        }

        if ((type == TextureType::TYPE_2D
            || type == TextureType::TYPE_3D)
            && layerCount != 1)
        {
            layerCount = 1;

            Log::Print(
                "Updated texture '" + to_string(ID) + "' layer count to '1' "
                "because the texture type '" + texType + "' requires a layer count of '1'.",
                "KG_TEXTURE",
                LogType::LOG_INFO);
        }
        if (type == TextureType::TYPE_CUBEMAP
            && layerCount != 6)
        {
            layerCount = 6;

            Log::Print(
                "Updated texture '" + to_string(ID) + "' layer count to '6' "
                "because the texture type '" + texType + "' requires a layer count of '6'.",
                "KG_TEXTURE",
                LogType::LOG_INFO);
        }
        if (type == TextureType::TYPE_CUBEMAP_ARRAY
            && layerCount % 6 != 0)
        {
            layerCount = 6;

            Log::Print(
                "Updated texture '" + to_string(ID) + "' layer count to '6' "
                "because the texture type '" + texType + "' layer count must be a multiple of '6'.",
                "KG_TEXTURE",
                LogType::LOG_INFO);
        }
        if (type == TextureType::TYPE_2D_ARRAY
            && layerCount != 1)
        {
            layerCount = 1;

            Log::Print(
                "Updated texture '" + to_string(ID) + "' layer count to '1' "
                "because the texture type '" + texType + "' default should be '1'.",
                "KG_TEXTURE",
                LogType::LOG_INFO);
        }

        Log::Print(
            "Set texture '" + to_string(ID) + "' type to '" + texType + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    TextureFilterMode Texture::GetFilterMode() const { return filterMode; }
    void Texture::SetFilterMode(TextureFilterMode newFilter)
    {
        //ignore if already the same
        if (filterMode == newFilter) return;

        filterMode = newFilter;

        string fmode{};
        switch (newFilter)
        {
        default:
        case TextureFilterMode::FILTER_LINEAR:
            fmode = "linear";
            break;
        case TextureFilterMode::FILTER_NEAREST:
            fmode = "nearest";
            break;
        }

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) + "' filter mode to '" + fmode + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    TextureShadowMapMode Texture::GetShadowMapMode() const { return shadowMode; }
    void Texture::SetShadowMapMode(TextureShadowMapMode newMode)
    {
        //ignore if already the same
        if (shadowMode == newMode) return;

        shadowMode = newMode;

        string smode{};
        switch (newMode)
        {
        default:
        case TextureShadowMapMode::MODE_ALWAYS:
            smode = "always";
            break;
        case TextureShadowMapMode::MODE_LESS:
            smode = "less";
            break;
        case TextureShadowMapMode::MODE_LESS_OR_EQUAL:
            smode = "less or equal";
            break;
        }

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) + "' shadow map mode to '" + smode + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    TextureWrapMode Texture::GetWrapMode() const { return wrapMode; }
    void Texture::SetWrapMode(TextureWrapMode newMode)
    {
        //ignore if already the same
        if (wrapMode == newMode) return;

        wrapMode = newMode;

        string wmode{};
        switch (newMode)
        {
        default:
        case TextureWrapMode::WRAP_REPEAT:
            wmode = "repeat";
            break;
        case TextureWrapMode::WRAP_MIRRORED_REPEAT:
            wmode = "mirrored repeat";
            break;
        case TextureWrapMode::WRAP_CLAMP_TO_EDGE:
            wmode = "clamp to edge";
            break;
        case TextureWrapMode::WRAP_CLAMP_TO_BORDER:
            wmode = "clamp to border";
            break;
        }

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) + "' wrap map mode to '" + wmode + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    TextureBorderColor Texture::GetBorderColor() const { return borderColor; }
    void Texture::SetBorderColor(TextureBorderColor newColor)
    {
        //ignore if already the same
        if (borderColor == newColor) return;

        borderColor = newColor;

        string bcol{};
        switch (newColor)
        {
        default:
        case TextureBorderColor::COLOR_TRANSPARENT_BLACK:
            bcol = "transparent black";
            break;
        case TextureBorderColor::COLOR_OPAQUE_BLACK:
            bcol = "opaque black";
            break;
        case TextureBorderColor::COLOR_OPAQUE_WHITE:
            bcol = "opaque white";
            break;
        }

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) + "' border color to '" + bcol + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    bool Texture::IsAnisotropyEnabled() const { return useAnisotropy; }
    void Texture::SetAnisotropyState(bool newValue)
    {
        //ignore if already the same
        if (useAnisotropy == newValue) return;

        useAnisotropy = newValue;

        isDirty = true;

        string state = useAnisotropy ? "true" : "false";

        Log::Print(
            "Set texture '" + to_string(ID) + "' anisotropy state to '" + state + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    vec2 Texture::GetSize() const { return size; }
    void Texture::SetSize(vec2 newSize)
    {
        //ignore if already the same
        if (size == newSize) return;

        if (newSize.x < 1
            || newSize.y < 1)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) 
                + "' size because it cannot be less than 1x1!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        u32 maxDimension{};

        if (!retreivedProps)
        {
            vkGetPhysicalDeviceProperties(
                GraphicsContext::GetPhysicalDevice(),
                &props);

            retreivedProps = true;
        }

        switch (type)
        {
        default:
        case TextureType::TYPE_2D:
        case TextureType::TYPE_2D_ARRAY:
            maxDimension = props.limits.maxImageDimension2D;
            break;
        case TextureType::TYPE_3D:
            maxDimension = props.limits.maxImageDimension3D;
            break;
        case TextureType::TYPE_CUBEMAP:
        case TextureType::TYPE_CUBEMAP_ARRAY:
            maxDimension = props.limits.maxImageDimensionCube;
            break;
        }

        if (newSize.x > maxDimension
            || newSize.y > maxDimension)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' size "
                "because its width or height exceeded max allowed value '" + to_string(maxDimension) + "'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if ((type == TextureType::TYPE_CUBEMAP
            || type == TextureType::TYPE_CUBEMAP_ARRAY)
            && newSize.x != newSize.y)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' size "
                "because its width and height are not equal while using a 'cubemap' or 'cubemap array' texture type!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        size = newSize;

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) 
            + "' size to '" + to_string(size.x) + ", " + to_string(size.y) + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    u32 Texture::GetDepth() const { return depth; }
    void Texture::SetDepth(u32 newDepth)
    {
        //ignore if already the same
        if (newDepth == depth) return;

        if (newDepth == 0)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' depth because it cannot be 0!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if ((type == TextureType::TYPE_2D
            || type == TextureType::TYPE_2D_ARRAY
            || type == TextureType::TYPE_CUBEMAP
            || type == TextureType::TYPE_CUBEMAP_ARRAY)
            && newDepth != 1)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' depth "
                "because its texture type is only allowed to have a depth of '1'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (!retreivedProps)
        {
            vkGetPhysicalDeviceProperties(
                GraphicsContext::GetPhysicalDevice(),
                &props);

            retreivedProps = true;
        }

        u32 maxDepth = props.limits.maxImageDimension3D;
        if (newDepth > maxDepth)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' depth "
                "because it exceeded max allowed value '" + to_string(maxDepth) + "'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        depth = newDepth;

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) + "' depth to '" + to_string(depth) + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    u32 Texture::GetLayerCount() const { return layerCount; }
    void Texture::SetLayerCount(u32 newCount)
    {
        //ignore if already the same
        if (newCount == layerCount) return;

        if (newCount == 0)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' layer count because it cannot be 0!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if ((type == TextureType::TYPE_2D
            || type == TextureType::TYPE_3D)
            && newCount != 1)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' layer count "
                "because its texture type is only allowed to have a layer count of '1'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (type == TextureType::TYPE_CUBEMAP
            && newCount != 6)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' layer count "
                "because its texture type is only allowed to have a layer count of '6'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (type == TextureType::TYPE_CUBEMAP_ARRAY
            && newCount % 6 != 0)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' layer count "
                "because its texture type is only allowed to be a multiple of '6'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (!retreivedProps)
        {
            vkGetPhysicalDeviceProperties(
                GraphicsContext::GetPhysicalDevice(),
                &props);

            retreivedProps = true;
        }

        u32 maxLayers = props.limits.maxImageArrayLayers;
        if (newCount > maxLayers)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' layer count "
                "because it exceeded max allowed value '" + to_string(maxLayers) + "'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        layerCount = newCount;

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) + "' layer count to '" + to_string(layerCount) + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    u8 Texture::GetMipMapCount() const { return mipMapCount; }
    void Texture::SetMipMapCount(u8 newCount)
    {
        //ignore if already the same
        if (newCount == mipMapCount) return;

        if (newCount == 0)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' mipmap count because it cannot be 0!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        u8 maxMipMap = floor(log2(std::max(size.x, size.y))) + 1;

        if (newCount > maxMipMap)
        {
            Log::Print(
                "Failed to set texture '" + to_string(ID) + "' mipmap count because it is bigger than max allowed '" 
                + to_string(maxMipMap) + "' for this texture size '" + to_string(size.x) + ", " + to_string(size.y) + "'!",
                "KG_TEXTURE",
                LogType::LOG_ERROR,
                2);

            return;
        }

        mipMapCount = newCount;

        isDirty = true;

        Log::Print(
            "Set texture '" + to_string(ID) + "' mipmap count to '" + to_string(mipMapCount) + "'.",
            "KG_TEXTURE",
            LogType::LOG_SUCCESS);
    }

    void Texture::UpdateTextureData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to update texture '" + to_string(ID) + "' data "
                "because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to update texture '" + to_string(ID) + "' data "
                "because the vma allocator was invalid!");
        }

        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to update texture '" + to_string(ID) 
                + "' data because its shader was invalid! Reason: " + err);
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to update texture '" + to_string(ID) 
                + "' data because its shaders viewport was invalid! Reason: " + err);
        }

        GraphicsContext* gctx{};
        err = GraphicsContext::GetRegistry().GetContent(vp->contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to create texture '" + to_string(ID) 
                + "' data because the graphics context on the shaders viewport was invalid! Reason: " + err);
        }

        bool recreateBuffer{};
        if (vkTexBuffer != VK_NULL_HANDLE)
        {
            if (pixelDataSize < pixelData.size()) recreateBuffer = true;
        }

        if (recreateBuffer
            || vmaTexAllocation == VK_NULL_HANDLE)
        {
            if (vmaTexAllocation != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(
                    allocator,
                    vkTexBuffer,
                    vmaTexAllocation);

                texMappedPtr = nullptr;
            }

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = pixelData.size();
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.flags = 
                VMA_ALLOCATION_CREATE_MAPPED_BIT
                | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

            VkBuffer newBuffer{};
            VmaAllocation newAllocation{};
            VmaAllocationInfo allocResult{};

            VkResult result = vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocInfo,
                &newBuffer,
                &newAllocation,
                &allocResult);

            if (result != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics texture update error",
                    "Failed to update texture because vma allocator init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(result));

                return;
            }

            vkTexBuffer = newBuffer;
            pixelDataSize = pixelData.size();
            vmaTexAllocation = newAllocation;
            texMappedPtr = allocResult.pMappedData;
        }

        memcpy(
            texMappedPtr,
            pixelData.data(),
            pixelData.size());

        if (isDirty)
        {
            //drain the gpu before recreating this texture
            VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
            if (vkResult != VK_SUCCESS)
            {
                GraphicsContext::ForceClose(
                    "KalaGraphics texture error",
                    "Failed to recreate texture '" 
                    + to_string(ID) + "' data because vkDeviceWaitIdle did not succeed!",
                    vkResult);
            }

            //
            // CREATE IMAGE
            //
            
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = ToVkImageType(type);
            imageInfo.extent = 
            {
                scast<u32>(size.x),
                scast<u32>(size.y),
                depth
            };
            imageInfo.mipLevels = mipMapCount;
            imageInfo.arrayLayers = layerCount;
            imageInfo.format = ToVkFormat(format);
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage =
                VK_IMAGE_USAGE_TRANSFER_DST_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.flags = (
                type == TextureType::TYPE_CUBEMAP
                || type == TextureType::TYPE_CUBEMAP_ARRAY)
                ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
                : 0;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            VkImage newImage{};
            VmaAllocation newImageAllocation{};

            VkResult result = vmaCreateImage(
                allocator,
                &imageInfo,
                &allocInfo,
                &newImage,
                &newImageAllocation,
                nullptr);

            if (result != VK_SUCCESS)
            {
                Log::Print(
                    "Failed to update texture '" + to_string(ID) + "' data "
                    "because vk image init failed! Reason: " + GraphicsContext::GetVkResultMessage(result),
                    "KG_TEXTURE",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            //
            // CREATE IMAGE VIEW
            //

            VkImageView newImageView{};

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = newImage;
            viewInfo.viewType = ToVkImageViewType(type);
            viewInfo.format = imageInfo.format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = mipMapCount;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = layerCount;

            result = vkCreateImageView(
                logicalDevice,
                &viewInfo,
                nullptr,
                &newImageView);

            if (result != VK_SUCCESS)
            {
                vmaDestroyImage(
                    allocator,
                    newImage,
                    newImageAllocation);

                Log::Print(
                    "Failed to update texture '" + to_string(ID) + "' data "
                    "because vk image view init failed! Reason: " + GraphicsContext::GetVkResultMessage(result),
                    "KG_TEXTURE",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            //
            // CREATE SAMPLER
            //

            if (!retreivedProps)
            {
                vkGetPhysicalDeviceProperties(
                    GraphicsContext::GetPhysicalDevice(),
                    &props);

                retreivedProps = true;
            }

            VkSampler newSampler{};

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

            samplerInfo.magFilter = ToVkFilter(filterMode);
            samplerInfo.minFilter = ToVkFilter(filterMode);

            samplerInfo.addressModeU = ToVkSamplerAddressMode(wrapMode);
            samplerInfo.addressModeV = ToVkSamplerAddressMode(wrapMode);
            samplerInfo.addressModeW = ToVkSamplerAddressMode(wrapMode);

            if (wrapMode == TextureWrapMode::WRAP_CLAMP_TO_BORDER)
            {
                samplerInfo.borderColor = ToVkBorderColor(borderColor);
            }

            if (useAnisotropy)
            {
                samplerInfo.anisotropyEnable = VK_TRUE;
                samplerInfo.maxAnisotropy = props.limits.maxSamplerAnisotropy;
            }
            if (shadowMode != TextureShadowMapMode::MODE_ALWAYS)
            {
                samplerInfo.compareEnable = VK_TRUE;
                samplerInfo.compareOp = ToVkCompareOp(shadowMode);
            }

            samplerInfo.mipmapMode = ToVkMipMapMode(filterMode);
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = scast<f32>(mipMapCount);

            result = vkCreateSampler(
                logicalDevice,
                &samplerInfo,
                nullptr,
                &newSampler);

            if (result != VK_SUCCESS)
            {
                vkDestroyImageView(
                    logicalDevice,
                    newImageView,
                    nullptr);

                vmaDestroyImage(
                    allocator,
                    newImage,
                    newImageAllocation);

                Log::Print(
                    "Failed to update texture '" + to_string(ID) + "' data "
                    "because vk sampler init failed! Reason: " + GraphicsContext::GetVkResultMessage(result),
                    "KG_TEXTURE",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            //
            // CREATE DESCRIPTOR SET
            //

            VkDescriptorSetAllocateInfo allocSetInfo{};
            allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocSetInfo.descriptorPool = GraphicsContext::GetDescriptorPool();
            allocSetInfo.descriptorSetCount = 1;
            allocSetInfo.pSetLayouts = &shader->descriptorSetLayouts[2];
            
            VkDescriptorSet newDescriptorSet{};

            result = vkAllocateDescriptorSets(
                logicalDevice,
                &allocSetInfo,
                &newDescriptorSet);

            if (result != VK_SUCCESS)
            {
                vkDestroySampler(
                    logicalDevice,
                    newSampler,
                    nullptr);

                vkDestroyImageView(
                    logicalDevice,
                    newImageView,
                    nullptr);

                vmaDestroyImage(
                    allocator,
                    newImage,
                    newImageAllocation);

                Log::Print(
                    "Failed to update texture '" + to_string(ID) + "' data "
                    "because vk descriptor set allocation failed! Reason: " + GraphicsContext::GetVkResultMessage(result),
                    "KG_TEXTURE",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            VkDescriptorImageInfo descImageInfo{};
            descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descImageInfo.imageView = newImageView;
            descImageInfo.sampler = newSampler;

            VkWriteDescriptorSet descWrite{};
            descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrite.dstSet = newDescriptorSet;
            descWrite.dstBinding = 0; // <<<< SET 2 BINDING 0 - TEXTURE SAMPLER SLOT
            descWrite.dstArrayElement = 0;
            descWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descWrite.descriptorCount = 1;
            descWrite.pImageInfo = &descImageInfo;

            vkUpdateDescriptorSets(
                logicalDevice,
                1,
                &descWrite,
                0,
                nullptr);

            //
            // CLEAN UP
            //

            if (vkDescriptorSet != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(
                    logicalDevice,
                    GraphicsContext::GetDescriptorPool(),
                    1,
                    &vkDescriptorSet);

                vkDescriptorSet = VK_NULL_HANDLE;
            }

            if (vkSampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(
                    logicalDevice,
                    vkSampler,
                    nullptr);

                vkSampler = VK_NULL_HANDLE;
            }

            if (vkImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(
                    logicalDevice,
                    vkImageView,
                    nullptr);
                    
                vkImageView = VK_NULL_HANDLE;
            }
            if (vmaImageAllocation != VK_NULL_HANDLE)
            {
                vmaDestroyImage(
                    allocator,
                    vkImage,
                    vmaImageAllocation);

                vkImage = VK_NULL_HANDLE;
                vmaImageAllocation = VK_NULL_HANDLE;
            }

            //
            // FINISH
            //

            vkImage = newImage;
            vmaImageAllocation = newImageAllocation;

            vkImageView = newImageView;

            vkSampler = newSampler;

            vkDescriptorSet = newDescriptorSet;

            VkCommandBuffer vkCommandBuffer = gctx->BeginSingleTimeCommands();

            UploadPixelData(vkCommandBuffer);
            GenerateMipMaps(vkCommandBuffer);

            gctx->EndSingleTimeCommands(vkCommandBuffer);

            isDirty = false;
        }
    }

    void Texture::UploadPixelData(VkCommandBuffer vkCommandBuffer)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = vkImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipMapCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = layerCount;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(
            vkCommandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = layerCount;
        copyRegion.imageExtent = 
        {
            scast<u32>(size.x),
            scast<u32>(size.y),
            depth    
        };

        vkCmdCopyBufferToImage(
            vkCommandBuffer,
            vkTexBuffer,
            vkImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);
    }

    void Texture::GenerateMipMaps(VkCommandBuffer vkCommandBuffer)
    {
        if (mipMapCount > 1)
        {
            i32 mipWidth = scast<i32>(size.x);
            i32 mipHeight = scast<i32>(size.y);

            for (u32 i = 1; i < mipMapCount; ++i)
            {
                //transition previous mip level to TRANSFER_SRC (source for the blit)

                VkImageMemoryBarrier toSrc{};
                toSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                toSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toSrc.image = vkImage;
                toSrc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                toSrc.subresourceRange.baseMipLevel = i - 1;
                toSrc.subresourceRange.levelCount = 1;
                toSrc.subresourceRange.baseArrayLayer = 0;
                toSrc.subresourceRange.layerCount = layerCount;
                toSrc.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(
                    vkCommandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &toSrc);

                //blit from mip i-1 to mip i (half size)

                VkImageBlit blit{};
                blit.srcOffsets[0] = { 0, 0, 0 };
                blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = layerCount;

                i32 nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
                i32 nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

                blit.dstOffsets[0] = { 0, 0, 0 };
                blit.dstOffsets[1] = { nextWidth, nextHeight, 1 };
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount = layerCount;

                vkCmdBlitImage(
                    vkCommandBuffer,
                    vkImage,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    vkImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &blit,
                    VK_FILTER_LINEAR);

                //transition mip i-1 to SHADER_READ_ONLY

                VkImageMemoryBarrier toRead{};
                toRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toRead.image = vkImage;
                toRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                toRead.subresourceRange.baseMipLevel = i - 1;
                toRead.subresourceRange.levelCount = 1;
                toRead.subresourceRange.baseArrayLayer = 0;
                toRead.subresourceRange.layerCount = layerCount;
                toRead.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(
                    vkCommandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &toRead);
            }

            //transition the last mip level to SHADER_READ_ONLY

            VkImageMemoryBarrier lastMip{};
            lastMip.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            lastMip.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            lastMip.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            lastMip.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            lastMip.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            lastMip.image = vkImage;
            lastMip.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            lastMip.subresourceRange.baseMipLevel = mipMapCount - 1;
            lastMip.subresourceRange.levelCount = 1;
            lastMip.subresourceRange.baseArrayLayer = 0;
            lastMip.subresourceRange.layerCount = layerCount;
            lastMip.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            lastMip.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                vkCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &lastMip);
        }
        else
        {
            //no mips beyond base, just transition mip 0 straight to SHADER_READ_ONLY

            VkImageMemoryBarrier toRead{};
            toRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toRead.image = vkImage;
            toRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            toRead.subresourceRange.baseMipLevel = 0;
            toRead.subresourceRange.levelCount = 1;
            toRead.subresourceRange.baseArrayLayer = 0;
            toRead.subresourceRange.layerCount = layerCount;
            toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                vkCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toRead);
        }
    }

    void Texture::Destroy()
    {
        for (u32 mID : meshIDs)
        {
            Mesh* m{};
            string err = Mesh::GetRegistry().GetContent(mID, m);
            if (err.empty()) m->textureID = 0;
        }
        meshIDs.clear();

        //only remove this texture from shader meshes list if the texture is still valid

        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (err.empty())
        {
            erase(
                shader->textureIDs,
                ID);
        }
         
        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to destroy texture '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Texture::~Texture()
    {
        Log::Print(
            "Destroying texture '" + to_string(ID) + "'.",
            "KG_TEXTURE",
            LogType::LOG_INFO);

        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE) 
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to clear shader '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics texture error",
                "Failed to clear texture '" + to_string(ID) + "' data "
                "because the vma allocator was invalid!");
        }

        //drain the gpu before destroying this texture
        VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
        if (vkResult != VK_SUCCESS)
        {
            GraphicsContext::ForceClose(
                "KalaGraphics texture error",
                "Failed to texture camera '" 
                + to_string(ID) + "' because vkDeviceWaitIdle did not succeed!",
                vkResult);
        }

        if (vmaTexAllocation != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkTexBuffer,
                vmaTexAllocation);

            texMappedPtr = nullptr;
        }

        if (vkDescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(
                logicalDevice,
                GraphicsContext::GetDescriptorPool(),
                1,
                &vkDescriptorSet);

            vkDescriptorSet = VK_NULL_HANDLE;
        }

        if (vkSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(
                logicalDevice,
                vkSampler,
                nullptr);
        }

        if (vkImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(
                logicalDevice,
                vkImageView,
                nullptr);
        }
        if (vmaImageAllocation != VK_NULL_HANDLE)
        {
            vmaDestroyImage(
                allocator,
                vkImage,
                vmaImageAllocation);
        }
    }
}