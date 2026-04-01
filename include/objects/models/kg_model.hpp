//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <string>

#include "core_utils.hpp"
#include "math_utils.hpp"
#include "import_kmd.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Object
{
    using KalaHeaders::KalaMath::vec3;
    using KalaHeaders::KalaMath::vec4;
    using KalaHeaders::KalaMath::Transform3D;

    using KalaHeaders::KalaModelData::Vertex;

    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::vector;
    using std::string;
    using std::string_view;

    using u32 = uint32_t;

    enum class BackendType : u8
    {
        BT_INVALID = 0u,

        BT_SOFTWARE = 1u,
        BT_OPENGL = 2u,
        BT_VULKAN = 3u
    };

    class LIB_API Model
    {
    public:
        static KalaGraphicsRegistry<Model>& GetRegistry();

        bool IsInitialized() const;

        bool SetName(string_view newName);
        const string& GetName() const;

        vector<Vertex>& GetVertices();
        vector<u32>& GetIndices();

        u32 GetID() const;
        u32 GetContextID() const;

        //Hot-swap context and backend at runtime,
        //warning: invalid backend will NOT work and will cause a force-close if abused.
        //Leave shaderID, backendID and type unassigned if you want a new backend for this model
        void SetBackend(
            u32 contextID, 
            u32 shaderID = 0,
            u32 backendID = 0, 
            BackendType type = BackendType::BT_INVALID);
        u32 GetBackendID() const;
        BackendType GetBackendType() const;

        void SetColor(const vec3& newColor);
        const vec3& GetColor() const;

        //Single draw call
        virtual void Update() = 0;

        ~Model();
    protected:
        bool isInitialized{};

        string modelName{};

        Transform3D transform{};

        u32 ID{};

        u32 contextID{};
        u32 backendID{};
        BackendType backendType{};

		vector<Vertex> vertices{};
		vector<u32> indices{};

        vec3 color{};
    };
}