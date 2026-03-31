//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "objects/models/kg_model.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"
#include "_internal/opengl/_kg_opengl_model.hpp"

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::WindowContextData;
using KalaGraphics::Internal::OpenGL::OpenGL_Model;

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
                
            }
            else if (ctx.context_vk_surface)
            {

            }
            else
            {

            }
        }
        else
        {

        }
    }
    u32 Model::GetBackendID() const { return backendID; }
    BackendType Model::GetBackendType() const { return backendType; }
}