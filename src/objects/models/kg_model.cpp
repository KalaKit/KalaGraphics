//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "objects/models/kg_model.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"
#include "_internal/opengl/_kg_opengl_model.hpp"
#include "_internal/opengl/_kg_opengl_shader.hpp"
#include "objects/models/kg_model_primitive.hpp"

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::WindowContextData;
using KalaGraphics::Internal::OpenGL::OpenGL_Model;
using KalaGraphics::Internal::OpenGL::OpenGL_Shader;
using KalaGraphics::Internal::OpenGL::shader_name;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

static bool ContextExists(u32 contextID)
{
    return WindowContext::GetRegistry().createdContent.contains(contextID);
} 

namespace KalaGraphics::Object
{
    static KalaGraphicsRegistry<Model> registry{};

    KalaGraphicsRegistry<Model>& Model::GetRegistry() { return registry; }

    bool Model::SetName(string_view newName)
    {
        string name = string(newName);

        if (newName.empty())
        {
            Log::Print(
                "Failed to set new model name because it was empty!",
                "MODEL",
                LogType::LOG_ERROR,
                2);

            return false;
        }
        if (newName.size() > 50)
        {
            Log::Print(
                "Failed to set new model name because it was too long!",
                "MODEL",
                LogType::LOG_ERROR,
                2);

            return false;
        }

        modelName = name;

        return true;
    }
    const string& Model::GetName() const { return modelName; }

    vector<Vertex>& Model::GetVertices() { return vertices; }
    vector<u32>& Model::GetIndices() { return indices; }

    u32 Model::GetID() const { return ID; }
    u32 Model::GetContextID() const { return contextID; }

    void Model::SetBackend(
        u32 contextID,
        u32 shaderID,
        u32 backendID,
        BackendType type)
    {
        if (!ContextExists(contextID))
        {
            KalaGraphicsCore::ForceClose(
                "Model backend error",
                "Failed to set backend for model '" + modelName + "' because the passed context ID '" + to_string(contextID) + "' was not found!");

            return;
        }

        WindowContextData& ctx = WindowContext::GetRegistry().GetContent(contextID)->GetWindowContextData();

        if (backendID == 0
            && backendType == BackendType::BT_INVALID)
        {
            if (ctx.context_gl)
            {
                if (shaderID == 0)
                {
                    for (const auto& s : OpenGL_Shader::GetRegistry().runtimeContent)
                    {
                        if (s->GetName() == shader_name)
                        {
                            shaderID = s->GetID();
                            break;
                        }
                    }

                    if (shaderID == 0)
                    {
                        KalaGraphicsCore::ForceClose(
                            "Model backend error",
                            "Tried to assign default primitive shader by name because no shader ID was passed but the shader was not found!!");

                        return;
                    }
                }

                OpenGL_Model* model = OpenGL_Model::InitializeModel(
                    scast<Model_Primitive*>(this),
                    shaderID);
                backendID = model->GetID();
                backendType = BackendType::BT_OPENGL;
            }
            else if (ctx.context_vk_surface)
            {
                KalaGraphicsCore::ForceClose(
                    "Not implemented",
                    "Feature \"Create vulkan model\" is not yet implemented!");

                return;
            }
            else
            {
                KalaGraphicsCore::ForceClose(
                    "Not implemented",
                    "Feature \"Create software model\" is not yet implemented!");

                return;
            }
        }
        else
        {
                KalaGraphicsCore::ForceClose(
                    "Not implemented",
                    "Feature \"Hot-swap model\" is not yet implemented!");

                return;
        }
    }
    u32 Model::GetBackendID() const { return backendID; }
    BackendType Model::GetBackendType() const { return backendType; }

    void Model::SetColor(const vec3& newColor)
    {
        if (newColor.x < 0.0f
            || newColor.y < 0.0f
            || newColor.z < 0.0f)
        {
            Log::Print(
                "Failed to set new model color because one of its values was too low! It must be 0.0f to 1.0f.",
                "MODEL",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (newColor.x > 1.0f
            || newColor.y > 1.0f
            || newColor.z > 1.0f)
        {
            Log::Print(
                "Failed to set new model color because one of its values was too high! It must be 0.0f to 1.0f.",
                "MODEL",
                LogType::LOG_ERROR,
                2);

            return;
        }

        color = newColor;
    }
    const vec3& Model::GetColor() const { return color; }

    Model::~Model()
    {
        
    }
}