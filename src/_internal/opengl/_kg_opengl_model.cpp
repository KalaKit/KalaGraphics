//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "_internal/opengl/_kg_opengl_model.hpp"

namespace KalaGraphics::Internal::OpenGL
{
    static KalaGraphicsRegistry<OpenGL_Model> registry{};

    KalaGraphicsRegistry<OpenGL_Model>& OpenGL_Model::GetRegistry() { return registry; }

    OpenGL_Model* OpenGL_Model::InitializeModel(Model_Primitive* data)
    {

    }

    OpenGL_Model* OpenGL_Model::InitializeModel(Model_Standard* data)
    {

    }
}