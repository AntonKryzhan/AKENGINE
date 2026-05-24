#include <AK/Destruction/Destruction.hpp>

#include <AK/Core/Guid.hpp>
#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr u32 MinimumDestructionResolution = 2;
        constexpr u32 MaximumDestructionResolution = 256;
        constexpr u32 DefaultPhysicsBodyIdFallback = 1;
        constexpr u64 BytesPerGeneratedVoxel = 32;
        constexpr u64 BytesPerCollisionProxy = 64;

        u32 BodyIdFromEntity(EntityId entity)
        {
            return entity.IsValid() ? entity.index : DefaultPhysicsBodyIdFallback;
        }

        bool HasBody(const PhysicsScene& scene, u32 bodyId)
        {
            return FindBody(scene, bodyId) != nullptr;
        }

        std::string SanitizeDebugName(std::string_view name)
        {
            std::string result;
            result.reserve(name.size());
            for (char c : name)
            {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')
                {
                    result.push_back(c);
                }
                else if (c == ' ' || c == ':' || c == '/' || c == '\\')
                {
                    result.push_back('_');
                }
            }
            if (result.empty())
            {
                result = "destruction";
            }
            return result;
        }

        Vec3 SafeExtents(AABB3 bounds)
        {
            if (!IsValid(bounds))
            {
                return {0.5f, 0.5f, 0.5f};
            }
            const Vec3 extents = Extents(bounds);
            return {
                std::max(0.001f, std::fabs(extents.x)),
                std::max(0.001f, std::fabs(extents.y)),
                std::max(0.001f, std::fabs(extents.z))
            };
        }

        BoundsUpdateStats BuildSingleBoundsStats(AABB3 worldBounds, bool rebuilt)
        {
            BoundsUpdateStats stats{};
            stats.visited = 1;
            stats.rebuilt = rebuilt ? 1 : 0;
            stats.invalid = IsValid(worldBounds) ? 0 : 1;
            stats.sceneBounds = worldBounds;
            return stats;
        }

        void PushWarning(DestructionResult& result, std::string warning)
        {
            if (!warning.empty())
            {
                result.warnings.push_back(std::move(warning));
            }
        }

        void RefreshSummary(DestructionResult& result)
        {
            std::ostringstream out;
            out << (result.ok ? "ok" : "failed")
                << " changed=" << result.csg.stats.changedVoxels
                << " solids=" << result.csg.stats.resultSolidVoxels
                << " proxies=" << result.collisionProxies.csgProxies.size()
                << " colliders=" << result.collisionProxies.physicsColliders.size()
                << " generated=" << (result.generatedGeometry.created ? "yes" : "no")
                << " dirty_cells=" << result.dirtyCells.size()
                << " warnings=" << result.warnings.size();
            result.summary = out.str();
        }
    }

    DestructibleComponent& DestructionRegistry::Add(EntityId entity, DestructibleComponent component)
    {
        component.fracture = SanitizeFractureSettings(component.fracture);
        return mComponents[entity] = std::move(component);
    }

    bool DestructionRegistry::Remove(EntityId entity)
    {
        return mComponents.erase(entity) > 0;
    }

    bool DestructionRegistry::Has(EntityId entity) const
    {
        return mComponents.find(entity) != mComponents.end();
    }

    DestructibleComponent* DestructionRegistry::Get(EntityId entity)
    {
        const auto it = mComponents.find(entity);
        return it == mComponents.end() ? nullptr : &it->second;
    }

    const DestructibleComponent* DestructionRegistry::Get(EntityId entity) const
    {
        const auto it = mComponents.find(entity);
        return it == mComponents.end() ? nullptr : &it->second;
    }

    std::size_t DestructionRegistry::Count() const
    {
        return mComponents.size();
    }

    void DestructionRegistry::Clear()
    {
        mComponents.clear();
    }

    const char* ToString(DestructionMaterialKind kind)
    {
        switch (kind)
        {
            case DestructionMaterialKind::Concrete:
                return "concrete";
            case DestructionMaterialKind::Wood:
                return "wood";
            case DestructionMaterialKind::Metal:
                return "metal";
            case DestructionMaterialKind::Glass:
                return "glass";
            case DestructionMaterialKind::Terrain:
                return "terrain";
            case DestructionMaterialKind::Custom:
                return "custom";
            default:
                return "unknown";
        }
    }

    const char* ToString(DestructionSourceKind kind)
    {
        switch (kind)
        {
            case DestructionSourceKind::Primitive:
                return "primitive";
            case DestructionSourceKind::MeshAsset:
                return "mesh-asset";
            default:
                return "unknown";
        }
    }

    DestructionMaterial MakeDestructionMaterial(DestructionMaterialKind kind)
    {
        DestructionMaterial material{};
        material.kind = kind;
        material.name = ToString(kind);

        switch (kind)
        {
            case DestructionMaterialKind::Wood:
                material.densityKgPerCubicMeter = 650.0f;
                material.fractureResistance = 0.45f;
                material.friction = 0.55f;
                material.restitution = 0.12f;
                break;
            case DestructionMaterialKind::Metal:
                material.densityKgPerCubicMeter = 7850.0f;
                material.fractureResistance = 2.5f;
                material.friction = 0.45f;
                material.restitution = 0.08f;
                material.generateDebris = false;
                break;
            case DestructionMaterialKind::Glass:
                material.densityKgPerCubicMeter = 2500.0f;
                material.fractureResistance = 0.18f;
                material.friction = 0.25f;
                material.restitution = 0.02f;
                break;
            case DestructionMaterialKind::Terrain:
                material.densityKgPerCubicMeter = 1800.0f;
                material.fractureResistance = 0.75f;
                material.friction = 0.9f;
                material.restitution = 0.0f;
                break;
            case DestructionMaterialKind::Custom:
                material.name = "custom";
                break;
            case DestructionMaterialKind::Concrete:
            default:
                break;
        }

        return material;
    }

    FractureSettings SanitizeFractureSettings(FractureSettings settings)
    {
        settings.resolutionX = std::clamp(settings.resolutionX, MinimumDestructionResolution, MaximumDestructionResolution);
        settings.resolutionY = std::clamp(settings.resolutionY, MinimumDestructionResolution, MaximumDestructionResolution);
        settings.resolutionZ = std::clamp(settings.resolutionZ, MinimumDestructionResolution, MaximumDestructionResolution);
        settings.maxCollisionProxies = std::max<u32>(1, settings.maxCollisionProxies);
        if (settings.collisionProxyMode == CsgCollisionProxyMode::None && settings.updatePhysicsProxies)
        {
            settings.collisionProxyMode = CsgCollisionProxyMode::GreedyXAxisAABB;
        }
        return settings;
    }

    CsgDestructionRequest SanitizeDestructionRequest(CsgDestructionRequest request)
    {
        request.fracture = SanitizeFractureSettings(request.fracture);
        if (!IsValid(request.localBounds))
        {
            request.localBounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, SafeExtents(MakeAABB3FromCenterExtents(request.sourcePrimitive.center, request.sourcePrimitive.halfExtents)));
        }
        if (!IsFinite(request.worldPosition))
        {
            request.worldPosition = {};
        }
        if (request.debugName.empty())
        {
            request.debugName = "destruction";
        }
        return request;
    }

    AABB3 ComputeCollisionProxyBounds(const std::vector<CsgCollisionProxy>& proxies, AABB3 fallbackBounds)
    {
        AABB3 result = MakeEmptyAABB3();
        for (const CsgCollisionProxy& proxy : proxies)
        {
            if (!IsValid(proxy.bounds))
            {
                continue;
            }
            result = IsValid(result) ? Union(result, proxy.bounds) : proxy.bounds;
        }
        return IsValid(result) ? result : fallbackBounds;
    }

    u64 EstimateDestructionGeometryBytes(const CsgBooleanResult& csg)
    {
        const u64 solidVoxels = csg.stats.resultSolidVoxels;
        const u64 proxyBytes = EstimateCollisionProxyBytes(csg.collisionProxies);
        return solidVoxels * BytesPerGeneratedVoxel + proxyBytes;
    }

    u64 EstimateCollisionProxyBytes(const std::vector<CsgCollisionProxy>& proxies)
    {
        return static_cast<u64>(proxies.size()) * BytesPerCollisionProxy;
    }

    std::string BuildGeneratedDestructionLogicalPath(EntityId entity, u64 frameIndex, std::string_view debugName)
    {
        std::ostringstream out;
        out << "generated:/destruction/e" << entity.index << "_g" << entity.generation
            << "_f" << frameIndex << "_" << SanitizeDebugName(debugName) << ".akmesh";
        return out.str();
    }

    DestructionResult ExecuteCsgDestruction(const CsgDestructionRequest& inputRequest)
    {
        const CsgDestructionRequest request = SanitizeDestructionRequest(inputRequest);

        DestructionResult result{};
        result.event.entity = request.entity;
        result.event.operation = request.operation;
        result.event.frameIndex = request.frameIndex;

        if (!request.entity.IsValid())
        {
            PushWarning(result, "destruction request rejected: invalid entity id");
            RefreshSummary(result);
            return result;
        }

        if (request.sourceKind == DestructionSourceKind::MeshAsset)
        {
            result.csg = BuildMeshPrimitiveBoolean(
                request.sourceMesh,
                request.cutterPrimitive,
                request.operation,
                request.localBounds,
                request.fracture.resolutionX,
                request.fracture.resolutionY,
                request.fracture.resolutionZ,
                request.fracture.collisionProxyMode);
        }
        else
        {
            result.csg = BuildPrimitiveBoolean(
                request.sourcePrimitive,
                request.cutterPrimitive,
                request.operation,
                request.localBounds,
                request.fracture.resolutionX,
                request.fracture.resolutionY,
                request.fracture.resolutionZ,
                request.fracture.collisionProxyMode);
        }

        result.ok = result.csg.stats.ok && result.csg.stats.resultSolidVoxels > 0;
        result.event.changedVoxels = result.csg.stats.changedVoxels;
        result.event.collisionProxies = result.csg.stats.collisionProxyCount;
        result.event.ok = result.ok;
        result.event.message = result.ok ? "destruction result generated" : "destruction result failed";

        result.collisionProxies.bodyId = BodyIdFromEntity(request.entity);
        result.collisionProxies.csgProxies = result.csg.collisionProxies;
        if (result.collisionProxies.csgProxies.size() > request.fracture.maxCollisionProxies)
        {
            result.collisionProxies.csgProxies.resize(request.fracture.maxCollisionProxies);
            PushWarning(result, "collision proxy set truncated by FractureSettings::maxCollisionProxies");
        }
        result.collisionProxies.localBounds = ComputeCollisionProxyBounds(result.collisionProxies.csgProxies, request.localBounds);
        result.collisionProxies.estimatedBytes = EstimateCollisionProxyBytes(result.collisionProxies.csgProxies);
        result.collisionProxies.physicsColliders = MakeProxyCollidersFromCsg(result.collisionProxies.bodyId, result.collisionProxies.csgProxies, request.fracture.maxCollisionProxies);

        result.physicsProxy.bodyId = result.collisionProxies.bodyId;
        result.physicsProxy.bodyKind = PhysicsBodyKind::Static;
        result.physicsProxy.localBounds = result.collisionProxies.localBounds;
        result.physicsProxy.colliderCount = static_cast<u32>(result.collisionProxies.physicsColliders.size());
        result.physicsProxy.estimatedBytes = result.collisionProxies.estimatedBytes;
        result.physicsProxy.active = result.physicsProxy.colliderCount > 0;

        result.generatedGeometry.localBounds = result.collisionProxies.localBounds;
        result.generatedGeometry.estimatedBytes = EstimateDestructionGeometryBytes(result.csg);
        result.generatedGeometry.logicalPath = BuildGeneratedDestructionLogicalPath(request.entity, request.frameIndex, request.debugName);
        result.generatedGeometry.guid = BuildAssetGuidFromNormalizedPath(result.generatedGeometry.logicalPath);

        DirtyWorldCell dirty{};
        dirty.id = MakeWorldPartitionCellId(request.worldPosition);
        dirty.generatedAsset = result.generatedGeometry.guid;
        dirty.estimatedBytes = result.generatedGeometry.estimatedBytes;
        result.dirtyCells.push_back(dirty);

        for (const std::string& warning : result.csg.stats.warnings)
        {
            PushWarning(result, warning);
        }

        RefreshSummary(result);
        return result;
    }

    bool ApplyDestructionResult(const CsgDestructionRequest& inputRequest, DestructionResult& result, DestructionApplyContext context)
    {
        const CsgDestructionRequest request = SanitizeDestructionRequest(inputRequest);
        bool applied = result.ok;

        if (!result.ok)
        {
            RefreshSummary(result);
            return false;
        }

        if (context.world && context.world->IsAlive(request.entity))
        {
            if (MeshComponent* mesh = context.world->GetMesh(request.entity))
            {
                mesh->mesh = result.generatedGeometry.logicalPath;
            }
            else
            {
                context.world->AddMesh(request.entity).mesh = result.generatedGeometry.logicalPath;
            }

            if (request.fracture.updateBounds)
            {
                BoundsComponent& bounds = context.world->AddBounds(request.entity);
                bounds.localBounds = result.generatedGeometry.localBounds;
                bounds.dirtyFlags |= BoundsDirty_Local | BoundsDirty_World | BoundsDirty_Sphere | BoundsDirty_Visibility;
                if (TransformComponent* transform = context.world->GetTransform(request.entity))
                {
                    transform->dirtyFlags |= TransformDirty_Bounds | TransformDirty_RenderProxy | TransformDirty_PhysicsProxy;
                    ++transform->revision;
                }
                const bool rebuilt = RebuildEntityBounds(*context.world, request.entity, true);
                if (const BoundsComponent* rebuiltBounds = context.world->GetBounds(request.entity))
                {
                    result.boundsStats = BuildSingleBoundsStats(rebuiltBounds->worldBounds, rebuilt);
                }
            }
        }
        else
        {
            PushWarning(result, "destruction apply skipped ECS update: entity is not alive");
            applied = false;
        }

        if (context.physics && request.fracture.updatePhysicsProxies)
        {
            if (!HasBody(*context.physics, result.physicsProxy.bodyId))
            {
                context.physics->bodies.push_back(MakeStaticBody(result.physicsProxy.bodyId, GetLocalFloat(request.worldPosition)));
            }
            context.physics->colliders.erase(
                std::remove_if(context.physics->colliders.begin(), context.physics->colliders.end(), [&result](const PhysicsCollider& collider)
                {
                    return collider.bodyId == result.physicsProxy.bodyId && collider.kind == PhysicsColliderKind::ProxyAABB;
                }),
                context.physics->colliders.end());
            context.physics->colliders.insert(context.physics->colliders.end(), result.collisionProxies.physicsColliders.begin(), result.collisionProxies.physicsColliders.end());
        }

        if (context.resources && request.fracture.createGeneratedMeshResource)
        {
            result.generatedGeometry.handle = context.resources->Create(
                {ResourceType::Mesh, result.generatedGeometry.guid, result.generatedGeometry.logicalPath, result.generatedGeometry.estimatedBytes},
                request.frameIndex);
            result.generatedGeometry.created = result.generatedGeometry.handle.IsValid();
        }

        if (context.worldPartition && request.fracture.updateWorldPartition)
        {
            for (const DirtyWorldCell& dirty : result.dirtyCells)
            {
                context.worldPartition->AddAssetRef(dirty.id, {dirty.generatedAsset, result.generatedGeometry.logicalPath, dirty.estimatedBytes});
            }
        }

        if (context.diagnostics && request.fracture.recordDiagnostics)
        {
            context.diagnostics->SetCounter("destruction.changed_voxels", static_cast<i64>(result.csg.stats.changedVoxels));
            context.diagnostics->SetCounter("destruction.collision_proxies", static_cast<i64>(result.collisionProxies.csgProxies.size()));
            context.diagnostics->SetCounter("destruction.generated_bytes", static_cast<i64>(result.generatedGeometry.estimatedBytes));
            context.diagnostics->AddEvent(result.ok ? DiagnosticSeverity::Info : DiagnosticSeverity::Warning, "Destruction", result.event.message);
        }

        RefreshSummary(result);
        return applied;
    }

    std::string ToDebugString(const DestructionMaterial& material)
    {
        std::ostringstream out;
        out << material.name
            << " kind=" << ToString(material.kind)
            << " density=" << material.densityKgPerCubicMeter
            << " resistance=" << material.fractureResistance
            << " friction=" << material.friction
            << " restitution=" << material.restitution
            << " debris=" << (material.generateDebris ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const FractureSettings& settings)
    {
        std::ostringstream out;
        out << settings.resolutionX << "x" << settings.resolutionY << "x" << settings.resolutionZ
            << " mode=" << ToString(settings.collisionProxyMode)
            << " max_proxies=" << settings.maxCollisionProxies
            << " bounds=" << (settings.updateBounds ? "yes" : "no")
            << " physics=" << (settings.updatePhysicsProxies ? "yes" : "no")
            << " partition=" << (settings.updateWorldPartition ? "yes" : "no")
            << " resource=" << (settings.createGeneratedMeshResource ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const CollisionProxySet& proxies)
    {
        std::ostringstream out;
        out << "body=" << proxies.bodyId
            << " csg=" << proxies.csgProxies.size()
            << " colliders=" << proxies.physicsColliders.size()
            << " bytes=" << proxies.estimatedBytes;
        if (IsValid(proxies.localBounds))
        {
            out << " size=" << AK::ToDebugString(Size(proxies.localBounds), 3);
        }
        return out.str();
    }

    std::string ToDebugString(const PhysicsProxyDesc& proxy)
    {
        std::ostringstream out;
        out << "body=" << proxy.bodyId
            << " kind=" << ToString(proxy.bodyKind)
            << " colliders=" << proxy.colliderCount
            << " active=" << (proxy.active ? "yes" : "no")
            << " bytes=" << proxy.estimatedBytes;
        return out.str();
    }

    std::string ToDebugString(const GeneratedGeometryDesc& geometry)
    {
        std::ostringstream out;
        out << "path=" << geometry.logicalPath
            << " handle=" << ToString(geometry.handle)
            << " created=" << (geometry.created ? "yes" : "no")
            << " bytes=" << geometry.estimatedBytes;
        return out.str();
    }

    std::string ToDebugString(const DestructionResult& result)
    {
        std::ostringstream out;
        out << "destruction " << result.summary
            << " physics=[" << ToDebugString(result.physicsProxy) << "]"
            << " generated=[" << ToDebugString(result.generatedGeometry) << "]";
        return out.str();
    }

    DestructionProbeResult BuildDestructionProbe()
    {
        World world;
        const EntityId entity = world.CreateEntity("DestructibleWall");
        TransformComponent& transform = world.AddTransform(entity);
        transform.position = {0.0f, 0.0f, 0.0f};
        transform.scale = {1.0f, 1.0f, 1.0f};
        world.AddMesh(entity).mesh = "builtin:cube";
        world.AddWorldPosition(entity).position = MakeWorldPosition(2049.0, 0.0, 2049.0);

        DestructionRegistry registry;
        DestructibleComponent component{};
        component.material = MakeDestructionMaterial(DestructionMaterialKind::Concrete);
        component.fracture.resolutionX = 12;
        component.fracture.resolutionY = 12;
        component.fracture.resolutionZ = 12;
        component.fracture.maxCollisionProxies = 128;
        registry.Add(entity, component);

        CsgDestructionRequest request{};
        request.entity = entity;
        request.sourceKind = DestructionSourceKind::Primitive;
        request.sourcePrimitive = MakeCsgBox({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
        request.cutterPrimitive = MakeCsgSphere({0.55f, 0.0f, 0.0f}, 0.72f);
        request.operation = CsgBooleanOperation::Difference;
        request.localBounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {1.1f, 1.1f, 1.1f});
        request.worldPosition = world.GetWorldPosition(entity)->position;
        request.fracture = component.fracture;
        request.debugName = "probe_wall_cut";
        request.frameIndex = 77;

        PhysicsScene physics;
        ResourceManager resources({3});
        WorldPartition partition({1024.0, 1, 2, 4, 8, 8});
        DiagnosticsHub diagnostics(8);
        diagnostics.BeginFrame(request.frameIndex);

        DestructionResult result = ExecuteCsgDestruction(request);
        const bool applied = ApplyDestructionResult(request, result, {&world, &physics, &resources, &partition, &diagnostics});
        diagnostics.EndFrame();

        DestructionProbeResult probe{};
        probe.result = result;
        probe.physics = physics;
        probe.resourceStats = resources.Stats();
        probe.partitionStats = partition.Stats(request.worldPosition);
        probe.diagnostics = diagnostics.Snapshot();

        const MeshComponent* mesh = world.GetMesh(entity);
        const BoundsComponent* bounds = world.GetBounds(entity);
        const bool meshUpdated = mesh && mesh->mesh == result.generatedGeometry.logicalPath;
        const bool boundsUpdated = bounds && IsValid(bounds->worldBounds);
        const bool physicsUpdated = !physics.colliders.empty() && !physics.bodies.empty();
        const bool resourceCreated = result.generatedGeometry.created && resources.IsAlive(result.generatedGeometry.handle);
        const bool partitionUpdated = probe.partitionStats.assetRefs > 0;
        const bool diagnosticsUpdated = probe.diagnostics.counterCount >= 3 && probe.diagnostics.eventCount >= 1;

        probe.ok = result.ok
            && applied
            && registry.Has(entity)
            && meshUpdated
            && boundsUpdated
            && physicsUpdated
            && resourceCreated
            && partitionUpdated
            && diagnosticsUpdated;

        std::ostringstream out;
        out << "Destruction bridge probe: " << (probe.ok ? "ok" : "failed")
            << " " << result.summary
            << " physics_bodies=" << physics.bodies.size()
            << " physics_colliders=" << physics.colliders.size()
            << " resource_alive=" << resources.Stats().alive
            << " partition_refs=" << probe.partitionStats.assetRefs
            << " diag_events=" << probe.diagnostics.eventCount;
        probe.summary = out.str();
        return probe;
    }

    std::string BuildDestructionProbeSummary()
    {
        return BuildDestructionProbe().summary;
    }
}
