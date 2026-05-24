#pragma once

#include <AK/ECS/World.hpp>
#include <AK/Math/Geometry.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class VisibilityClassification : u32
    {
        Outside = 0,
        Intersecting = 1,
        Inside = 2
    };

    struct BoundsUpdateStats
    {
        std::size_t visited = 0;
        std::size_t created = 0;
        std::size_t rebuilt = 0;
        std::size_t invalid = 0;
        AABB3 sceneBounds = MakeEmptyAABB3();
    };

    struct VisibilityStats
    {
        std::size_t tested = 0;
        std::size_t visible = 0;
        std::size_t culled = 0;
    };

    AABB3 DefaultLocalBoundsForEntity(const World& world, EntityId entity);
    Sphere3 MakeBoundingSphere(AABB3 bounds);
    BoundsComponent BuildBoundsComponent(const TransformComponent& transform, AABB3 localBounds);
    bool RebuildEntityBounds(World& world, EntityId entity, bool force = false);
    BoundsUpdateStats RebuildSceneBounds(World& world, bool force = false);

    VisibilityClassification ClassifyAABB(const Frustum3& frustum, AABB3 bounds);
    VisibilityStats UpdateFrustumVisibility(World& world, const Frustum3& frustum);
    std::vector<EntityId> CollectVisibleEntities(const World& world);

    Frustum3 MakeOrthographicFrustum(float minX, float maxX, float minY, float maxY, float minZ, float maxZ);
    std::string ToDebugString(VisibilityClassification classification);
    std::string ToDebugString(const BoundsUpdateStats& stats);
    std::string ToDebugString(const VisibilityStats& stats);
    std::string BuildBoundsVisibilityProbeSummary();
}
