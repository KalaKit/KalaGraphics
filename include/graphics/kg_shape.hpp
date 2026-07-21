//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"
#include "resources/kg_mesh.hpp"

namespace KalaGraphics::Graphics
{
    using KalaHeaders::KalaMath::Transform3D;

    using KalaGraphics::Core::KalaGraphicsRegistry;
    using KalaGraphics::Resources::Transform;

    using std::string;
    using std::string_view;
    using std::vector;

    using u32 = uint32_t;

    struct LIB_API Material
    {
        //do we want to render this mesh at all
        bool isEnabled = true;

        u32 meshID{};
        u32 shaderID{};
        vector<u32> textureIDs{};
    };

    class LIB_API Shape
    {
    public:
        static KalaGraphicsRegistry<Shape>& GetRegistry();

        //Create a shape with meshes, shaders and textures
        static Shape* Initialize(
            string_view name,
            u32 contextID,
            const vector<Material>& materials,
            const Transform& transform);

        u32 GetID() const;
        const vector<Material>& GetMaterials() const;

        u32 GetContextID() const;
        void SetContextID(u32 newValue);

        const string& GetName() const;
        void SetName(string_view newName);

        //Is this shape renderable at all
        bool IsEnabled() const;
        void SetEnabledstate(bool state);

        //Is this material and its assigned mesh renderable at all
        bool IsMaterialEnabled(u32 materialSlot) const;
        void SetMaterialEnabledState(u32 materialSlot);

        void Destroy();

        ~Shape();
    private:
        u32 ID{};
        u32 contextID{};
        vector<Material> materials{};

        Transform3D transform{};

        bool isEnabled = true;

        string name{};
    };
}