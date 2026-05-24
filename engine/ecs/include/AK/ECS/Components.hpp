#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/World/WorldCoordinates.hpp>

#include <string>

namespace AK
{
    enum class LightType : u32
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    enum TransformDirtyFlag : u32
    {
        TransformDirty_None = 0,
        TransformDirty_LocalMatrix = 1u << 0u,
        TransformDirty_WorldMatrix = 1u << 1u,
        TransformDirty_Bounds = 1u << 2u,
        TransformDirty_RenderProxy = 1u << 3u,
        TransformDirty_PhysicsProxy = 1u << 4u,
        TransformDirty_All = TransformDirty_LocalMatrix
            | TransformDirty_WorldMatrix
            | TransformDirty_Bounds
            | TransformDirty_RenderProxy
            | TransformDirty_PhysicsProxy
    };


    enum BoundsDirtyFlag : u32
    {
        BoundsDirty_None = 0,
        BoundsDirty_Local = 1u << 0u,
        BoundsDirty_World = 1u << 1u,
        BoundsDirty_Sphere = 1u << 2u,
        BoundsDirty_Visibility = 1u << 3u,
        BoundsDirty_All = BoundsDirty_Local
            | BoundsDirty_World
            | BoundsDirty_Sphere
            | BoundsDirty_Visibility
    };

    struct BoundsComponent
    {
        AABB3 localBounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        AABB3 worldBounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        Sphere3 worldSphere{{0.0f, 0.0f, 0.0f}, 0.8660254f};
        u32 dirtyFlags = BoundsDirty_All;
        u64 revision = 1;
        bool visible = true;
        bool culled = false;
    };

    struct TransformComponent
    {
        Vec3 position{0.0f, 0.0f, 0.0f};
        Vec3 rotation{0.0f, 0.0f, 0.0f};
        Vec3 scale{1.0f, 1.0f, 1.0f};
        u32 dirtyFlags = TransformDirty_All;
        u64 revision = 1;
    };

    struct WorldPositionComponent
    {
        WorldPosition position{};
        bool authoritative = true;
    };

    struct CameraComponent
    {
        float verticalFovDegrees = 60.0f;
        float nearPlane = 0.05f;
        float farPlane = 1000.0f;
        bool primary = false;
    };

    struct LightComponent
    {
        LightType type = LightType::Directional;
        float intensity = 1.0f;
        Vec3 color{1.0f, 1.0f, 1.0f};
        float range = 10.0f;
        float innerConeDegrees = 20.0f;
        float outerConeDegrees = 35.0f;
    };

    struct MeshComponent
    {
        std::string mesh = "builtin:cube";
        std::string material = "builtin:default";
    };
}
