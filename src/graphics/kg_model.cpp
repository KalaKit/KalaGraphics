//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "graphics/kg_model.hpp"
#include "core/kg_core.hpp"

using KalaGraphics::Core::MAX_NAME_LENGTH;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::to_string;

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

    void Model::Destroy()
    {
        registry.RemoveContent(ID);
    }

    Model::~Model()
    {
        Log::Print(
            "Destroying model '" + to_string(ID) + "'.",
            "KG_MODEL",
            LogType::LOG_INFO);
    }
}