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
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::GraphicsContextData;
using KalaGraphics::Core::MAX_NAME_LENGTH;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

static bool ContextExists(u32 contextID)
{
    return GraphicsContext::GetRegistry().createdContent.contains(contextID);
} 

namespace KalaGraphics::Graphics
{
    static KalaGraphicsRegistry<Model> registry{};

    KalaGraphicsRegistry<Model>& Model::GetRegistry() { return registry; }

    void Model::SetName(string_view newName)
    {
        if (newName.empty())
        {
            Log::Print(
                "Failed to set new model name because it was empty!",
                "KG_MODEL",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (newName.size() > MAX_NAME_LENGTH)
        {
            Log::Print(
                "Failed to set new model name because it was too long!",
                "KG_MODEL",
                LogType::LOG_ERROR,
                2);

            return;
        }

        name = newName;
    }
    const string& Model::GetName() const { return name; }

    vector<Vertex>& Model::GetVertices() { return vertices; }
    vector<u32>& Model::GetIndices() { return indices; }

    u32 Model::GetID() const { return ID; }
    u32 Model::GetGraphicsContextID() const { return contextID; }
    u32 Model::GetVulkanContextID() const { return vulkanID; }
    u32 Model::GetShaderID() const { return shaderID; }

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