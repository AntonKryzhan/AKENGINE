#pragma once

#include <AK/Core/Handle.hpp>
#include <AK/Core/Types.hpp>
#include <AK/CSG/Boolean.hpp>
#include <AK/Diagnostics/Diagnostics.hpp>
#include <AK/ECS/EntityId.hpp>
#include <AK/ECS/World.hpp>
#include <AK/Physics/Physics.hpp>
#include <AK/Resources/ResourceManager.hpp>
#include <AK/Visibility/Bounds.hpp>
#include <AK/World/WorldCoordinates.hpp>
#include <AK/WorldPartition/WorldPartition.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace AK
{
    enum class DestructionMaterialKind : u32
    {
        Concrete = 0,
        Wood = 1,
        Metal = 2,
        Glass = 3,
        Terrain = 4,
        Custom = 5
    };

    enum class DestructionSourceKind : u32
    {
        Primitive = 0,
        MeshAsset = 1
    };

    struct DestructionMaterial
    {
        DestructionMaterialKind kind = DestructionMaterialKind::Concrete;
        std::string name = "concrete";
        float densityKgPerCubicMeter = 2400.0f;
        float fractureResistance = 1.0f;
        float friction = 0.7f;
        float restitution = 0.05f;
        bool generateDebris = true;
    };

    struct FractureSettings
    {
        u32 resolutionX = 16;
        u32 resolutionY = 16;
        u32 resolutionZ = 16;
        u32 maxCollisionProxies = 128;
        CsgCollisionProxyMode collisionProxyMode = CsgCollisionProxyMode::GreedyXAxisAABB;
        bool updateBounds = true;
        bool updatePhysicsProxies = true;
        bool updateWorldPartition = true;
        bool createGeneratedMeshResource = true;
        bool recordDiagnostics = true;
    };

    struct DestructibleComponent
    {
        bool enabled = true;
        DestructionMaterial material{};
        FractureSettings fracture{};
        ResourceHandle generatedMesh{};
        u64 revision = 1;
    };

    struct CsgDestructionRequest
    {
        EntityId entity = InvalidEntity;
        DestructionSourceKind sourceKind = DestructionSourceKind::Primitive;
        CsgPrimitiveDesc sourcePrimitive = MakeCsgBox({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
        CsgMeshAsset sourceMesh{};
        CsgPrimitiveDesc cutterPrimitive = MakeCsgSphere({0.35f, 0.0f, 0.0f}, 0.65f);
        CsgBooleanOperation operation = CsgBooleanOperation::Difference;
        AABB3 localBounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {1.1f, 1.1f, 1.1f});
        WorldPosition worldPosition{};
        FractureSettings fracture{};
        std::string debugName = "destruction";
        u64 frameIndex = 0;
    };

    struct CollisionProxySet
    {
        u32 bodyId = 0;
        AABB3 localBounds = MakeEmptyAABB3();
        std::vector<CsgCollisionProxy> csgProxies;
        std::vector<PhysicsCollider> physicsColliders;
        u64 estimatedBytes = 0;
    };

    struct PhysicsProxyDesc
    {
        u32 bodyId = 0;
        PhysicsBodyKind bodyKind = PhysicsBodyKind::Static;
        AABB3 localBounds = MakeEmptyAABB3();
        u32 colliderCount = 0;
        u64 estimatedBytes = 0;
        bool active = false;
    };

    struct GeneratedGeometryDesc
    {
        AssetGuid guid{};
        ResourceHandle handle{};
        std::string logicalPath;
        AABB3 localBounds = MakeEmptyAABB3();
        u64 estimatedBytes = 0;
        bool created = false;
    };

    struct DirtyWorldCell
    {
        WorldPartitionCellId id{};
        AssetGuid generatedAsset{};
        u64 estimatedBytes = 0;
    };

    struct DestructionEvent
    {
        EntityId entity = InvalidEntity;
        CsgBooleanOperation operation = CsgBooleanOperation::Difference;
        u64 frameIndex = 0;
        u64 changedVoxels = 0;
        u32 collisionProxies = 0;
        bool ok = false;
        std::string message;
    };

    struct DestructionResult
    {
        bool ok = false;
        CsgBooleanResult csg{};
        CollisionProxySet collisionProxies{};
        PhysicsProxyDesc physicsProxy{};
        GeneratedGeometryDesc generatedGeometry{};
        std::vector<DirtyWorldCell> dirtyCells;
        BoundsUpdateStats boundsStats{};
        DestructionEvent event{};
        std::vector<std::string> warnings;
        std::string summary;
    };

    struct DestructionApplyContext
    {
        World* world = nullptr;
        PhysicsScene* physics = nullptr;
        ResourceManager* resources = nullptr;
        WorldPartition* worldPartition = nullptr;
        DiagnosticsHub* diagnostics = nullptr;
    };

    struct DestructionProbeResult
    {
        bool ok = false;
        DestructionResult result{};
        PhysicsScene physics{};
        ResourceStats resourceStats{};
        WorldPartitionStats partitionStats{};
        DiagnosticsSnapshot diagnostics{};
        std::string summary;
    };

    class DestructionRegistry final
    {
    public:
        DestructibleComponent& Add(EntityId entity, DestructibleComponent component = {});
        bool Remove(EntityId entity);
        bool Has(EntityId entity) const;
        DestructibleComponent* Get(EntityId entity);
        const DestructibleComponent* Get(EntityId entity) const;
        std::size_t Count() const;
        void Clear();

    private:
        std::unordered_map<EntityId, DestructibleComponent> mComponents;
    };

    const char* ToString(DestructionMaterialKind kind);
    const char* ToString(DestructionSourceKind kind);

    DestructionMaterial MakeDestructionMaterial(DestructionMaterialKind kind);
    FractureSettings SanitizeFractureSettings(FractureSettings settings);
    CsgDestructionRequest SanitizeDestructionRequest(CsgDestructionRequest request);

    AABB3 ComputeCollisionProxyBounds(const std::vector<CsgCollisionProxy>& proxies, AABB3 fallbackBounds = MakeEmptyAABB3());
    u64 EstimateDestructionGeometryBytes(const CsgBooleanResult& csg);
    u64 EstimateCollisionProxyBytes(const std::vector<CsgCollisionProxy>& proxies);
    std::string BuildGeneratedDestructionLogicalPath(EntityId entity, u64 frameIndex, std::string_view debugName);

    DestructionResult ExecuteCsgDestruction(const CsgDestructionRequest& request);
    bool ApplyDestructionResult(const CsgDestructionRequest& request, DestructionResult& result, DestructionApplyContext context);

    std::string ToDebugString(const DestructionMaterial& material);
    std::string ToDebugString(const FractureSettings& settings);
    std::string ToDebugString(const CollisionProxySet& proxies);
    std::string ToDebugString(const PhysicsProxyDesc& proxy);
    std::string ToDebugString(const GeneratedGeometryDesc& geometry);
    std::string ToDebugString(const DestructionResult& result);

    DestructionProbeResult BuildDestructionProbe();
    std::string BuildDestructionProbeSummary();
}
