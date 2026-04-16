//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "graphics/models/kg_model.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"
#include "graphics/models/kg_model_primitive.hpp"

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::WindowContext;
using KalaGraphics::Core::WindowContextData;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

static bool ContextExists(u32 contextID)
{
    return WindowContext::GetRegistry().createdContent.contains(contextID);
} 

namespace KalaGraphics::Graphics
{
    static KalaGraphicsRegistry<Model> registry{};

    KalaGraphicsRegistry<Model>& Model::GetRegistry() { return registry; }

    bool Model::SetName(string_view newName)
    {
        if (newName.empty())
        {
            Log::Print(
                "Failed to set new model name because it was empty!",
                "KG_MODEL",
                LogType::LOG_ERROR,
                2);

            return false;
        }
        if (newName.size() > 50)
        {
            Log::Print(
                "Failed to set new model name because it was too long!",
                "KG_MODEL",
                LogType::LOG_ERROR,
                2);

            return false;
        }

        name = newName;

        return true;
    }
    const string& Model::GetName() const { return name; }

    vector<Vertex>& Model::GetVertices() { return vertices; }
    vector<u32>& Model::GetIndices() { return indices; }

    u32 Model::GetID() const { return ID; }
    u32 Model::GetContextID() const { return contextID; }

    void Model::SetBackend(
        u32 contextID,
        u32 shaderID,
        u32 backendID)
    {
        if (!ContextExists(contextID))
        {
            KalaGraphicsCore::ForceClose(
                "Model backend error",
                "Failed to set backend for model '" + name + "' because the passed context ID '" + to_string(contextID) + "' was not found!");

            return;
        }

        WindowContextData& ctx = WindowContext::GetRegistry().GetContent(contextID)->GetWindowContextData();

        //set vulkan backend here
    }
    u32 Model::GetBackendID() const { return backendID; }

    void Model::SetColor(const vec3& newColor)
    {
        if (newColor.x < 0.0f
            || newColor.y < 0.0f
            || newColor.z < 0.0f)
        {
            Log::Print(
                "Failed to set new model color because one of its values was too low! It must be 0.0f to 1.0f.",
                "KG_MODEL",
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
                "KG_MODEL",
                LogType::LOG_ERROR,
                2);

            return;
        }

        color = newColor;
    }
    const vec3& Model::GetColor() const { return color; }
}