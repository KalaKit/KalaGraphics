//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string_view>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"
#include "objects/models/kg_model_primitive.hpp"
#include "objects/models/kg_model_standard.hpp"

namespace KalaGraphics::Internal::OpenGL
{
    using KalaGraphics::Core::KalaGraphicsRegistry;
    using KalaGraphics::Object::Model_Primitive;
    using KalaGraphics::Object::Model_Standard;

    using std::string_view;

    using u32 = uint32_t;

    constexpr string_view shader_name = "gl_basic";

    class LIB_API OpenGL_Model
    {
    public:
        static KalaGraphicsRegistry<OpenGL_Model>& GetRegistry();

        static OpenGL_Model* InitializeModel(
            Model_Primitive* data,
            u32 shaderID);
        static OpenGL_Model* InitializeModel(
            Model_Standard* data,
            u32 shaderID);

        u32 GetID() const;
        u32 GetModel() const;
        u32 GetShaderID() const;

        u32 GetVAO() const;
        u32 GetVBO() const;
        u32 GetEBO() const;

        void Update();

        ~OpenGL_Model();
    private:
        u32 ID{};
        u32 modelID{};
        u32 shaderID{};

        u32 VAO{};
        u32 VBO{};
        u32 EBO{};
    };
}