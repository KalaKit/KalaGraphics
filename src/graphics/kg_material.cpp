//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "vulkan/vulkan_core.h"

#include "log_utils.hpp"

#include "graphics/kg_material.hpp"
#include "graphics/kg_mesh.hpp"
#include "graphics/kg_shader.hpp"
#include "graphics/kg_viewport.hpp"
#include "graphics/kg_texture.hpp"
#include "widgets/kg_text.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::kclamp;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Graphics::Mesh;
using KalaGraphics::Graphics::Shader;
using KalaGraphics::Graphics::Viewport;
using KalaGraphics::Graphics::Texture;
using KalaGraphics::Widgets::Text;

using std::string;
using std::to_string;
using std::clamp;

static bool isVerboseLoggingEnabled{};

namespace KalaGraphics::Graphics
{
    static KalaGraphicsRegistry<Material> registry{};

    KalaGraphicsRegistry<Material>& Material::GetRegistry() { return registry; }

    bool Material::IsVerboseLoggingEnabled() { return isVerboseLoggingEnabled; }
    void Material::SetVerboseLoggingState(bool state) { isVerboseLoggingEnabled = state; }

    Material* Material::Initialize(u32 shaderID)
    {
        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize material because the shader was invalid! Reason: " + err,
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return nullptr;
        }

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(shader->rootTextureID, texture);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to initialize material because the shader '" + to_string(shader->ID) 
                + "' root texture '" + to_string(shader->rootTextureID) + "' was invalid!");
        }

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        Material* matPtr{};
        err = registry.AddContent(newID, matPtr);
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics material error",
				"Failed to initialize material! Reason: " + err);
        }

        matPtr->ID = newID;

        matPtr->shaderID = shaderID;
        shader->materialIDs.push_back(newID);

        //rect
        matPtr->rectData.baseColorTextureID = shader->rootTextureID;
        //font
        matPtr->fontData.baseColorTextureID = shader->rootTextureID;
        //unlit
        matPtr->unlitData.baseColorTextureID = shader->rootTextureID;
        //blinn-phong
        matPtr->blinnPhongData.baseColorTextureID = shader->rootTextureID;
        matPtr->blinnPhongData.specularTextureID  = shader->rootTextureID;
        matPtr->blinnPhongData.normalTextureID    = shader->rootTextureID;
        //pbr
        matPtr->pbrData.baseColorTextureID         = shader->rootTextureID;
        matPtr->pbrData.metallicRoughnessTextureID = shader->rootTextureID;
        matPtr->pbrData.normalTextureID            = shader->rootTextureID;
        matPtr->pbrData.occlusionTextureID         = shader->rootTextureID;
        matPtr->pbrData.emissiveTextureID          = shader->rootTextureID;

        Log::Print(
            "Created new material '" + to_string(newID) + "' for shader '" + to_string(shaderID) + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);

        return matPtr;
    }

    u32 Material::GetID() const { return ID; }
    u32 Material::GetShaderID() const { return shaderID; }
    vector<u32> Material::GetMeshIDs() const { return meshIDs; }

    MaterialType2D Material::GetMaterial2DType() const { return material2DType; }
    void Material::SetMaterial2DType(MaterialType2D newValue)
    {
        material2DType = newValue;

        string typeStr{};
        switch (material2DType)
        {
        default:
        case MaterialType2D::RECT:
            typeStr = "rect";
            break;
        case MaterialType2D::FONT:
            typeStr = "font";
            break;
        }

        Log::Print(
            "Set 2D material '" + to_string(ID) + "' type to '" + typeStr + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);
    }

    MaterialType3D Material::GetMaterial3DType() const { return material3DType; }
    void Material::SetMaterial3DType(MaterialType3D newValue)
    {
        material3DType = newValue;

        string typeStr{};
        switch (material3DType)
        {
        default:
        case MaterialType3D::UNLIT:
            typeStr = "unlit";
            break;
        case MaterialType3D::BLINN_PHONG:
            typeStr = "blinn-phong";
            break;
        case MaterialType3D::PBR:
            typeStr = "pbr";
            break;
        }

        Log::Print(
            "Set 3D material '" + to_string(ID) + "' type to '" + typeStr + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);
    }

    const vec4& Material::GetBaseColor() const
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' base color because its shader was invalid! Reason: " + err);
        }

        if (!s->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::UNLIT:
                return unlitData.baseColor;
            case MaterialType3D::BLINN_PHONG:
                return blinnPhongData.baseColor;
            case MaterialType3D::PBR:
                return pbrData.baseColor;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::RECT:
                return rectData.baseColor;
            case MaterialType2D::FONT:
                return fontData.baseColor;
            }
        }
    }
    void Material::SetBaseColor(vec4&& newValue)
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' base color because its shader was invalid! Reason: " + err);
        }

        vec4* baseColor{};
        if (!s->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::UNLIT:
                baseColor = &unlitData.baseColor;
                break;
            case MaterialType3D::BLINN_PHONG:
                baseColor = &blinnPhongData.baseColor;
                break;
            case MaterialType3D::PBR:
                baseColor = &pbrData.baseColor;
                break;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::RECT:
                baseColor = &rectData.baseColor;
                break;
            case MaterialType2D::FONT:
                baseColor = &fontData.baseColor;
                break;
            }
        }

        if (newValue == *baseColor)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "base color because it already is the same!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return;
        }

        *baseColor = kclamp(newValue, 0, 1);

        string colorStr = 
            to_string(baseColor->x) + ", "
            + to_string(baseColor->y) + ", "
            + to_string(baseColor->z) + ", "
            + to_string(baseColor->w);

        Log::Print(
            "Set material '" + to_string(ID) + "' color to '" + colorStr + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);
    }

    AlphaMode Material::GetAlphaMode() const
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' alpha mode because its shader was invalid! Reason: " + err);
        }

        if (s->is2D
            && material2DType == MaterialType2D::FONT)
        {
            Log::Print(
                "Failed to get material '" + to_string(ID) + "' "
                "alpha mode because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return {};
        }

        if (!s->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::UNLIT:
                return unlitData.alphaMode;
            case MaterialType3D::BLINN_PHONG:
                return blinnPhongData.alphaMode;
            case MaterialType3D::PBR:
                return pbrData.alphaMode;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::RECT:
                return rectData.alphaMode;
            }
        }
    }
    void Material::SetAlphaMode(AlphaMode newValue)
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' alpha mode because its shader was invalid! Reason: " + err);
        }

        if (s->is2D
            && material2DType == MaterialType2D::FONT)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha mode because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return;
        }

        AlphaMode* alphaMode{};
        if (!s->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::UNLIT:
                alphaMode = &unlitData.alphaMode;
                break;
            case MaterialType3D::BLINN_PHONG:
                alphaMode = &blinnPhongData.alphaMode;
                break;
            case MaterialType3D::PBR:
                alphaMode = &pbrData.alphaMode;
                break;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::RECT:
                alphaMode = &rectData.alphaMode;
                break;
            }
        }

        if (newValue == *alphaMode)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha mode because it already is the same!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return;
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(s->viewportID, vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) + "' alpha mode "
                "because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
        }

        if (!s->is2D) vp->is3DMeshSortDirty = true;
        else          vp->is2DMeshSortDirty = true;

        *alphaMode = newValue;
        string alphaModeString = *alphaMode == AlphaMode::TYPE_OPAQUE 
            ? "opaque" 
            : (*alphaMode == AlphaMode::TYPE_BLEND 
                ? "blend" 
                : "mask");

        Log::Print(
            "Set material '" + to_string(ID) + "' alpha mode to '" + alphaModeString + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);
    }

    f32 Material::GetAlphaCutoff() const
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' alpha cutoff because its shader was invalid! Reason: " + err);
        }

        if (s->is2D
            && material2DType == MaterialType2D::FONT)
        {
            Log::Print(
                "Failed to get material '" + to_string(ID) + "' "
                "alpha cutoff because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return {};
        }

        if (!s->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::UNLIT:
                return unlitData.alphaCutoff;
            case MaterialType3D::BLINN_PHONG:
                return blinnPhongData.alphaCutoff;
            case MaterialType3D::PBR:
                return pbrData.alphaCutoff;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::RECT:
                return rectData.alphaCutoff;
            }
        }
    }
    void Material::SetAlphaCutoff(f32 newValue)
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' alpha cutoff because its shader was invalid! Reason: " + err);
        }

        if (s->is2D
            && material2DType == MaterialType2D::FONT)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha cutoff because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return;
        }

        if (GetAlphaMode() != AlphaMode::TYPE_MASK)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha cutoff because alpha mode is not set to mask!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return;
        }

        f32* alphaCutoff{};
        if (!s->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::UNLIT:
                alphaCutoff = &unlitData.alphaCutoff;
                break;
            case MaterialType3D::BLINN_PHONG:
                alphaCutoff = &blinnPhongData.alphaCutoff;
                break;
            case MaterialType3D::PBR:
                alphaCutoff = &pbrData.alphaCutoff;
                break;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::RECT:
                alphaCutoff = &rectData.alphaCutoff;
                break;
            }
        }

        newValue = clamp(newValue, 0.0f, 1.0f);

        if (newValue == *alphaCutoff)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha cutoff because it already is the same!",
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            return;
        }

        *alphaCutoff = newValue;

        Log::Print(
            "Set material '" + to_string(ID) + "' alpha cutoff to '" + to_string(*alphaCutoff) + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);
    }

    u32 Material::GetBaseColorTextureID()
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' base color texture ID because its shader was invalid! Reason: " + err);
        }

        auto get_target_texture_id = [&](u32*& outOrigin) -> u32
            {
                if (!s->is2D)
                {
                    switch (material3DType)
                    {
                    default:
                    case MaterialType3D::UNLIT:
                        outOrigin = &unlitData.baseColorTextureID;
                        return unlitData.baseColorTextureID;
                    case MaterialType3D::BLINN_PHONG:
                        outOrigin = &blinnPhongData.baseColorTextureID;
                        return blinnPhongData.baseColorTextureID;
                    case MaterialType3D::PBR:
                        outOrigin = &pbrData.baseColorTextureID;
                        return pbrData.baseColorTextureID;
                    }
                }
                else
                {
                    switch (material2DType)
                    {
                    default:
                    case MaterialType2D::RECT:
                        outOrigin = &rectData.baseColorTextureID;
                        return rectData.baseColorTextureID;
                    case MaterialType2D::FONT:
                        outOrigin = &fontData.baseColorTextureID;
                        return fontData.baseColorTextureID;
                    }
                }
            };

        u32* originID{};
        u32 targetTextureID = get_target_texture_id(originID);

        Texture* tex{};
        err = Texture::GetRegistry().GetContent(targetTextureID, tex);
        if (!err.empty())
        {
            Log::Print(
                "Failed to get material '" + to_string(ID) + "' base color texture ID "
                "because the texture ID was invalid! Using fallback texture. Reason: " + err,
                "KG_MATERIAL",
                LogType::LOG_WARNING);

            

            err = Texture::GetRegistry().GetContent(s->GetFallbackTextureID(), tex);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics material error",
                    "Failed to get material '" + to_string(ID) + "' base color texture ID "
                    "because fallback for invalid stored texture was invalid! Reason: " + err);
            }

            *originID = tex->ID;
            targetTextureID = tex->ID;
        }
        
        return targetTextureID;
    }
    void Material::SetBaseColorTextureID(u32 newValue)
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' base color texture ID because its shader was invalid! Reason: " + err);
        }

        u32* textureID{};
        size_t textureSlot{};

        if (!s->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::UNLIT:
                textureID = &unlitData.baseColorTextureID;
                textureSlot = 2;
                break;
            case MaterialType3D::BLINN_PHONG:
                textureID = &blinnPhongData.baseColorTextureID;
                textureSlot = 3;
                break;
            case MaterialType3D::PBR:
                textureID = &pbrData.baseColorTextureID;
                textureSlot = 6;
                break;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::RECT:
                textureID = &rectData.baseColorTextureID;
                textureSlot = 0;
                break;
            case MaterialType2D::FONT:
                textureID = &fontData.baseColorTextureID;
                textureSlot = 1;
                break;
            }
        }

        if (*textureID == newValue)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) 
                + "' base color texture ID to '" + to_string(newValue) 
                + "' because it already is the same!",
                "KG_MESH",
                LogType::LOG_WARNING);

            return;
        }

        Texture* oldTexture{};
        if (*textureID != 0)
        {
            err = Texture::GetRegistry().GetContent(*textureID, oldTexture);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics material error",
                    "Failed to set base color texture ID for material '" 
                    + to_string(ID) + "' because its old texture was invalid! Reason: " + err);
            }
        }

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(newValue, texture);
        if (!texture)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) 
                + "' base color texture ID because it was invalid! Reason: " + err,
                "KG_MESH",
                LogType::LOG_WARNING);

            return;
        }

        *textureID = newValue;

        //remove from old texture
        if (oldTexture)
        {
            for (auto it = oldTexture->materialIDs.begin(); it != oldTexture->materialIDs.end(); ++it)
            {
                if (it->first != ID) continue;

                it->second[textureSlot] = false;

                bool isStillUsed{};

                for (bool slot : it->second)
                {
                    if (slot)
                    {
                        isStillUsed = true;
                        break;
                    }
                }

                if (!isStillUsed) oldTexture->materialIDs.erase(it);

                break;
            }
        }

        //add to new texture
        pair<u32, array<bool, 11>>* targetTexturePair{};

        for (auto& target : texture->materialIDs)
        {
            if (target.first == ID)
            {
                targetTexturePair = &target;
                break;
            }
        }

        if (!targetTexturePair)
        {
            texture->materialIDs.push_back({ ID, {} });
            targetTexturePair = &texture->materialIDs.back();
        }

        targetTexturePair->second[textureSlot] = true;

        Log::Print(
            "Set material '" + to_string(ID) 
            + "' base color texture ID to '" + to_string(*textureID) + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    void Material::Update(VkCommandBuffer buffer)
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' base color because its shader was invalid! Reason: " + err);
        }

        /*
        Log::Print(
            "@@@@@\n"
            "material: " + to_string(ID) + "\n"
            "shader: " + to_string(shaderID) + "\n"
            "is 2D: " + (s->is2D ? "true" : "false") + "\n"
            "2D material type: " + to_string(scast<u32>(material2DType)) + "\n"
            "3D material type: " + to_string(scast<u32>(material3DType)));
        */

        vkCmdPushConstants(
            buffer,
            s->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(GetBaseColor()),
            &GetBaseColor());

        if (material2DType != MaterialType2D::FONT)
        {
            u32 alphaModeValue = scast<u32>(GetAlphaMode());
            vkCmdPushConstants(
                buffer,
                s->pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                sizeof(GetBaseColor()),
                sizeof(alphaModeValue),
                &alphaModeValue);

            f32 alphaCutoff = GetAlphaCutoff();

            vkCmdPushConstants(
                buffer,
                s->pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                sizeof(GetBaseColor()) + sizeof(alphaModeValue),
                sizeof(alphaCutoff),
                &alphaCutoff);
        }

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(GetBaseColorTextureID(), texture);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to render material '" + to_string(ID) 
                + "' because its base texture was invalid! Reason: " + err);
        }

        vkCmdBindDescriptorSets(
            buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            s->pipelineLayout,
            2, // <<<< SET 2 BINDING 0 - TEXTURE SAMPLER SLOT
            1,
            &texture->vkDescriptorSet,
            0,
            nullptr);
    }

    void Material::Destroy()
    {
        Shader* s{};
        string err = Shader::GetRegistry().GetContent(shaderID, s);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to destroy material '" + to_string(ID) 
                + "' because its shader was invalid! Reason: " + err);
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(s->GetViewportID(), vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to destroy material '" + to_string(ID) 
                + "' because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
        }

        u32 rootMatID{};
        if (shaderID == vp->GetRootShaderID(RootShaderTarget::UNLIT))
        {
            rootMatID = vp->GetRootMaterialID(RootShaderTarget::UNLIT);
        }
        if (shaderID == vp->GetRootShaderID(RootShaderTarget::RECT))
        {
            rootMatID = vp->GetRootMaterialID(RootShaderTarget::RECT);
        }
        if (shaderID == vp->GetRootShaderID(RootShaderTarget::FONT))
        {
            rootMatID = vp->GetRootMaterialID(RootShaderTarget::FONT);
        }

        //something is very wrong if no root material was found
        if (rootMatID == 0)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to destroy material '" + to_string(ID) 
                + "' because no root material could be found!");
        }

        Material* rootMat{};
        //if root mat id is current material that is being destroyed then don't search for it and dont use it
        if (rootMatID != ID)
        {
            //discard the error, we dont care about not found value response
            string _ = registry.GetContent(rootMatID, rootMat);
        }

        vector<u32> meshIDsCopy = meshIDs;
        for (u32 meshID : meshIDsCopy)
        {
            Mesh* mesh{};
            string err = Mesh::GetRegistry().GetContent(meshID, mesh);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics material error",
                    "Failed to destroy material '" + to_string(ID) 
                    + "' because its mesh was invalid! Reason: " + err);
            }

            //destroy mesh because viewports root material is gone,
            //no fallback to assign, probably shutting down program
            if (!rootMat
                || rootMatID == 0)
            {
                if (mesh->textWidgetID != 0)
                {
                    Text* t{};
                    string err = Text::GetRegistry().GetContent(mesh->textWidgetID, t);
                    if (!err.empty())
                    {
                        KalaGraphicsCore::ForceClose(
                            "KalaGraphics material error",
                            "Failed to destroy material '" + to_string(ID) 
                            + "' because its mesh '" + to_string(mesh->ID) + "' text widget was invalid! Reason: " + err);
                    }
                    t->Destroy();
                }
                else mesh->Destroy();
            }
            //update mesh material to root material because it still exists
            else
            {
                mesh->materialID = rootMatID;
                rootMat->meshIDs.push_back(meshID);
            }
        }

        vector<u32*> textureIDs
        {
            //rect
            &rectData.baseColorTextureID,
            //font
            &fontData.baseColorTextureID,
            //unlit
            &unlitData.baseColorTextureID,
            //blinn-phong
            &blinnPhongData.baseColorTextureID,
            &blinnPhongData.specularTextureID,
            &blinnPhongData.normalTextureID,
            //pbr
            &pbrData.baseColorTextureID,
            &pbrData.metallicRoughnessTextureID,
            &pbrData.normalTextureID,
            &pbrData.occlusionTextureID,
            &pbrData.emissiveTextureID
        };

        for (size_t i = 0; i < textureIDs.size(); i++)
        {
            u32* tex = textureIDs[i];

            Texture* oldTex{};
            err = Texture::GetRegistry().GetContent(*tex, oldTex);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics material error",
                    "Failed to destroy material '" 
                    + to_string(ID) + "' because its old texture was invalid! Reason: " + err);
            }

            for (auto it = oldTex->materialIDs.begin(); it != oldTex->materialIDs.end(); ++it)
            {
                if (it->first == ID)
                {
                    oldTex->materialIDs.erase(it);
                    break;
                }
            }
        }

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to destroy material! Reason: " + err);
        }
    }

    Material::~Material()
    {
		Log::Print(
			"Destroying material '" + to_string(ID) + "'.",
			"KG_MATERIAL",
			LogType::LOG_INFO);
    }
}