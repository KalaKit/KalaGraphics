//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "vulkan/vulkan_core.h"

#include "log_utils.hpp"

#include "graphics/kg_material.hpp"
#include "graphics/kg_mesh.hpp"
#include "graphics/kg_shader.hpp"
#include "graphics/kg_viewport.hpp"
#include "graphics/kg_texture.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::kclamp;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Graphics::Mesh;
using KalaGraphics::Graphics::Shader;
using KalaGraphics::Graphics::Viewport;
using KalaGraphics::Graphics::Texture;

using std::unique_ptr;
using std::make_unique;
using std::string;
using std::to_string;
using std::clamp;

namespace KalaGraphics::Graphics
{
    static KalaGraphicsRegistry<Material> registry{};

    KalaGraphicsRegistry<Material>& Material::GetRegistry() { return registry; }

    Material* Material::Initialize(u32 meshID)
    {
        Mesh* mesh{};
        string err = Mesh::GetRegistry().GetContent(meshID, mesh);

        Shader* shader = Shader::GetRegistry().GetAllContent().front();

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(shader->rootTextureID, texture);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to create material because the shader '" + to_string(shader->ID) 
                + "' root texture '" + to_string(shader->rootTextureID) + "' was invalid!");
        }

        unique_ptr<Material> newMat = make_unique<Material>();
        Material* matPtr = newMat.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        matPtr->ID = newID;

        matPtr->meshID = meshID;
        mesh->materialID = newID;

        matPtr->rectData.baseColorTextureID = texture->ID;

        err = registry.AddContent(newID, std::move(newMat));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics material error",
				"Failed to initialize material! Reason: " + err);
        }

        Log::Print(
            "Created new material '" + to_string(newID) + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);

        return matPtr;
    }

    u32 Material::GetID() const { return ID; }
    u32 Material::GetMeshID() const { return meshID; }

    MaterialType2D Material::GetMaterial2DType() const { return material2DType; }
    void Material::SetMaterial2DType(MaterialType2D newValue)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' 2D type because its mesh was invalid! Reason: " + err);
        }

        material2DType = newValue;

        string typeStr{};
        switch (material2DType)
        {
        default:
        case MaterialType2D::M_RECT:
            typeStr = "rect";
            break;
        case MaterialType2D::M_FONT:
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
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' 3D type because its mesh was invalid! Reason: " + err);
        }

        material3DType = newValue;

        string typeStr{};
        switch (material3DType)
        {
        default:
        case MaterialType3D::M_UNLIT:
            typeStr = "unlit";
            break;
        case MaterialType3D::M_BLINN_PHONG:
            typeStr = "blinn-phong";
            break;
        case MaterialType3D::M_PBR:
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
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' base color because its mesh was invalid! Reason: " + err);
        }

        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                return unlitData.baseColor;
            case MaterialType3D::M_BLINN_PHONG:
                return blinnPhongData.baseColor;
            case MaterialType3D::M_PBR:
                return pbrData.baseColor;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::M_RECT:
                return rectData.baseColor;
            case MaterialType2D::M_FONT:
                return fontData.baseColor;
            }
        }
    }
    void Material::SetBaseColor(vec4&& newValue)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' base color because its mesh was invalid! Reason: " + err);
        }

        vec4* baseColor{};
        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                baseColor = &unlitData.baseColor;
                break;
            case MaterialType3D::M_BLINN_PHONG:
                baseColor = &blinnPhongData.baseColor;
                break;
            case MaterialType3D::M_PBR:
                baseColor = &pbrData.baseColor;
                break;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::M_RECT:
                baseColor = &rectData.baseColor;
                break;
            case MaterialType2D::M_FONT:
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
                LogType::LOG_ERROR,
                2);

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
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' alpha mode because its mesh was invalid! Reason: " + err);
        }

        if (m->is2D
            && material2DType == MaterialType2D::M_FONT)
        {
            Log::Print(
                "Failed to get material '" + to_string(ID) + "' "
                "alpha mode because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                return unlitData.alphaMode;
            case MaterialType3D::M_BLINN_PHONG:
                return blinnPhongData.alphaMode;
            case MaterialType3D::M_PBR:
                return pbrData.alphaMode;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::M_RECT:
                return rectData.alphaMode;
            }
        }
    }
    void Material::SetAlphaMode(AlphaMode newValue)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' alpha mode because its mesh was invalid! Reason: " + err);
        }

        if (m->is2D
            && material2DType == MaterialType2D::M_FONT)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha mode because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_ERROR,
                2);

            return;
        }

        AlphaMode* alphaMode{};
        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                alphaMode = &unlitData.alphaMode;
                break;
            case MaterialType3D::M_BLINN_PHONG:
                alphaMode = &blinnPhongData.alphaMode;
                break;
            case MaterialType3D::M_PBR:
                alphaMode = &pbrData.alphaMode;
                break;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::M_RECT:
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
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (m->shaderID != 0)
        {
            Shader* shader{};
            string err = Shader::GetRegistry().GetContent(m->shaderID, shader);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics material error",
                    "Failed to set material '" + to_string(ID) + "' alpha mode "
                    "because its mesh '" + to_string(meshID) + "' shader was invalid! Reason: " + err);
            }

            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics material error",
                    "Failed to set material '" + to_string(ID) + "' alpha mode "
                    "because its shader '" + to_string(m->shaderID) + "' viewport was invalid! Reason: " + err);
            }

            if (!m->is2D) vp->is3DMeshSortDirty = true;
            else          vp->is2DMeshSortDirty = true;
        }

        *alphaMode = newValue;
        string alphaModeString = *alphaMode == AlphaMode::A_OPAQUE 
            ? "opaque" 
            : (*alphaMode == AlphaMode::A_BLEND 
                ? "blend" 
                : "mask");

        Log::Print(
            "Set material '" + to_string(ID) + "' alpha mode to '" + alphaModeString + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);
    }

    f32 Material::GetAlphaCutoff() const
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' alpha cutoff because its mesh was invalid! Reason: " + err);
        }

        if (m->is2D
            && material2DType == MaterialType2D::M_FONT)
        {
            Log::Print(
                "Failed to get material '" + to_string(ID) + "' "
                "alpha cutoff because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                return unlitData.alphaCutoff;
            case MaterialType3D::M_BLINN_PHONG:
                return blinnPhongData.alphaCutoff;
            case MaterialType3D::M_PBR:
                return pbrData.alphaCutoff;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::M_RECT:
                return rectData.alphaCutoff;
            }
        }
    }
    void Material::SetAlphaCutoff(f32 newValue)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' alpha cutoff because its mesh was invalid! Reason: " + err);
        }

        if (m->is2D
            && material2DType == MaterialType2D::M_FONT)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha cutoff because font material does not support it!",
                "KG_MATERIAL",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (GetAlphaMode() != AlphaMode::A_MASK)
        {
            Log::Print(
                "Failed to set material '" + to_string(ID) + "' "
                "alpha cutoff because alpha mode is not set to mask!",
                "KG_MATERIAL",
                LogType::LOG_ERROR,
                2);

            return;
        }

        f32* alphaCutoff{};
        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                alphaCutoff = &unlitData.alphaCutoff;
                break;
            case MaterialType3D::M_BLINN_PHONG:
                alphaCutoff = &blinnPhongData.alphaCutoff;
                break;
            case MaterialType3D::M_PBR:
                alphaCutoff = &pbrData.alphaCutoff;
                break;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::M_RECT:
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
                LogType::LOG_ERROR,
                2);

            return;
        }

        *alphaCutoff = newValue;

        Log::Print(
            "Set material '" + to_string(ID) + "' alpha cutoff to '" + to_string(*alphaCutoff) + "'!",
            "KG_MATERIAL",
            LogType::LOG_SUCCESS);
    }

    u32 Material::GetBaseColorTextureID() const
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to get material '" + to_string(ID) 
                + "' base color texture ID because its mesh was invalid! Reason: " + err);
        }

        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                return unlitData.baseColorTextureID;
            case MaterialType3D::M_BLINN_PHONG:
                return blinnPhongData.baseColorTextureID;
            case MaterialType3D::M_PBR:
                return pbrData.baseColorTextureID;
            }
        }
        else
        {
            switch (material2DType)
            {
            default:
            case MaterialType2D::M_RECT:
                return rectData.baseColorTextureID;
            case MaterialType2D::M_FONT:
                return fontData.baseColorTextureID;
            }
        }
    }
    void Material::SetBaseColorTextureID(u32 newValue)
    {
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to set material '" + to_string(ID) 
                + "' base color texture ID because its mesh was invalid! Reason: " + err);
        }

        u32* textureID{};
        size_t textureSlot{};

        if (!m->is2D)
        {
            switch (material3DType)
            {
            default:
            case MaterialType3D::M_UNLIT:
                textureID = &unlitData.baseColorTextureID;
                textureSlot = 2;
                break;
            case MaterialType3D::M_BLINN_PHONG:
                textureID = &blinnPhongData.baseColorTextureID;
                textureSlot = 3;
                break;
            case MaterialType3D::M_PBR:
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
            case MaterialType2D::M_RECT:
                textureID = &rectData.baseColorTextureID;
                textureSlot = 0;
                break;
            case MaterialType2D::M_FONT:
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
                + "' because it already is that value!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

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
                LogType::LOG_ERROR,
                2);

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
        Mesh* m{};
        string err = Mesh::GetRegistry().GetContent(meshID, m);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to update material '" + to_string(ID) 
                + "' data because its shader was invalid! Reason: " + err);
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(m->shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to update material '" + to_string(ID) 
                + "' data because its mesh '" + to_string(meshID) + "' shader was invalid! Reason: " + err);
        }

        vkCmdPushConstants(
            buffer,
            shader->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(GetBaseColor()),
            &GetBaseColor());

        u32 alphaModeValue = scast<u32>(GetAlphaMode());
        vkCmdPushConstants(
            buffer,
            shader->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(GetBaseColor()),
            sizeof(alphaModeValue),
            &alphaModeValue);

        if (GetAlphaMode() == AlphaMode::A_MASK)
        {
            f32 alphaCutoff = GetAlphaCutoff();

            vkCmdPushConstants(
                buffer,
                shader->pipelineLayout,
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
            shader->pipelineLayout,
            2, // <<<< SET 2 BINDING 0 - TEXTURE SAMPLER SLOT
            1,
            &texture->vkDescriptorSet,
            0,
            nullptr);
    }

    void Material::Destroy()
    {
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
            string err = Texture::GetRegistry().GetContent(*tex, oldTex);
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

        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics material error",
                "Failed to destroy material '" + to_string(ID) + "'! Reason: " + err);
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