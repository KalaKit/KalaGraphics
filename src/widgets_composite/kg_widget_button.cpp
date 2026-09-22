//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"

#include "widgets_composite/kg_widget_button.hpp"
#include "widgets_primitive/kg_widget_text.hpp"
#include "graphics/kg_viewport.hpp"
#include "graphics/kg_shader.hpp"
#include "graphics/kg_texture.hpp"
#include "graphics/kg_material.hpp"
#include "graphics/kg_mesh.hpp"
#include "import/kg_import_font.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::vec3;

using KalaGraphics::PrimitiveWidgets::Text;
using KalaGraphics::Graphics::AnchorPosition;
using KalaGraphics::Graphics::RootShaderTarget;
using KalaGraphics::Graphics::Viewport;
using KalaGraphics::Graphics::Shader;
using KalaGraphics::Graphics::TexturePixelFormat;
using KalaGraphics::Graphics::Texture;
using KalaGraphics::Graphics::MaterialType2D;
using KalaGraphics::Graphics::Material;
using KalaGraphics::Graphics::Mesh;
using KalaGraphics::Import::ImportFont;
using KalaGraphics::Core::KalaGraphicsCore;

using std::string;
using std::to_string;
using std::unique_ptr;
using std::make_unique;

static bool isVerboseLoggingEnabled{};

namespace KalaGraphics::CompositeWidgets
{
    static KalaGraphicsRegistry<Button> registry{};

    KalaGraphicsRegistry<Button>& Button::GetRegistry() { return registry; }

    bool Button::IsVerboseLoggingEnabled() { return isVerboseLoggingEnabled; }
    void Button::SetVerboseLoggingState(bool state) { isVerboseLoggingEnabled = state; }

    Button* Button::Initialize(u32 viewportID)
    {
        Viewport* vp{};
        string err = Viewport::GetRegistry().GetContent(viewportID, vp);
        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize button widget because its viewport '" 
                + to_string(viewportID) + "' was invalid! Reason: " + err,
                "KG_WIDGET_BUTTON",
                LogType::LOG_WARNING);

            return {};
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(vp->GetRootShaderID(RootShaderTarget::T_RECT), shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to initialize button widget because its viewport '" 
                + to_string(viewportID) + "' root RECT shader was invalid! Reason: " + err);
        }

        Texture* tex = Texture::Initialize(
            shader->GetID(),
            {
                .format = TexturePixelFormat::FORMAT_BASIC_R8G8B8A8
            });
        if (!tex)
        {
            Log::Print(
                "Failed to initialize button widget because its texture failed to initialize!",
                "KG_WIDGET_BUTTON",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        Mesh* mesh = Mesh::Initialize(shader->GetID());
        if (!mesh)
        {
            Log::Print(
                "Failed to initialize button widget because its mesh failed to initialize!",
                "KG_WIDGET_BUTTON",
                LogType::LOG_ERROR,
                2);

            return {};
        }

        Material* mat{};
        err = Material::GetRegistry().GetContent(mesh->GetMaterialID(), mat);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to initialize button widget because its mesh material was invalid! Reason: " + err);
        }

        if (ImportFont::GetRegistry().GetAllContent().empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to initialize button widget because no fonts were initialized!");
        }

        //TODO: add root font
        ImportFont* font = ImportFont::GetRegistry().GetAllContent().front();
        if (!font)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to initialize button widget because the root font is invalid!");
        }

        Text* text = Text::Initialize(
            font->GetID(),
            viewportID);
        if (!text)
        {
            Log::Print(
                "Failed to initialize button widget because its text widget failed to initialize!",
                "KG_WIDGET_BUTTON",
                LogType::LOG_ERROR,
                2);

            mesh->Destroy();

            return {};
        }

        Mesh* textMesh{};
        err = Mesh::GetRegistry().GetContent(text->meshID, textMesh);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to initialize button widget because its text widgets mesh is invalid! Reason: " + err);
        }

        textMesh->SetTargetAnchorPosition(
            AnchorPosition::P_CENTER,
            mesh->ID);

        mat->SetMaterial2DType(MaterialType2D::M_RECT);
        mat->SetBaseColorTextureID(tex->GetID());
        mat->SetBaseColor({ vec3{ 0.0f }, 1.0f });

        unique_ptr<Button> newButton = make_unique<Button>();
        Button* buttonPtr = newButton.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        buttonPtr->ID = newID;
        buttonPtr->shaderID = shader->GetID();
        buttonPtr->textureID = tex->GetID();
        buttonPtr->meshID = mesh->GetID();
        buttonPtr->textWidgetID = text->ID;

        tex->textWidgetID = newID;
        mesh->textWidgetID = newID;
        shader->textWidgetIDs.push_back(newID);
        text->buttonWidgetID = newID;

        err = registry.AddContent(newID, std::move(newButton));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics button widget error",
				"Failed to initialize button widget! Reason: " + err);
        }

        Log::Print(
			"Created new button widget '" + to_string(newID) + "'!",
			"KG_WIDGET_BUTTON",
			LogType::LOG_SUCCESS);

        return buttonPtr;
    }

    u32 Button::GetID() const { return ID; }
    u32 Button::GetShaderID() const { return shaderID; }
    u32 Button::GetTextureID() const { return textureID; }
    u32 Button::GetMeshID() const { return meshID; }
    u32 Button::GetTextWidgetID() const { return textWidgetID; }

    void Button::Update(
        VkCommandBuffer buffer,
        f64 deltaTime)
    {
        Text* text{};
        string err = Text::GetRegistry().GetContent(textWidgetID, text);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to update button widget '" + to_string(ID) + "' because its text widget '" 
                + to_string(textWidgetID) + "' was invalid! Reason: " + err);
        } 

        Mesh* mesh{};
        err = Mesh::GetRegistry().GetContent(meshID, mesh);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to update button widget '" + to_string(ID) + "' because its mesh '" 
                + to_string(meshID) + "' was invalid! Reason: " + err);
        }        

        text->Update(buffer);
        text->UpdateCursor(deltaTime);

        mesh->Update(buffer);
    }

    void Button::Destroy()
    {
        Text* text{};
        string err = Text::GetRegistry().GetContent(textWidgetID, text);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to destroy button widget '" + to_string(ID) 
                + "' because its text widget was invalid! Reason: " + err);
        }

        Texture* tex{};
        err = Texture::GetRegistry().GetContent(textureID, tex);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to destroy button widget '" + to_string(ID) 
                + "' because its texture was invalid! Reason: " + err);
        }

        Mesh* mesh{};
        err = Mesh::GetRegistry().GetContent(meshID, mesh);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to destroy button widget '" + to_string(ID) 
                + "' because its mesh was invalid! Reason: " + err);
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to destroy button widget '" + to_string(ID) 
                + "' because its shader was invalid! Reason: " + err);
        }

        tex->textWidgetID = 0;
        mesh->textWidgetID = 0;

        text->buttonWidgetID = 0;

        erase(shader->buttonWidgetIDs, ID);

        tex->Destroy();
        mesh->Destroy();

        text->Destroy();

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics button widget error",
                "Failed to destroy button widget! Reason: " + err);
        }
    }

    Button::~Button()
    {
        Log::Print(
            "Destroying button widget '" + to_string(ID) + "'.",
            "KG_WIDGET_BUTTON",
            LogType::LOG_INFO);
    }
}