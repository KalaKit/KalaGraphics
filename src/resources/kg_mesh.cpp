//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "core/kg_core.hpp"

#include "vulkan/vulkan_core.h"
KG_VK_MEM_ALLOC_IGNORE_PUSH
#include "vma/vk_mem_alloc.h"
KG_VK_MEM_ALLOC_IGNORE_POP

#include "log_utils.hpp"

#include "resources/kg_mesh.hpp"
#include "core/kg_context.hpp"
#include "core/kg_viewport.hpp"
#include "core/kg_hit_test.hpp"
#include "core/kg_shader.hpp"
#include "resources/kg_texture.hpp"
#include "resources/kg_camera.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::isnear;
using KalaHeaders::KalaMath::mat4;
using KalaHeaders::KalaMath::PI64;
using KalaHeaders::KalaMath::PosTarget;
using KalaHeaders::KalaMath::RotTarget;
using KalaHeaders::KalaMath::SizeTarget;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::Viewport;
using KalaGraphics::Core::HitTest;
using KalaGraphics::Core::Shader;

using std::to_string;
using std::unique_ptr;
using std::make_unique;
using std::swap;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Mesh> registry{};

    KalaGraphicsRegistry<Mesh>& Mesh::GetRegistry() { return registry; }

    MeshData Mesh::GenerateMeshData(Mesh_Cube cubeData)
    {
        if (cubeData.edgeCount < MIN_CUBE_EDGE_COUNT)
        {
            Log::Print(
                "Cube or cylinder edge count cannot be less than " 
                + to_string(MIN_CUBE_EDGE_COUNT) + "! Value was clamped.",
                "KG_MESH",
                LogType::LOG_WARNING);

            cubeData.edgeCount = MIN_CUBE_EDGE_COUNT;
        }
        if (cubeData.edgeCount > MAX_CUBE_EDGE_COUNT)
        {
            Log::Print(
                "Cube or cylinder edge count cannot be more than " 
                + to_string(MAX_CUBE_EDGE_COUNT) + "! Value was clamped.",
                "KG_MESH",
                LogType::LOG_WARNING);

            cubeData.edgeCount = MAX_CUBE_EDGE_COUNT;
        }

        MeshData data{};

        const f64 top = 0.5f;
        const f64 bottom = -0.5f;

        const f64 angleStep = 
            (2.0 * PI64) 
            / scast<f64>(cubeData.edgeCount);

        //rotate by half an edge so edgeCount 4 produces
        //faces aligned with the X and Z axes
        const f64 angleOffset = angleStep * 0.5f;

        const f64 halfExtent = 0.5;
        const f64 radius = cubeData.edgeCount == 3
            ? halfExtent
            : halfExtent / cos(angleStep * 0.5);

        //
        // SIDES
        //

        //each side needs its own vertices
        //because adjacent faces have different normals
        if (cubeData.normalType == NormalType::N_FLAT)
        {
            for (u32 i = 0; i < cubeData.edgeCount; ++i)
            {
                const f64 angle0 = angleOffset + angleStep * i;
                const f64 angle1 = angleOffset + angleStep * (i + 1);

                const vec3 bottom0 =
                {
                    scast<f32>(cos(angle0) * radius),
                    scast<f32>(bottom),
                    scast<f32>(sin(angle0) * radius)
                };

                const vec3 bottom1 =
                {
                    scast<f32>(cos(angle1) * radius),
                    scast<f32>(bottom),
                    scast<f32>(sin(angle1) * radius)
                };

                const vec3 top0 = 
                {
                    bottom0.x,
                    scast<f32>(top),
                    bottom0.z
                };

                const vec3 top1 = 
                {
                    bottom1.x,
                    scast<f32>(top),
                    bottom1.z
                };

                //normal points outward through the center of the face
                const f64 middleAngle = (angle0 + angle1) * 0.5;

                vec3 normal =
                {
                    scast<f32>(cos(middleAngle)),
                    0.0f,
                    scast<f32>(sin(middleAngle))
                };

                if (cubeData.faceDir == FaceDirection::F_IN)
                {
                    normal = -normal;
                }

                const u32 index = scast<u32>(data.vertices.size());

                data.vertices.push_back(
                {
                    .pos = bottom0,
                    .normal = normal
                });

                data.vertices.push_back(
                {
                    .pos = bottom1,
                    .normal = normal,
                    .uv = { 1.0f, 0.0f }
                });

                data.vertices.push_back(
                {
                    .pos = top1,
                    .normal = normal,
                    .uv = 1.0f
                });

                data.vertices.push_back(
                {
                    .pos = top0,
                    .normal = normal,
                    .uv = { 0.0f, 1.0f }
                });

                if (cubeData.faceDir == FaceDirection::F_OUT)
                {
                    data.indices.insert(
                        data.indices.end(),
                        {
                            index,
                            index + 1,
                            index + 2,

                            index,
                            index + 2,
                            index + 3
                        });
                }
                else
                {
                    data.indices.insert(
                        data.indices.end(),
                        {
                            index,
                            index + 2,
                            index + 1,

                            index,
                            index + 3,
                            index + 2
                        });
                }
            }
        }
        //smooth sides share normals around the circumference.
        //the seam is duplicate because it needs both U=0 and U=1
        else
        {
            for (u32 i = 0; i <= cubeData.edgeCount; ++i)
            {
                const f64 angle = angleOffset + angleStep * i;

                vec3 normal =
                {
                    scast<f32>(cos(angle)),
                    0.0f,
                    scast<f32>(sin(angle))
                };
                
                if (cubeData.faceDir == FaceDirection::F_IN)
                {
                    normal = -normal;
                }

                const vec3 bottomPos =
                {
                    scast<f32>(cos(angle) * radius),
                    scast<f32>(bottom),
                    scast<f32>(sin(angle) * radius)
                };

                const vec3 topPos = 
                {
                    bottomPos.x,
                    scast<f32>(top),
                    bottomPos.z
                };

                const f32 u = scast<f32>(
                    scast<f64>(i) 
                    / cubeData.edgeCount);

                data.vertices.push_back(
                {
                   .pos = bottomPos,
                   .normal = normal,
                   .uv = { u, 0.0f } 
                });

                data.vertices.push_back(
                {
                   .pos = topPos,
                   .normal = normal,
                   .uv = { u, 1.0f } 
                });
            }

            for (u32 i = 0; i < cubeData.edgeCount; ++i)
            {
                const u32 index = i * 2;

                if (cubeData.faceDir == FaceDirection::F_OUT)
                {
                    data.indices.insert(
                        data.indices.end(), 
                        {
                            index,
                            index + 2,
                            index + 3,

                            index,
                            index + 3,
                            index + 1
                        });
                }
                else
                {
                    data.indices.insert(
                        data.indices.end(), 
                        {
                            index,
                            index + 3,
                            index + 2,

                            index,
                            index + 1,
                            index + 3
                        });
                }
            }
        }

        //
        // TOP CAP
        //

        //generate top cap
        const u32 topCenter = scast<u32>(data.vertices.size());

        vec3 topNormal = { 0.0f, 1.0f, 0.0f };
        if (cubeData.faceDir == FaceDirection::F_IN)
        {
            topNormal = -topNormal;
        }

        data.vertices.push_back(
        {
            .pos = { 0.0f, scast<f32>(top), 0.0f },
            .normal = topNormal,
            .uv = { 0.5f, 0.5f }
        });

        for (u32 i = 0; i < cubeData.edgeCount; ++i)
        {
            const f64 angle = angleOffset + angleStep * i;

            const vec3 pos = 
            {
                scast<f32>(cos(angle) * radius),
                scast<f32>(top),
                scast<f32>(sin(angle) * radius)
            };

            //planar projection onto the XZ plane
            const vec2 uv =
            {
                (pos.x / scast<f32>(radius) + 1.0f) * 0.5f,
                (pos.z / scast<f32>(radius) + 1.0f) * 0.5f
            };

            data.vertices.push_back(
            {
               .pos = pos,
               .normal = topNormal,
               .uv = uv 
            });
        }

        for (u32 i = 0; i < cubeData.edgeCount; ++i)
        {
            const u32 current = topCenter + 1 + i;
            const u32 next = 
                topCenter 
                + 1 
                + ((i + 1) % cubeData.edgeCount);

            if (cubeData.faceDir == FaceDirection::F_OUT)
            {
                data.indices.insert(
                    data.indices.end(), 
                    {
                        topCenter,
                        current,
                        next
                    });
            }
            else
            {
                data.indices.insert(
                    data.indices.end(), 
                    {
                        topCenter,
                        next,
                        current
                    });
            }
        }

        //
        // BOTTOM CAP
        //

        //generate bottom cap
        const u32 bottomCenter = scast<u32>(data.vertices.size());

        vec3 bottomNormal = { 0.0f, -1.0f, 0.0f };
        if (cubeData.faceDir == FaceDirection::F_IN)
        {
            bottomNormal = -bottomNormal;
        }

        data.vertices.push_back(
        {
            .pos = { 0.0f, scast<f32>(bottom), 0.0f },
            .normal = bottomNormal,
            .uv = { 0.5f, 0.5f }
        });

        for (u32 i = 0; i < cubeData.edgeCount; ++i)
        {
            const f64 angle = angleOffset + angleStep * i;

            const vec3 pos = 
            {
                scast<f32>(cos(angle) * radius),
                scast<f32>(bottom),
                scast<f32>(sin(angle) * radius)
            };

            //planar projection onto the XZ plane
            const vec2 uv =
            {
                (pos.x / scast<f32>(radius) + 1.0f) * 0.5f,
                (pos.z / scast<f32>(radius) + 1.0f) * 0.5f
            };

            data.vertices.push_back(
            {
               .pos = pos,
               .normal = bottomNormal,
               .uv = uv 
            });
        }

        for (u32 i = 0; i < cubeData.edgeCount; ++i)
        {
            const u32 current = bottomCenter + 1 + i;
            const u32 next = 
                bottomCenter 
                + 1 
                + ((i + 1) % cubeData.edgeCount);

            if (cubeData.faceDir == FaceDirection::F_OUT)
            {
                data.indices.insert(
                    data.indices.end(), 
                    {
                        bottomCenter,
                        next,
                        current
                    });
            }
            else
            {
                data.indices.insert(
                    data.indices.end(), 
                    {
                        bottomCenter,
                        current,
                        next
                    });
            }
        }

        return data;
    }
    MeshData Mesh::GenerateMeshData(Mesh_Pyramid pyramidData)
    {
        if (pyramidData.edgeCount < MIN_PYRAMID_EDGE_COUNT)
        {
            Log::Print(
                "Pyramid or cone edge count cannot be less than " 
                + to_string(MIN_PYRAMID_EDGE_COUNT) + "! Value was clamped.",
                "KG_MESH",
                LogType::LOG_WARNING);

            pyramidData.edgeCount = MIN_PYRAMID_EDGE_COUNT;
        }
        if (pyramidData.edgeCount > MAX_PYRAMID_EDGE_COUNT)
        {
            Log::Print(
                "Pyramid or cone edge count cannot be more than " 
                + to_string(MAX_PYRAMID_EDGE_COUNT) + "! Value was clamped.",
                "KG_MESH",
                LogType::LOG_WARNING);

            pyramidData.edgeCount = MAX_PYRAMID_EDGE_COUNT;
        }

        MeshData data{};

        const f64 top = 0.5;
        const f64 bottom = -0.5;
        const f64 height = top - bottom;

        const f64 angleStep = 
            (2.0 * PI64)
            / scast<f64>(pyramidData.edgeCount);

        //rotate by half an edge so the base follows the same
        //orientation convention as cube/cylinder generation
        const f64 angleOffset = angleStep * 0.5;

        const f64 halfExtent = 0.5;
        const f64 radius = pyramidData.edgeCount == 3
            ? halfExtent
            : halfExtent / cos(angleStep * 0.5);

        //
        // SIDES
        //

        //each side needs its own vertices
        //because adjacent faces have different normals
        if (pyramidData.normalType == NormalType::N_FLAT)
        {
            const f64 apothem = radius * cos(angleStep * 0.5);

            for (u32 i = 0; i < pyramidData.edgeCount; ++i)
            {
                const f64 angle0 = angleOffset + angleStep * i;
                const f64 angle1 = angleOffset + angleStep * (i + 1);

                const vec3 bottom0 =
                {
                    scast<f32>(cos(angle0) * radius),
                    scast<f32>(bottom),
                    scast<f32>(sin(angle0) * radius)
                };

                const vec3 bottom1 =
                {
                    scast<f32>(cos(angle1) * radius),
                    scast<f32>(bottom),
                    scast<f32>(sin(angle1) * radius)
                };

                const vec3 tip = 
                {
                    0.0f,
                    scast<f32>(top),
                    0.0f
                };

                //normal points outward through the center of the face
                const f64 middleAngle = (angle0 + angle1) * 0.5f;

                const f64 normalX = height * cos(middleAngle);
                const f64 normalY = apothem;
                const f64 normalZ = height * sin(middleAngle);

                const f64 normalLength = sqrt(
                    normalX * normalX
                    + normalY * normalY
                    + normalZ * normalZ);

                vec3 normal =
                {
                    scast<f32>(normalX / normalLength),
                    scast<f32>(normalY / normalLength),
                    scast<f32>(normalZ / normalLength)
                };

                if (pyramidData.faceDir == FaceDirection::F_IN)
                {
                    normal = -normal;
                }

                const u32 index = scast<u32>(data.vertices.size());

                data.vertices.push_back(
                {
                    .pos = bottom0,
                    .normal = normal
                });

                data.vertices.push_back(
                {
                    .pos = bottom1,
                    .normal = normal,
                    .uv = { 1.0f, 0.0f }
                });

                data.vertices.push_back(
                {
                    .pos = tip,
                    .normal = normal,
                    .uv = { 0.5f, 1.0f }
                });

                if (pyramidData.faceDir == FaceDirection::F_OUT)
                {
                    data.indices.insert(
                        data.indices.end(), 
                        {
                            index,
                            index + 1,
                            index + 2
                        });
                }
                else
                {
                    data.indices.insert(
                        data.indices.end(), 
                        {
                            index,
                            index + 2,
                            index + 1
                        });
                }
            }
        }
        //smooth sides share normals around the circumference.
        //the seam is duplicate because it needs both U=0 and U=1
        else
        {
            for (u32 i = 0; i <= pyramidData.edgeCount; ++i)
            {
                const f64 angle = angleOffset + angleStep * i;

                const f64 normalX = height * cos(angle);
                const f64 normalY = radius;
                const f64 normalZ = height * sin(angle);

                const f64 normalLength = sqrt(
                    normalX * normalX
                    + normalY * normalY
                    + normalZ * normalZ);

                vec3 normal =
                {
                    scast<f32>(normalX / normalLength),
                    scast<f32>(normalY / normalLength),
                    scast<f32>(normalZ / normalLength)
                };

                if (pyramidData.faceDir == FaceDirection::F_IN)
                {
                    normal = -normal;
                }

                const vec3 bottomPos = 
                {
                    scast<f32>(cos(angle) * radius),
                    scast<f32>(bottom),
                    scast<f32>(sin(angle) * radius)
                };

                const vec3 tipPos =
                {
                    0.0f,
                    scast<f32>(top),
                    0.0f
                };

                const f32 u = scast<f32>(
                    scast<f64>(i)
                    / pyramidData.edgeCount);

                data.vertices.push_back(
                {
                   .pos = bottomPos,
                   .normal = normal,
                   .uv = { u, 0.0f } 
                });

                //the tip is duplicated so each section can carry
                //its corresponding smooth normal and UV
                data.vertices.push_back(
                {
                   .pos = tipPos,
                   .normal = normal,
                   .uv = { u, 1.0f } 
                });
            }

            for (u32 i = 0; i < pyramidData.edgeCount; ++i)
            {
                const u32 index = i * 2;

                if (pyramidData.faceDir == FaceDirection::F_OUT)
                {
                    data.indices.insert(
                        data.indices.end(), 
                        {
                            index,
                            index + 2,
                            index + 1
                        });
                }
                else
                {
                    data.indices.insert(
                        data.indices.end(), 
                        {
                            index,
                            index + 1,
                            index + 2
                        });
                }
            }
        }

        //
        // BOTTOM CAP
        //

        const u32 bottomCenter = scast<u32>(data.vertices.size());

        vec3 bottomNormal = { 0.0f, -1.0f, 0.0f };

        if (pyramidData.faceDir == FaceDirection::F_IN)
        {
            bottomNormal = -bottomNormal;
        }

        data.vertices.push_back(
        {
           .pos = { 0.0f, scast<f32>(bottom), 0.0f },
           .normal = bottomNormal,
           .uv = { 0.5f, 0.5f } 
        });

        for (u32 i = 0; i < pyramidData.edgeCount; ++i)
        {
            const f64 angle = angleOffset + angleStep * i;

            const vec3 pos =
            {
                scast<f32>(cos(angle) * radius),
                scast<f32>(bottom),
                scast<f32>(sin(angle) * radius)
            };

            //planar projection onto the XZ plane
            const vec2 uv =
            {
                (pos.x / scast<f32>(radius) + 1.0f) * 0.5f,
                (pos.z / scast<f32>(radius) + 1.0f) * 0.5f
            };

            data.vertices.push_back(
            {
                .pos = pos,
                .normal = bottomNormal,
                .uv = uv
            });
        }

        for (u32 i = 0; i < pyramidData.edgeCount; ++i)
        {
            const u32 current = bottomCenter + 1 + i;
            const u32 next =
                bottomCenter
                + 1
                + ((i + 1) % pyramidData.edgeCount);

            if (pyramidData.faceDir == FaceDirection::F_OUT)
            {
                data.indices.insert(
                    data.indices.end(), 
                    {
                        bottomCenter,
                        next,
                        current
                    });
            }
            else
            {
                data.indices.insert(
                    data.indices.end(), 
                    {
                        bottomCenter,
                        current,
                        next
                    });
            }
        }

        return data;
    }
    MeshData Mesh::GenerateMeshData(Mesh_Sphere sphereData)
    {
        if (sphereData.detailLevel < MIN_SPHERE_DETAIL_LEVEL)
        {
            Log::Print(
                "Sphere detail level cannot be less than " 
                + to_string(MIN_SPHERE_DETAIL_LEVEL) + "! Value was clamped.",
                "KG_MESH",
                LogType::LOG_WARNING);

            sphereData.detailLevel = MIN_SPHERE_DETAIL_LEVEL;
        }
        if (sphereData.detailLevel > MAX_SPHERE_DETAIL_LEVEL)
        {
            Log::Print(
                "Sphere detail level cannot be more than " 
                + to_string(MAX_SPHERE_DETAIL_LEVEL) + "! Value was clamped.",
                "KG_MESH",
                LogType::LOG_WARNING);

            sphereData.detailLevel = MAX_SPHERE_DETAIL_LEVEL;
        }

        MeshData data{};

        const f64 radius = 0.5;

        const u32 latitudeCount = 4 * scast<u32>(sphereData.detailLevel);
        const u32 longitudeCount = 8 * scast<u32>(sphereData.detailLevel);

        const f64 latitudeStep = PI64 / scast<f64>(latitudeCount);
        const f64 longitudeStep =
            (2.0 * PI64)
            / scast<f64>(longitudeCount);

        if (sphereData.normalType == NormalType::N_FLAT)
        {
            auto get_position = [&](u32 latitude, u32 longitude) -> vec3
                {
                    const f64 latitudeAngle = 
                        PI64 * 0.5
                        - latitudeStep * latitude;

                    const f64 longitudeAngle = longitudeStep * longitude;

                    const f64 ringRadius = cos(latitudeAngle) * radius;

                    return
                    {
                        scast<f32>(cos(longitudeAngle) * ringRadius),
                        scast<f32>(sin(latitudeAngle) * radius),
                        scast<f32>(sin(longitudeAngle) * ringRadius)
                    };
                };

            auto get_uv = [&](u32 latitude, u32 longitude) -> vec2
                {
                    return
                    {
                        scast<f32>(scast<f64>(longitude) / longitudeCount),
                        scast<f32>(scast<f64>(latitude) / latitudeCount)
                    };
                };

            auto add_triangle = [&](
                const vec3& pos0,
                const vec3& pos1,
                const vec3& pos2,
                const vec2& uv0,
                const vec2& uv1,
                const vec2& uv2) -> void
                {
                    const vec3 edge0 = pos1 - pos0;
                    const vec3 edge1 = pos2 - pos0;

                    vec3 normal = 
                    {
                        edge0.y * edge1.z - edge0.z * edge1.y,
                        edge0.z * edge1.x - edge0.x * edge1.z,
                        edge0.x * edge1.y - edge0.y * edge1.x
                    };

                    const f64 normalLength = sqrt(
                        scast<f64>(normal.x) * normal.x
                        + scast<f64>(normal.y) * normal.y
                        + scast<f64>(normal.z) * normal.z);

                    normal = 
                    {
                        scast<f32>(normal.x / normalLength),
                        scast<f32>(normal.y / normalLength),
                        scast<f32>(normal.z / normalLength)
                    };

                    if (sphereData.faceDir == FaceDirection::F_IN)
                    {
                        normal = -normal;
                    }

                    const u32 index = scast<u32>(data.vertices.size());

                    data.vertices.push_back(
                    {
                        .pos = pos0,
                        .normal = normal,
                        .uv = uv0 
                    });

                    data.vertices.push_back(
                    {
                        .pos = pos1,
                        .normal = normal,
                        .uv = uv1 
                    });

                    data.vertices.push_back(
                    {
                        .pos = pos2,
                        .normal = normal,
                        .uv = uv2 
                    });

                    if (sphereData.faceDir == FaceDirection::F_OUT)
                    {
                        data.indices.insert(
                            data.indices.end(), 
                            {
                                index,
                                index + 1,
                                index + 2
                            });
                    }
                    else
                    {
                        data.indices.insert(
                            data.indices.end(), 
                            {
                                index,
                                index + 2,
                                index + 1
                            });
                    }
                };

            for (u32 latitude = 0; latitude < latitudeCount; ++latitude)
            {
                for (u32 longitude = 0; longitude < longitudeCount; ++longitude)
                {
                    const vec3 topLeft     = get_position(latitude, longitude);
                    const vec3 topRight    = get_position(latitude, longitude + 1);
                    const vec3 bottomLeft  = get_position(latitude + 1, longitude);
                    const vec3 bottomRight = get_position(latitude + 1, longitude + 1);

                    const vec2 uvTopLeft     = get_uv(latitude, longitude);
                    const vec2 uvTopRight    = get_uv(latitude, longitude + 1);
                    const vec2 uvBottomLeft  = get_uv(latitude + 1, longitude);
                    const vec2 uvBottomRight = get_uv(latitude + 1, longitude + 1);

                    //top pole only needs one triangle
                    if (latitude != 0)
                    {
                        add_triangle(
                            topLeft,
                            bottomLeft,
                            topRight,
                            uvTopLeft,
                            uvBottomLeft,
                            uvTopRight);
                    }

                    //bottom pole only needs one triangle
                    if (latitude != latitudeCount - 1)
                    {
                        add_triangle(
                            topRight,
                            bottomLeft,
                            bottomRight,
                            uvTopRight,
                            uvBottomLeft,
                            uvBottomRight);
                    }
                }
            }
        }
        else
        {
            //duplicate the longitude seam because U needs both 0 and 1
            for (u32 latitude = 0; latitude <= latitudeCount; ++latitude)
            {
                const f64 latitudeAngle = 
                    PI64 * 0.5 
                    - latitudeStep * latitude;

                const f64 ringRadius = cos(latitudeAngle) * radius;

                const f64 y = sin(latitudeAngle) * radius;
                
                const f32 v = scast<f32>(scast<f64>(latitude) / latitudeCount);

                for (u32 longitude = 0; longitude <= longitudeCount; ++longitude)
                {
                    const f64 longitudeAngle = longitudeStep * longitude;

                    const f64 x = cos(longitudeAngle) * ringRadius;
                    const f64 z = sin(longitudeAngle) * ringRadius;

                    const vec3 pos =
                    {
                        scast<f32>(x),
                        scast<f32>(y),
                        scast<f32>(z)
                    };

                    vec3 normal = 
                    {
                        scast<f32>(x / radius),
                        scast<f32>(y / radius),
                        scast<f32>(z / radius)
                    };

                    if (sphereData.faceDir == FaceDirection::F_IN)
                    {
                        normal = -normal;
                    }

                    const f32 u = scast<f32>(scast<f64>(longitude) / longitudeCount);

                    data.vertices.push_back(
                    {
                        .pos = pos,
                        .normal = normal,
                        .uv = { u, v } 
                    });
                }
            }

            const u32 ringSize = longitudeCount + 1;

            for (u32 latitude = 0; latitude < latitudeCount; ++latitude)
            {
                for (u32 longitude = 0; longitude < longitudeCount; ++longitude)
                {
                    const u32 topLeft = latitude * ringSize + longitude;
                    const u32 topRight = topLeft + 1;
                    const u32 bottomLeft = (latitude + 1) * ringSize + longitude;
                    const u32 bottomRight = bottomLeft + 1;

                    //top pole only needs one triangle
                    if (latitude != 0)
                    {
                        if (sphereData.faceDir == FaceDirection::F_OUT)
                        {
                            data.indices.insert(
                                data.indices.end(), 
                                {
                                    topLeft,
                                    bottomLeft,
                                    topRight
                                });
                        }
                        else
                        {
                            data.indices.insert(
                                data.indices.end(), 
                                {
                                    topLeft,
                                    topRight,
                                    bottomLeft
                                });
                        }
                    }

                    //bottom pole only needs one triangle
                    if (latitude != latitudeCount - 1)
                    {
                        if (sphereData.faceDir == FaceDirection::F_OUT)
                        {
                            data.indices.insert(
                                data.indices.end(), 
                                {
                                    topRight,
                                    bottomLeft,
                                    bottomRight
                                });
                        }
                        else
                        {
                            data.indices.insert(
                                data.indices.end(), 
                                {
                                    topRight,
                                    bottomRight,
                                    bottomLeft
                                });
                        }
                    }
                }
            }
        }

        return data;
    }

    Mesh* Mesh::Initialize(
        u32 shaderID,
        u32 textureID)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to create mesh because the logical device was invalid!");
        }

        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            Log::Print(
                "Failed to create mesh because the shader was invalid! Reason: " + err,
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
        if (!err.empty())
        {
            Log::Print(
                "Failed to create mesh because the shader '" + to_string(shader->ID) + "' viewport was invalid! Reason: " + err,
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        //TODO: figure out a better solution
        if (shader->descriptorSetLayouts.empty())
        {
            Log::Print(
                "Failed to create mesh because the shader '" 
                + to_string(shaderID) + "' had no shader data!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(textureID, texture);
        if (!err.empty())
        {
            Log::Print(
                "Failed to create mesh because the texture was invalid! Reason: " + err,
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Mesh> newMesh = make_unique<Mesh>();
        Mesh* meshPtr = newMesh.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        meshPtr->ID = newID;
        meshPtr->shaderID = shaderID;
        meshPtr->textureID = textureID;

        //texture references this mesh
        texture->meshIDs.push_back(newID);

        //shader references this mesh
        shader->meshIDs.push_back(newID);

        meshPtr->isBufferDataDirty = true;
        meshPtr->is2D = shader->is2D;

        if (meshPtr->is2D)
        {
            meshPtr->vertices2D =
            {
                //bottom-left
                {
                    .pos = { -0.5f, -0.5f },
                    .uv = { 0.0f, 0.0f }
                },

                //bottom-right
                {
                    .pos = { 0.5f, -0.5f },
                    .uv = { 1.0f, 0.0f }
                },

                //top-right
                {
                    .pos = { 0.5f, 0.5f },
                    .uv = { 1.0f, 1.0f }
                },

                //top-left
                {
                    .pos = { -0.5f, 0.5f },
                    .uv = { 0.0f, 1.0f }
                }
            };
            meshPtr->indices =
            {
                0, 2, 1,
                0, 3, 2
            };

            meshPtr->isMeshDataDirty = true;
            vp->is2DMeshSortDirty = true;
        }

        err = registry.AddContent(newID, std::move(newMesh));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics mesh error",
				"Failed to initialize mesh! Reason: " + err);
        }

        Log::Print(
			"Created new mesh '" + to_string(newID) 
            + "' for shader '" + to_string(shaderID) + "'!",
			"KG_MESH",
			LogType::LOG_SUCCESS);

        return meshPtr;
    }

    u32 Mesh::GetID() const { return ID; }
    u32 Mesh::GetCameraID() const { return cameraID; }

    u32 Mesh::GetShaderID() const { return shaderID; }
    void Mesh::SetShaderID(u32 newValue)
    {
        if (newValue == 0)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' shader ID because it was empty!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (shaderID == newValue)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' shader ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Shader* oldShader{};
        string err = Shader::GetRegistry().GetContent(shaderID, oldShader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to set shader ID for mesh '" 
                + to_string(ID) + "' because of invalid old shader! Reason: " + err);
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(newValue, shader);
        if (!err.empty())
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' shader ID because it was invalid! Reason: " + err,
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (shader->is2D != is2D)
        {
            Log::Print(
                "Failed to update mesh '" + to_string(ID) 
                + "' shader because new shader '" + to_string(shader->ID) 
                + "' 2D state does not match old shader '" + to_string(oldShader->ID) + "' 2D state!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        shaderID = newValue;

        erase(
            oldShader->meshIDs,
            ID);
        shader->meshIDs.push_back(ID);

        Log::Print(
            "Set mesh '" + to_string(ID) 
            + "' shader ID to '" + to_string(shaderID) + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    u32 Mesh::GetTextureID() const { return textureID; }
    void Mesh::SetTextureID(u32 newValue)
    {
        if (textureID == newValue)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' texture ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Texture* oldTexture{};
        string err = Texture::GetRegistry().GetContent(textureID, oldTexture);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to set texture ID for mesh '" 
                + to_string(ID) + "' because of invalid old texture! Reason: " + err);
        }

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(newValue, texture);
        if (!texture)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' texture ID because it was invalid! Reason: " + err,
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        textureID = newValue;

        if (oldTexture)
        {
            erase(
                oldTexture->meshIDs,
                ID);
        }
        texture->meshIDs.push_back(ID);

        Log::Print(
            "Set mesh '" + to_string(ID) 
            + "' texture ID to '" + to_string(textureID) + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    bool Mesh::IsHovered() const { return hitTestID != 0; }

    bool Mesh::IsVisible() const { return isVisible; }
    void Mesh::SetVisibleState(bool newValue)
    {
        if (newValue == isVisible)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' "
                "visible state because it already is the same!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (shaderID != 0)
        {
            Shader* shader{};
            string err = Shader::GetRegistry().GetContent(shaderID, shader);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to update mesh '" + to_string(ID) + "' visible state "
                    "because its shader was invalid! Reason: " + err);
            }

            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to update mesh '" + to_string(ID) + "' visible state "
                    "because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
            }

            if (!is2D) vp->is3DMeshSortDirty = true;
            else       vp->is2DMeshSortDirty = true;
        }

        isVisible = newValue;
        string val = isVisible ? "true" : "false";

        Log::Print(
            "Set mesh '" + to_string(ID) + "' "
            "visible state to " + val + "!", 
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    bool Mesh::Is2D() const { return is2D; }

    u16 Mesh::GetDrawOrderIndex() const { return drawOrderIndex; }
    void Mesh::SetDrawOrderIndex(u16 newValue)
    {
        if (!is2D)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' draw order index because it is a 3D mesh!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        drawOrderIndex = newValue;

        if (shaderID != 0)
        {
            Shader* shader{};
            string err = Shader::GetRegistry().GetContent(shaderID, shader);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to update mesh '" + to_string(ID) + "' draw order "
                    "because its shader was invalid! Reason: " + err);
            }

            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to update mesh '" + to_string(ID) + "' draw order "
                    "because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
            }

            vp->is2DMeshSortDirty = true;
        }

        Log::Print(
            "Set mesh '" + to_string(ID) + "' draw order index to '" + to_string(drawOrderIndex) + "'.",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    Transform Mesh::GetTransform()
    {
        return
        {
            transform3D,
            transform2D,
            is2D,
            ID
        };
    }

    AnchorPosition Mesh::GetLocalAnchorPosition() const { return localAnchor; }
    void Mesh::SetLocalAnchorPosition(AnchorPosition newValue)
    { 
        if (!is2D)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' local anchor position because it is a 3D mesh!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        localAnchor = newValue;

        string val{};
        switch (newValue)
        {
        default:
        case AnchorPosition::P_DEFAULT:
            val = "default";
            break;

        case AnchorPosition::P_BOTTOM_LEFT:
            val = "bottom left";
            break;
        case AnchorPosition::P_BOTTOM_RIGHT:
            val = "bottom right";
            break;

        case AnchorPosition::P_TOP_LEFT:
            val = "top left";
            break;
        case AnchorPosition::P_TOP_RIGHT:
            val = "top right";
            break;

        case AnchorPosition::P_CENTER:
            val = "center";
            break;
        }

        Log::Print(
            "Set mesh '" + to_string(ID) + "' local anchor position to '" + val + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    AnchorPosition Mesh::GetViewportAnchorPosition() const { return viewportAnchor; }
    void Mesh::SetViewportAnchorPosition(AnchorPosition newValue)
    { 
        if (!is2D)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' viewport anchor position because it is a 3D mesh!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        viewportAnchor = newValue;

        string val{};
        switch (newValue)
        {
        default:
        case AnchorPosition::P_DEFAULT:
            val = "default";
            break;

        case AnchorPosition::P_BOTTOM_LEFT:
            val = "bottom left";
            break;
        case AnchorPosition::P_BOTTOM_RIGHT:
            val = "bottom right";
            break;

        case AnchorPosition::P_TOP_LEFT:
            val = "top left";
            break;
        case AnchorPosition::P_TOP_RIGHT:
            val = "top right";
            break;

        case AnchorPosition::P_CENTER:
            val = "center";
            break;
        }

        Log::Print(
            "Set mesh '" + to_string(ID) + "' viewport anchor position to '" + val + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    const vec4& Mesh::GetColor() const { return color; }
    void Mesh::SetColor(vec4&& newValue)
    {
        color = kclamp(newValue, 0, 1);

        string colorStr = 
            to_string(color.x) + ", "
            + to_string(color.y) + ", "
            + to_string(color.z) + ", "
            + to_string(color.w);

        if (!isnear(color.w, 1.0f)
            && isTransparent != 1)
        {
            Log::Print(
                "Mesh '" + to_string(ID) + "' color alpha "
                "was set below 1.0 but transparency is not enabled.",
                "KG_MESH",
                LogType::LOG_WARNING);
        }

        Log::Print(
            "Set mesh '" + to_string(ID) + "' color to '" + colorStr + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    bool Mesh::IsTransparent() const { return isTransparent; }
    void Mesh::SetTransparentState(bool newValue)
    {
        if (newValue == isTransparent)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' "
                "transparent state because it already is the same!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (shaderID != 0)
        {
            Shader* shader{};
            string err = Shader::GetRegistry().GetContent(shaderID, shader);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to update mesh '" + to_string(ID) + "' transparent state "
                    "because its shader was invalid! Reason: " + err);
            }

            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to update mesh '" + to_string(ID) + "' transparent state "
                    "because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
            }

            if (!is2D) vp->is3DMeshSortDirty = true;
            else       vp->is2DMeshSortDirty = true;
        }

        isTransparent = newValue;
        string transparentStr = newValue ? "true" : "false";

        Log::Print(
            "Set mesh '" + to_string(ID) + "' transparent state to '" + transparentStr + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    const vector<Vertex>& Mesh::GetVertices() const { return vertices; }
    const vector<Vertex2D>& Mesh::GetVertices2D() const { return vertices2D; }
    const vector<u32>& Mesh::GetIndices() const { return indices; }

    void Mesh::SetMeshData(MeshData&& meshData)
    {
        if (is2D)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' data "
                "because 2D mesh data cannot be updated!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (meshData.vertices == vertices
            && meshData.indices == indices)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' data "
                "because it is already the same!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (meshData.vertices.empty())
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' vertices "
                "because no 3D vertex data was passed!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vertices = std::move(meshData.vertices);
        indices = std::move(meshData.indices);

        if (shaderID != 0)
        {
            Shader* shader{};
            string err = Shader::GetRegistry().GetContent(shaderID, shader);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to set mesh '" + to_string(ID) + "' data "
                    "because its shader was invalid! Reason: " + err);
            }

            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to set mesh '" + to_string(ID) + "' data "
                    "because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
            }

            vp->is3DMeshSortDirty = true;
        }
    
        isMeshDataDirty = true;

        Log::Print(
            "Updated mesh data for mesh '" + to_string(ID) + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    const mat4& Mesh::GetMatrix() const { return meshMatrix; }

    void Mesh::FlipFaceDirection()
    {
        if (is2D)
        {
            Log::Print(
                "Failed to flip mesh '" + to_string(ID) 
                + "' face direction because it is a 2D mesh!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (!indices.empty())
        {
            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                swap(indices[i + 1], indices[i + 2]);
            }
        }
        else
        {
            for (size_t i = 0; i + 2 < vertices.size(); i += 3)
            {
                swap(vertices[i + 1], vertices[i + 2]);
            }
        }

        if (shaderID != 0)
        {
            Shader* shader{};
            string err = Shader::GetRegistry().GetContent(shaderID, shader);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to flip mesh '" + to_string(ID) + "' face direction "
                    "because its shader was invalid! Reason: " + err);
            }

            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to flip mesh '" + to_string(ID) + "' face direction "
                    "because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
            }

            vp->is3DMeshSortDirty = true;
        }
    
        isMeshDataDirty = true;

        Log::Print(
            "Flipped mesh '" + to_string(ID) + "' face directions!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    void Mesh::SetHoverCallback(function<void()>&& newValue)
    { 
        hoverCallback = std::move(newValue);
    }
    void Mesh::SetOnHoverStartCallback(function<void()>&& newValue)
    { 
        onHoverStartCallback = std::move(newValue);
    }
    void Mesh::SetOnHoverExitCallback(function<void()>&& newValue)
    { 
        onHoverExitCallback = std::move(newValue);
    }

    void Mesh::SetKeyHeldCallback(
        KeyboardButton btn, 
        function<void()>&& newValue,
        bool requireHover)
    {
        keyHeldCallbacks[btn] = { requireHover, std::move(newValue) };
    }
    void Mesh::SetKeyPressedCallback(
        KeyboardButton btn, 
        function<void()>&& newValue,
        bool requireHover)
    {
        keyPressedCallbacks[btn] = { requireHover, std::move(newValue) };
    }
    void Mesh::SetKeyReleasedCallback(
        KeyboardButton btn, 
        function<void()>&& newValue,
        bool requireHover)
    {
        keyReleasedCallbacks[btn] = { requireHover, std::move(newValue) };
    }

    void Mesh::SetMouseButtonHeldCallback(
        MouseButton btn, 
        function<void()>&& newValue,
        bool requireHover)
    {
        mouseButtonHeldCallbacks[btn] = { requireHover, std::move(newValue) };
    }
    void Mesh::SetMouseButtonPressedCallback(
        MouseButton btn, 
        function<void()>&& newValue,
        bool requireHover)
    {
        mouseButtonPressedCallbacks[btn] = { requireHover, std::move(newValue) };
    }
    void Mesh::SetMouseButtonReleasedCallback(
        MouseButton btn, 
        function<void()>&& newValue,
        bool requireHover)
    {
        mouseButtonReleasedCallbacks[btn] = { requireHover, std::move(newValue) };
    }
    void Mesh::SetMouseButtonDoubleClickedCallback(
        MouseButton btn, 
        function<void()>&& newValue,
        bool requireHover)
    {
        mouseButtonDoubleClickedCallbacks[btn] = { requireHover, std::move(newValue) };
    }
    void Mesh::SetMouseButtonDraggingCallback(
        MouseButton btn, 
        function<void(vec2)>&& newValue,
        bool requireHover)
    {
        mouseButtonDraggingCallbacks[btn] = { requireHover, std::move(newValue) };
    }

    void Mesh::SetScrollUpCallback(
        function<void(f32)>&& newValue,
        bool requireHover)
    {
        scrollUpCallback = { requireHover, std::move(newValue) };
    }
    void Mesh::SetScrollDownCallback(
        function<void(f32)>&& newValue,
        bool requireHover)
    {
        scrollDownCallback = { requireHover, std::move(newValue) };
    }

    void Mesh::UpdateMeshData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) + "' data "
                "because the vma allocator was invalid!");
        }

        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because its shader was invalid! Reason: " + err);
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
        }

        GraphicsContext* gctx{};
        err = GraphicsContext::GetRegistry().GetContent(vp->contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because its viewport '" + to_string(vp->GetID()) + "' graphics context was invalid! Reason: " + err);
        }

        auto is_valid_hover_state = [&](bool requireHoverState) -> bool
            {
                return 
                    !requireHoverState 
                    || IsHovered();  
            };

        //keyboard button callbacks

        for (KeyboardButton key : gctx->GetHeldKeys())
        {
            auto it = keyHeldCallbacks.find(key);
            if (it != keyHeldCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second();
            }
        }
        for (KeyboardButton key : gctx->GetPressedKeys())
        {
            auto it = keyPressedCallbacks.find(key);
            if (it != keyPressedCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second();
            }
        }
        for (KeyboardButton key : gctx->GetReleasedKeys())
        {
            auto it = keyReleasedCallbacks.find(key);
            if (it != keyReleasedCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second();
            }
        }

        //mouse button callbacks

        for (MouseButton mb : gctx->GetHeldMouseButtons())
        {
            auto it = mouseButtonHeldCallbacks.find(mb);
            if (it != mouseButtonHeldCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second();
            }
        }
        for (MouseButton mb : gctx->GetPressedMouseButtons())
        {
            auto it = mouseButtonPressedCallbacks.find(mb);
            if (it != mouseButtonPressedCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second();
            }
        }
        for (MouseButton mb : gctx->GetReleasedMouseButtons())
        {
            auto it = mouseButtonReleasedCallbacks.find(mb);
            if (it != mouseButtonReleasedCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second();
            }
        }
        for (MouseButton mb : gctx->GetDoubleClickedMouseButtons())
        {
            auto it = mouseButtonDoubleClickedCallbacks.find(mb);
            if (it != mouseButtonDoubleClickedCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second();
            }
        }

        for (MouseButton mb : gctx->GetDraggingMouseButtons())
        {
            auto it = mouseButtonDraggingCallbacks.find(mb);
            if (it != mouseButtonDraggingCallbacks.end()
                && it->second.second
                && is_valid_hover_state(it->second.first))
            {
                it->second.second(gctx->mousePosYReversed);
            }
        }

        //scrollwheel callbacks

        f32 scrollWheelDelta = gctx->GetScrollWheelDelta();

        if (scrollWheelDelta > 0
            && scrollUpCallback.second
            && is_valid_hover_state(scrollUpCallback.first))
        {
            scrollUpCallback.second(scrollWheelDelta);
        }
        if (scrollWheelDelta < 0
            && scrollDownCallback.second
            && is_valid_hover_state(scrollDownCallback.first))
        {
            scrollDownCallback.second(scrollWheelDelta);
        }

        //rest of update logic

        if (vkDescriptorSet == VK_NULL_HANDLE)
        {
            //
            // DESCRIPTOR SET
            //

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = GraphicsContext::GetDescriptorPool();
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts = &shader->descriptorSetLayouts[1];

            VkDescriptorSet newDescriptorSet;
            VkResult vkResult = vkAllocateDescriptorSets(
                logicalDevice,
                &descriptorSetAllocateInfo,
                &newDescriptorSet);

            if (vkResult != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh update error",
                    "Failed to update mesh because descriptor set init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkDescriptorSet = newDescriptorSet;

            //
            // VMA ALLOCATOR
            //

            size_t bufferSize = sizeof(mat4);

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.flags = 
                VMA_ALLOCATION_CREATE_MAPPED_BIT
                | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

            VkBuffer newBuffer{};
            VmaAllocation newAllocation{};
            VmaAllocationInfo allocResult{};

            VkResult result = vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocInfo,
                &newBuffer,
                &newAllocation,
                &allocResult);

            if (result != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh update error",
                    "Failed to update mesh because vma allocator init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkMeshUBOBuffer = newBuffer;
            vmaMeshUBOAllocation = newAllocation;
            meshUBOMappedPtr = allocResult.pMappedData;
        }
    
        if (is2D)
        {
            vec2 pos_world = transform2D.getpos(PosTarget::POS_WORLD);
            f32 rot_world = transform2D.getrot(RotTarget::ROT_WORLD);
            vec2 size_world = transform2D.getsize(SizeTarget::SIZE_WORLD);

            f32 fullWidth = isnear(size_world.x) ? 0 : size_world.x;
            f32 fullHeight = isnear(size_world.y) ? 0 : size_world.y;

            f32 halfWidth = fullWidth * 0.5f;
            f32 halfHeight = fullHeight * 0.5f;

            vec2 localAnchorPos{};
            switch (localAnchor)
            {
            default:
            case AnchorPosition::P_DEFAULT:
            case AnchorPosition::P_CENTER:
                break;

            case AnchorPosition::P_BOTTOM_LEFT:
                localAnchorPos = { -halfWidth, -halfHeight };
                break;

            case AnchorPosition::P_BOTTOM_RIGHT:
                localAnchorPos = { halfWidth, -halfHeight };
                break;

            case AnchorPosition::P_TOP_LEFT:
                localAnchorPos = { -halfWidth, halfHeight };
                break;

            case AnchorPosition::P_TOP_RIGHT:
                localAnchorPos = { halfWidth, halfHeight };
                break;
            }

            vec2 viewportAnchorPos{};
            switch (viewportAnchor)
            {
            default:
            case AnchorPosition::P_DEFAULT:
                viewportAnchorPos = 0;
                break;

            case AnchorPosition::P_BOTTOM_LEFT:
                viewportAnchorPos = vp->posBottomLeft;
                break;
            case AnchorPosition::P_BOTTOM_RIGHT:
                viewportAnchorPos = vp->posBottomRight;
                break;

            case AnchorPosition::P_TOP_LEFT:
                viewportAnchorPos = vp->posTopLeft;
                break;
            case AnchorPosition::P_TOP_RIGHT:
                viewportAnchorPos = vp->posTopRight;
                break;

            case AnchorPosition::P_CENTER:
                viewportAnchorPos = vp->posCenter;
                break;
            }

            //stored for hit test
            finalAnchorPos = pos_world - localAnchorPos + viewportAnchorPos;

            meshMatrix = createmodelmatrix(
                finalAnchorPos, 
                rot_world, 
                size_world);
        }
        else
        {
            meshMatrix = createmodelmatrix3d(
                transform3D.getpos(PosTarget::POS_WORLD),
                transform3D.getrotquat(RotTarget::ROT_WORLD), 
                transform3D.getsize(SizeTarget::SIZE_WORLD));
        }

        memcpy(
            meshUBOMappedPtr,
            &meshMatrix,
            sizeof(mat4));

        if (isBufferDataDirty)
        {
            VkDescriptorBufferInfo transformInfo{};
            transformInfo.buffer = vkMeshUBOBuffer;
            transformInfo.offset = 0;
            transformInfo.range = sizeof(mat4);

            VkWriteDescriptorSet writes[1]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = vkDescriptorSet;
            writes[0].dstBinding = 0; // <<<< SET 1 BINDING 0 - MESH UBO SLOT
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &transformInfo;

            vkUpdateDescriptorSets(
                logicalDevice,
                1,
                writes,
                0,
                nullptr);

            isBufferDataDirty = false;
        }

        if (isMeshDataDirty)
        {
            {
                u64 newSize = (is2D 
                    ? vertices2D.size() * sizeof(Vertex2D) 
                    : vertices.size() * sizeof(Vertex));
                if (newSize == 0)
                {
                    Log::Print(
                        "Failed to update vertices for mesh '" + to_string(ID) + "' because no vertex data was passed!",
                        "KG_MESH",
                        LogType::LOG_WARNING);

                    return;
                }

                if (newSize != verticesSize
                    || vkVertexBuffer == VK_NULL_HANDLE)
                {
                    VkBufferCreateInfo bufferInfo{};
                    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    bufferInfo.size = newSize;
                    bufferInfo.usage = 
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                        | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                    VmaAllocationCreateInfo allocInfo{};
                    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                    allocInfo.flags = 
                        VMA_ALLOCATION_CREATE_MAPPED_BIT
                        | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

                    VkBuffer newBuffer{};
                    VmaAllocation newAllocation{};
                    VmaAllocationInfo allocResult{};

                    VkResult result = vmaCreateBuffer(
                        allocator,
                        &bufferInfo,
                        &allocInfo,
                        &newBuffer,
                        &newAllocation,
                        &allocResult);

                    if (result != VK_SUCCESS)
                    {
                        Log::Print(
                            "Failed to update vertices for mesh '" + to_string(ID) + "' because vertex vk buffer creation failed!",
                            "KG_MESH",
                            LogType::LOG_ERROR,
                            2);

                        return;
                    }

                    if (vkVertexBuffer != VK_NULL_HANDLE)
                    {
                        Log::Print(
                            "Recreating vertex buffer during mesh '" + to_string(ID) 
                            + "' update because old buffer size did not match new buffer size.",
                            "KG_MESH",
                            LogType::LOG_INFO);

                        vkDeviceWaitIdle(logicalDevice);

                        vmaDestroyBuffer(
                            allocator,
                            vkVertexBuffer,
                            vmaVertexAllocation);

                        vmaVertexAllocation = VK_NULL_HANDLE;
                        vkVertexBuffer = VK_NULL_HANDLE;
                        vertexMappedPtr = nullptr;
                    }

                    vkVertexBuffer = newBuffer;
                    verticesSize = newSize;
                    vmaVertexAllocation = newAllocation;
                    vertexMappedPtr = allocResult.pMappedData;
                }

                memcpy(
                    vertexMappedPtr, 
                    is2D 
                        ? scast<const void*>(vertices2D.data()) 
                        : scast<const void*>(vertices.data()), 
                    verticesSize);
            }

            {
                u64 newSize = indices.size() * sizeof(u32);

                //empty indices = non-indexed mesh, not an error
                if (newSize == 0)
                {
                    if (vkIndexBuffer != VK_NULL_HANDLE)
                    {
                        vmaDestroyBuffer(
                            allocator,
                            vkIndexBuffer,
                            vmaIndexAllocation);

                        vmaIndexAllocation = VK_NULL_HANDLE;
                        vkIndexBuffer = VK_NULL_HANDLE;
                        indexMappedPtr = nullptr;
                    }

                    return;
                }

                if (indicesSize != newSize
                    || vkIndexBuffer == VK_NULL_HANDLE)
                {
                    VkBufferCreateInfo indexBufferInfo{};
                    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    indexBufferInfo.size = newSize;
                    indexBufferInfo.usage =
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                        | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                    indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                    VmaAllocationCreateInfo indexAllocInfo{};
                    indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                    indexAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                    indexAllocInfo.flags = 
                        VMA_ALLOCATION_CREATE_MAPPED_BIT
                        | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

                    VkBuffer newBuffer{};
                    VmaAllocation newAllocation{};
                    VmaAllocationInfo allocResult{};

                    VkResult result = vmaCreateBuffer(
                        allocator,
                        &indexBufferInfo,
                        &indexAllocInfo,
                        &newBuffer,
                        &newAllocation,
                        &allocResult);

                    if (result != VK_SUCCESS)
                    {
                        Log::Print(
                            "Failed to update indices for mesh '" + to_string(ID) + "' because index vk buffer creation failed!",
                            "KG_MESH",
                            LogType::LOG_ERROR,
                            2);

                        return;
                    }

                    if (vkIndexBuffer != VK_NULL_HANDLE)
                    {
                        Log::Print(
                            "Recreating index buffer during mesh '" + to_string(ID) 
                            + "' update because old buffer size did not match new buffer size.",
                            "KG_MESH",
                            LogType::LOG_INFO);

                        vkDeviceWaitIdle(logicalDevice);

                        vmaDestroyBuffer(
                            allocator,
                            vkIndexBuffer,
                            vmaIndexAllocation);

                        vmaIndexAllocation = VK_NULL_HANDLE;
                        vkIndexBuffer = VK_NULL_HANDLE;
                        indexMappedPtr = nullptr;
                    }

                    vkIndexBuffer = newBuffer;
                    indicesSize = newSize;
                    vmaIndexAllocation = newAllocation;
                    indexMappedPtr = allocResult.pMappedData;
                }

                memcpy(
                    indexMappedPtr, 
                    indices.data(), 
                    indicesSize);
            }

            isMeshDataDirty = false;
        }
    }

    void Mesh::Update(VkCommandBuffer buffer)
    {
        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because its shader was invalid! Reason: " + err);
        }

        if (shader->is2D != is2D)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) + "' "
                "because its 2D state doesn't match its shader '" + to_string(shaderID) + "' 2D state!");
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
        }

        UpdateMeshData();

        /*
        Log::Print(
            "@@@@@\n"
            "mesh: " + to_string(ID) + "\n"
            "shader: " + to_string(shaderID) + "\n"
            "is 2D: " + (shader->is2D ? "true" : "false") + "\n"
            "last bound 3D shader: " + to_string(vp->lastBoundShader3DID) + "\n"
            "last bound 2D shader: " + to_string(vp->lastBoundShader2DID));
        */

        //only bind pipeline if last bound shader is not the same as this mesh shader
        if ((!shader->is2D 
            && vp->lastBoundShader3DID != shaderID)
            || (shader->is2D 
            && vp->lastBoundShader2DID != shaderID))
        {
            vkCmdBindPipeline(
                buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                shader->pipeline);

            Camera* c{};
            err = Camera::GetRegistry().GetContent(
                (!is2D ? vp->primary3DCameraID : vp->primary2DCameraID), 
                c);

            string camStr = !is2D ? "3D" : "2D";
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to update mesh '" + to_string(ID) 
                    + "' data because its viewport '" + to_string(vp->ID) + "' primary " 
                    + camStr + " camera was invalid! Reason: " + err);
            }

            c->UpdateCameraData();

            vkCmdBindDescriptorSets(
                buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                shader->pipelineLayout,
                0, // <<<< SET 0 BINDING 0 - CAMERA UBO SLOT
                1,
                &c->vkDescriptorSet,
                0,
                nullptr);

            if (!shader->is2D) vp->lastBoundShader3DID = shaderID;
            else               vp->lastBoundShader2DID = shaderID;
        }

        if (vkVertexBuffer == VK_NULL_HANDLE)
        {
            //skip mesh if it has no vertex buffer data
            if (verticesSize == 0) return;
            //invalid mesh, vertex buffer was removed for calculated mesh data
            else
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to render mesh '" + to_string(ID) 
                    + "' because its vertex buffer size is more than 0 but it "
                    "doesn't have a valid vertex buffer!");
            }
        }
        //vertex buffer was added but its data was not assigned
        else if (verticesSize == 0)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to render mesh '" + to_string(ID) 
                + "' because its vertex buffer is valid but it "
                "doesn't have vertex buffer data!");
        }

        vkCmdBindDescriptorSets(
            buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            shader->pipelineLayout,
            1, // <<<< SET 1 BINDING 0 - MESH UBO SLOT
            1,
            &vkDescriptorSet,
            0,
            nullptr);

        vkCmdPushConstants(
            buffer,
            shader->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(color),
            &color);
        vkCmdPushConstants(
            buffer,
            shader->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(color),
            sizeof(isTransparent),
            &isTransparent);

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(textureID, texture);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to render mesh '" + to_string(ID) 
                + "' because its texture was invalid! Reason: " + err);
        }

        vkCmdBindDescriptorSets(
            buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            shader->pipelineLayout,
            2, // <<<< SET 2 BINDING 0 - TEXTURE SAMPLER SLOT
            1,
            &texture->vkDescriptorSet,
            0,
            nullptr);

        /*
        Log::Print(
            "@@@@@\n"
            "mesh ID: " + to_string(ID) + "\n"
            "vertices size: " + to_string(vertices.size()) + "\n"
            "indices size: " + to_string(indices.size()));
        */

        VkDeviceSize offset{};
        vkCmdBindVertexBuffers(
            buffer,
            0,
            1,
            &vkVertexBuffer,
            &offset);

        if (vkIndexBuffer == VK_NULL_HANDLE)
        {
            vkCmdDraw(
                buffer,
                is2D
                    ? scast<u32>(vertices2D.size())
                    : scast<u32>(vertices.size()),
                1,
                0,
                0);
        }
        else
        {
            //vertex buffer was added but its data was not assigned
            if (indicesSize == 0)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to render mesh '" + to_string(ID) 
                    + "' because its index buffer is valid but it "
                    "doesn't have index buffer data!");
            }
            else
            {
                vkCmdBindIndexBuffer(
                    buffer,
                    vkIndexBuffer,
                    0,
                    VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(
                    buffer,
                    indices.size(),
                    1,
                    0,
                    0,
                    0);
            }
        }
    }

    void Mesh::Destroy()
    {
        Camera* camera{};
        string err = Camera::GetRegistry().GetContent(cameraID, camera);
        if (err.empty()) camera->meshID = 0;

        //only remove this mesh from texture meshes list of the texture is still valid

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(textureID, texture);
        if (!err.empty())
        {
            erase(
                texture->meshIDs,
                ID);
        }

        //only remove this mesh from shader meshes list if the shader is still valid

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (err.empty())
        {
            erase(
                shader->meshIDs,
                ID);

            Viewport* vp{};
            err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to destroy mesh '" + to_string(ID) + "' "
                    "because its shader '" + to_string(shaderID) + "' viewport was invalid! Reason: " + err);
            }

            if (!is2D) vp->is3DMeshSortDirty = true;
            else       vp->is2DMeshSortDirty = true;
        }

        HitTest* hitTest{};
        err = HitTest::GetRegistry().GetContent(hitTestID, hitTest);
        if (err.empty())
        {
            if (is2D) hitTest->mesh2DID = 0;
            else      hitTest->mesh3DID = 0;
        }

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to destroy mesh '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Mesh::~Mesh()
    {
        Log::Print(
            "Destroying mesh '" + to_string(ID) + "'.",
            "KG_MESH",
            LogType::LOG_INFO);

        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to clear mesh '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to clear mesh '" + to_string(ID) 
                + "' data because the vma allocator was invalid!");
        }

        //drain the gpu before destroying this mesh
        VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
        if (vkResult != VK_SUCCESS)
        {
            GraphicsContext::ForceClose(
                "KalaGraphics mesh error",
                "Failed to clear mesh '" 
                + to_string(ID) + "' data because vkDeviceWaitIdle did not succeed!",
                vkResult);
        }

        isBufferDataDirty = false;
        isMeshDataDirty = false;

        if (cameraID != 0)
        {
            Camera* cam{};
            string err = Camera::GetRegistry().GetContent(cameraID, cam);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to clear mesh '" + to_string(ID) 
                    + "' data because its camera was invalid! Reason: " + err);
            }

            cam->meshID = 0;

            Log::Print(
                "Detached mesh '" + to_string(ID) 
                + "' from camera '" + to_string(cameraID) + "' because mesh data was cleared!",
                "KG_MESH",
                LogType::LOG_WARNING);

            cameraID = 0;
        }

        vertices.clear();
        indices.clear();

        if (vkVertexBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkVertexBuffer,
                vmaVertexAllocation);

            vmaVertexAllocation = VK_NULL_HANDLE;
            vkVertexBuffer = VK_NULL_HANDLE;
            vertexMappedPtr = nullptr;
        }
        if (vkIndexBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkIndexBuffer,
                vmaIndexAllocation);

            vmaIndexAllocation = VK_NULL_HANDLE;
            vkIndexBuffer = VK_NULL_HANDLE;
            indexMappedPtr = nullptr;
        }

        if (vmaMeshUBOAllocation != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkMeshUBOBuffer,
                vmaMeshUBOAllocation);

            meshUBOMappedPtr = nullptr;
        }

        if (vkDescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(
                logicalDevice,
                GraphicsContext::GetDescriptorPool(),
                1,
                &vkDescriptorSet);
        }
    }
}