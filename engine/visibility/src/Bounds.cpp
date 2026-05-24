#include <AK/Visibility/Bounds.hpp>

#include <AK/Math/Transform.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        EulerTransform ToEulerTransform(const TransformComponent& transform)
        {
            return MakeEulerTransform(transform.position, transform.rotation, transform.scale);
        }

        bool NeedsBoundsRebuild(const TransformComponent& transform, const BoundsComponent* bounds, bool force)
        {
            if (force || !bounds)
            {
                return true;
            }

            return (transform.dirtyFlags & TransformDirty_Bounds) != 0
                || (bounds->dirtyFlags & (BoundsDirty_World | BoundsDirty_Sphere | BoundsDirty_Visibility)) != 0;
        }
    }

    AABB3 DefaultLocalBoundsForEntity(const World& world, EntityId entity)
    {
        if (world.HasMesh(entity))
        {
            return MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        }

        if (world.HasCamera(entity))
        {
            return MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.25f, 0.18f, 0.35f});
        }

        if (const LightComponent* light = world.GetLight(entity))
        {
            const float range = light->type == LightType::Directional ? 0.35f : std::max(0.35f, light->range);
            return MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {range, range, range});
        }

        return MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.20f, 0.20f, 0.20f});
    }

    Sphere3 MakeBoundingSphere(AABB3 bounds)
    {
        if (!IsValid(bounds))
        {
            return {};
        }

        const Vec3 center = Center(bounds);
        const Vec3 extents = Extents(bounds);
        return {center, Length(extents)};
    }

    BoundsComponent BuildBoundsComponent(const TransformComponent& transform, AABB3 localBounds)
    {
        BoundsComponent bounds{};
        bounds.localBounds = IsValid(localBounds) ? localBounds : MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        bounds.worldBounds = TransformAABB(Mat4FromEulerTransform(ToEulerTransform(transform)), bounds.localBounds);
        bounds.worldSphere = MakeBoundingSphere(bounds.worldBounds);
        bounds.dirtyFlags = BoundsDirty_None;
        bounds.visible = true;
        bounds.culled = false;
        ++bounds.revision;
        return bounds;
    }

    bool RebuildEntityBounds(World& world, EntityId entity, bool force)
    {
        TransformComponent* transform = world.GetTransform(entity);
        if (!transform)
        {
            return false;
        }

        BoundsComponent* existingBounds = world.GetBounds(entity);
        if (!NeedsBoundsRebuild(*transform, existingBounds, force))
        {
            return false;
        }

        const AABB3 localBounds = existingBounds && IsValid(existingBounds->localBounds)
            ? existingBounds->localBounds
            : DefaultLocalBoundsForEntity(world, entity);

        BoundsComponent rebuilt = BuildBoundsComponent(*transform, localBounds);
        if (existingBounds)
        {
            rebuilt.revision = existingBounds->revision + 1;
            rebuilt.visible = existingBounds->visible;
            rebuilt.culled = existingBounds->culled;
        }

        world.AddBounds(entity) = rebuilt;
        transform->dirtyFlags &= ~TransformDirty_Bounds;
        return true;
    }

    BoundsUpdateStats RebuildSceneBounds(World& world, bool force)
    {
        BoundsUpdateStats stats{};
        stats.sceneBounds = MakeEmptyAABB3();

        for (const EntityRecord& entity : world.Entities())
        {
            ++stats.visited;
            const bool hadBounds = world.HasBounds(entity.id);
            const bool rebuilt = RebuildEntityBounds(world, entity.id, force);
            if (!hadBounds && world.HasBounds(entity.id))
            {
                ++stats.created;
            }
            if (rebuilt)
            {
                ++stats.rebuilt;
            }

            const BoundsComponent* bounds = world.GetBounds(entity.id);
            if (!bounds || !IsValid(bounds->worldBounds))
            {
                ++stats.invalid;
                continue;
            }

            stats.sceneBounds = IsValid(stats.sceneBounds) ? Union(stats.sceneBounds, bounds->worldBounds) : bounds->worldBounds;
        }

        return stats;
    }

    VisibilityClassification ClassifyAABB(const Frustum3& frustum, AABB3 bounds)
    {
        if (!IsValid(bounds))
        {
            return VisibilityClassification::Outside;
        }

        bool intersects = false;
        for (const Plane3& plane : frustum.planes)
        {
            const Vec3 positiveVertex{
                plane.normal.x >= 0.0f ? bounds.max.x : bounds.min.x,
                plane.normal.y >= 0.0f ? bounds.max.y : bounds.min.y,
                plane.normal.z >= 0.0f ? bounds.max.z : bounds.min.z
            };

            if (SignedDistance(plane, positiveVertex) < 0.0f)
            {
                return VisibilityClassification::Outside;
            }

            const Vec3 negativeVertex{
                plane.normal.x >= 0.0f ? bounds.min.x : bounds.max.x,
                plane.normal.y >= 0.0f ? bounds.min.y : bounds.max.y,
                plane.normal.z >= 0.0f ? bounds.min.z : bounds.max.z
            };

            if (SignedDistance(plane, negativeVertex) < 0.0f)
            {
                intersects = true;
            }
        }

        return intersects ? VisibilityClassification::Intersecting : VisibilityClassification::Inside;
    }

    VisibilityStats UpdateFrustumVisibility(World& world, const Frustum3& frustum)
    {
        VisibilityStats stats{};
        for (const EntityRecord& entity : world.Entities())
        {
            BoundsComponent* bounds = world.GetBounds(entity.id);
            if (!bounds)
            {
                continue;
            }

            ++stats.tested;
            const VisibilityClassification classification = ClassifyAABB(frustum, bounds->worldBounds);
            bounds->visible = classification != VisibilityClassification::Outside;
            bounds->culled = classification == VisibilityClassification::Outside;
            bounds->dirtyFlags &= ~BoundsDirty_Visibility;
            if (bounds->visible)
            {
                ++stats.visible;
            }
            else
            {
                ++stats.culled;
            }
        }
        return stats;
    }

    std::vector<EntityId> CollectVisibleEntities(const World& world)
    {
        std::vector<EntityId> result;
        result.reserve(world.Entities().size());
        for (const EntityRecord& entity : world.Entities())
        {
            const BoundsComponent* bounds = world.GetBounds(entity.id);
            if (!bounds || bounds->visible)
            {
                result.push_back(entity.id);
            }
        }
        return result;
    }

    Frustum3 MakeOrthographicFrustum(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
    {
        Frustum3 frustum{};
        frustum.planes[0] = MakePlane({1.0f, 0.0f, 0.0f}, -minX);
        frustum.planes[1] = MakePlane({-1.0f, 0.0f, 0.0f}, maxX);
        frustum.planes[2] = MakePlane({0.0f, 1.0f, 0.0f}, -minY);
        frustum.planes[3] = MakePlane({0.0f, -1.0f, 0.0f}, maxY);
        frustum.planes[4] = MakePlane({0.0f, 0.0f, 1.0f}, -minZ);
        frustum.planes[5] = MakePlane({0.0f, 0.0f, -1.0f}, maxZ);
        return frustum;
    }

    std::string ToDebugString(VisibilityClassification classification)
    {
        switch (classification)
        {
            case VisibilityClassification::Outside:
                return "outside";
            case VisibilityClassification::Intersecting:
                return "intersecting";
            case VisibilityClassification::Inside:
                return "inside";
            default:
                return "unknown";
        }
    }

    std::string ToDebugString(const BoundsUpdateStats& stats)
    {
        std::ostringstream out;
        out << "visited=" << stats.visited
            << " created=" << stats.created
            << " rebuilt=" << stats.rebuilt
            << " invalid=" << stats.invalid;
        if (IsValid(stats.sceneBounds))
        {
            out << " scene_size=(" << ToDebugString(Size(stats.sceneBounds), 2) << ")";
        }
        return out.str();
    }

    std::string ToDebugString(const VisibilityStats& stats)
    {
        std::ostringstream out;
        out << "tested=" << stats.tested
            << " visible=" << stats.visible
            << " culled=" << stats.culled;
        return out.str();
    }

    std::string BuildBoundsVisibilityProbeSummary()
    {
        World world;
        const EntityId inside = world.CreateEntity("Inside");
        TransformComponent& insideTransform = world.AddTransform(inside);
        insideTransform.position = {0.0f, 0.0f, 0.0f};
        world.AddMesh(inside);

        const EntityId outside = world.CreateEntity("Outside");
        TransformComponent& outsideTransform = world.AddTransform(outside);
        outsideTransform.position = {100.0f, 0.0f, 0.0f};
        world.AddMesh(outside);

        const BoundsUpdateStats boundsStats = RebuildSceneBounds(world, true);
        const Frustum3 frustum = MakeOrthographicFrustum(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);
        const VisibilityStats visibilityStats = UpdateFrustumVisibility(world, frustum);

        std::ostringstream out;
        out << "Bounds probe: " << ToDebugString(boundsStats)
            << " | visibility " << ToDebugString(visibilityStats);
        return out.str();
    }
}
