//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "import_kmd.hpp"
#include "log_utils.hpp"

#include "_internal/opengl/_kg_opengl_model.hpp"
#include "_internal/opengl/_kg_opengl.hpp"
#include "_internal/opengl/_kg_opengl_shader.hpp"
#include "core/kg_core.hpp"
#include "graphics/models/kg_model.hpp"

using KalaHeaders::KalaModelData::Vertex;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaGraphics::Internal::OpenGL::OpenGL_Core;
using KalaGraphics::Internal::OpenGL::OpenGL_Core_Functions;
using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Graphics::Model;

using std::unique_ptr;
using std::make_unique;
using std::to_string;

namespace KalaGraphics::Internal::OpenGL
{
    static KalaGraphicsRegistry<OpenGL_Model> registry{};

    KalaGraphicsRegistry<OpenGL_Model>& OpenGL_Model::GetRegistry() { return registry; }

    OpenGL_Model* OpenGL_Model::InitializeModel(
        Model_Primitive* data,
        u32 shaderID)
    {
        auto* shader = OpenGL_Shader::GetRegistry().GetContent(shaderID);
        if (!shader)
        {
            KalaGraphicsCore::ForceClose(
                "OpenGL model init error",
                "Failed to initialize OpenGL model because the shader ID was not found!");

            return nullptr;
        }

        unique_ptr<OpenGL_Model> newModel = make_unique<OpenGL_Model>();
        OpenGL_Model* modelPtr = newModel.get();

        const OpenGL_Core_Functions& coreFunc = OpenGL_Core::GetCoreFunctions();

        coreFunc.glGenVertexArrays(1, &modelPtr->VAO);
        coreFunc.glGenBuffers(1, &modelPtr->VBO);
        coreFunc.glGenBuffers(1, &modelPtr->EBO);

        coreFunc.glBindVertexArray(modelPtr->VAO);

        coreFunc.glBindBuffer(
            GL_ARRAY_BUFFER, 
            modelPtr->VBO);
        coreFunc.glBufferData(
            GL_ARRAY_BUFFER, 
            data->GetVertices().size() * sizeof(Vertex),
            data->GetVertices().data(),
            GL_STATIC_DRAW);

        coreFunc.glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, 
            modelPtr->EBO);
        coreFunc.glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 
            data->GetIndices().size() * sizeof(u32),
            data->GetIndices().data(),
            GL_STATIC_DRAW);

        coreFunc.glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, position));
        coreFunc.glEnableVertexAttribArray(0);

        coreFunc.glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, normal));
        coreFunc.glEnableVertexAttribArray(1);

        coreFunc.glVertexAttribPointer(
            2,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, texCoord));
        coreFunc.glEnableVertexAttribArray(2);

        coreFunc.glVertexAttribPointer(
            3,
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, tangent));
        coreFunc.glEnableVertexAttribArray(3);

        coreFunc.glBindVertexArray(0);

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        modelPtr->ID = newID;

        registry.AddContent(newID, std::move(newModel));

        Log::Print(
            "Created new OpenGL model with ID '" + to_string(newID) + "'!",
            "KG_GL_MODEL",
            LogType::LOG_SUCCESS);

        return modelPtr;
    }

    OpenGL_Model* OpenGL_Model::InitializeModel(
        Model_Standard* data,
        u32 shaderID)
    {

    }

    u32 OpenGL_Model::GetID() const { return ID; }
    u32 OpenGL_Model::GetModel() const { return modelID; }

    u32 OpenGL_Model::GetVAO() const { return VAO; }
    u32 OpenGL_Model::GetVBO() const { return VBO; }
    u32 OpenGL_Model::GetEBO() const { return EBO; }

    void OpenGL_Model::Update()
    {
        auto* shader = OpenGL_Shader::GetRegistry().GetContent(shaderID);
        if (!shader)
        {
            KalaGraphicsCore::ForceClose(
                "OpenGL model init error",
                "Failed to update OpenGL model because the shader ID was not found!");

            return;
        }

        auto* model = Model::GetRegistry().GetContent(modelID);
        if (!model)
        {
            KalaGraphicsCore::ForceClose(
                "OpenGL model render error",
                "Failed to update OpenGL model because the model ID was not found!");

            return;
        }

        shader->Bind();
        shader->SetVec3("u_Color", model->GetColor());

        const OpenGL_Core_Functions& coreFunc = OpenGL_Core::GetCoreFunctions();

        coreFunc.glBindVertexArray(VAO);
        coreFunc.glDrawElements(
            GL_TRIANGLES, 
            model->GetIndices().size(),
            GL_UNSIGNED_INT,
            0);
        coreFunc.glBindVertexArray(0);
    }

    OpenGL_Model::~OpenGL_Model()
    {
       Log::Print(
            "Destroying OpenGL model with ID '" + to_string(ID) + "'.",
            "KG_GL_MODEL",
            LogType::LOG_INFO);

        const OpenGL_Core_Functions& coreFunc = OpenGL_Core::GetCoreFunctions();

        if (VAO) coreFunc.glDeleteVertexArrays(1, &VAO);
        if (VBO) coreFunc.glDeleteBuffers(1, &VBO);
        if (EBO) coreFunc.glDeleteBuffers(1, &EBO);
    }
}