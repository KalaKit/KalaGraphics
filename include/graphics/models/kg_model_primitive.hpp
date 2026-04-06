//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string_view>

#include "core_utils.hpp"

#include "graphics/models/kg_model.hpp"

namespace KalaGraphics::Graphics
{
    using std::string_view;

    using u8 = uint8_t;

    enum class SphereType : u8
    {
        ST_INVALID = 0u,

        ST_UV,
        ST_QUAD,
        ST_ICO
    };

    struct LIB_API CubeDetails
    {
        //must be atleast 3, increase above 4 for cylinder
        u8 edges = 4;
    };
    struct LIB_API PyramidDetails
    {
        //must be atleast 3, increase above 4 for cone
        u8 edges = 3;
    };
    struct LIB_API SphereDetails
    {
        SphereType type = SphereType::ST_UV;
        //must be atleast 1
        u8 sphereDetailLevel = 1;
    };
    struct LIB_API TorusDetails
    {
        //inner radius must always be smaller than outer radius
        f32 innerRadius = 5.0f;
        //outer radius must always be bigger than inner radius
        f32 outerRadius = 10.0f;
    };

    class LIB_API Model_Primitive : public Model
    {
    public:
        //Create a new cube
        static Model_Primitive* CreatePrimitive(
            string_view modelName,
            u32 contextID,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            CubeDetails cDet);

        //Create a new pyramid
        static Model_Primitive* CreatePrimitive(
            string_view modelName,
            u32 contextID,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            PyramidDetails pDet);

        //Create a new sphere
        static Model_Primitive* CreatePrimitive(
            string_view modelName,
            u32 contextID,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            SphereDetails sDet);

        //Create a new torus
        static Model_Primitive* CreatePrimitive(
            string_view modelName,
            u32 contextID,
            const vec3& pos,
            const vec3& rot,
            const vec3& size,
            TorusDetails tDet);
        
        const CubeDetails& GetCubeDetails() const;
        const PyramidDetails& GetPyramidDetails() const;
        const SphereDetails& GetSphereDetails() const;
        const TorusDetails& GetTorusDetails() const;

        void Update() override;

        ~Model_Primitive();
    private:
        CubeDetails cDet{};
        PyramidDetails pDet{};
        SphereDetails sDet{};
        TorusDetails tDet{};
    };
}