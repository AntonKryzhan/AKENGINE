#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class CsgPrimitiveKind : u32
    {
        Box = 0,
        Sphere = 1,
        CylinderY = 2,
        CapsuleY = 3
    };

    enum class CsgBooleanOperation : u32
    {
        Union = 0,
        Intersection = 1,
        Difference = 2
    };

    enum class CsgVoxelizationMode : u32
    {
        PrimitiveSdf = 0,
        MeshOddEvenRaycast = 1
    };

    enum class CsgCollisionProxyMode : u32
    {
        None = 0,
        PerVoxelAABB = 1,
        GreedyXAxisAABB = 2
    };

    struct CsgPrimitiveDesc
    {
        CsgPrimitiveKind kind = CsgPrimitiveKind::Box;
        Vec3 center{0.0f, 0.0f, 0.0f};
        Vec3 halfExtents{0.5f, 0.5f, 0.5f};
        float radius = 0.5f;
        float height = 1.0f;
    };

    struct CsgTriangle
    {
        Vec3 a{};
        Vec3 b{};
        Vec3 c{};
    };

    struct CsgMeshAsset
    {
        std::string name;
        std::vector<CsgTriangle> triangles;
        AABB3 bounds = MakeEmptyAABB3();
        bool declaredWatertight = false;
    };

    struct CsgVoxelGrid
    {
        AABB3 bounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        u32 resolutionX = 1;
        u32 resolutionY = 1;
        u32 resolutionZ = 1;
        std::vector<u8> solid;
    };

    struct CsgCollisionProxy
    {
        AABB3 bounds{};
        u32 voxelCount = 0;
    };

    struct CsgBooleanStats
    {
        CsgBooleanOperation operation = CsgBooleanOperation::Difference;
        CsgVoxelizationMode leftMode = CsgVoxelizationMode::PrimitiveSdf;
        CsgVoxelizationMode rightMode = CsgVoxelizationMode::PrimitiveSdf;
        CsgCollisionProxyMode proxyMode = CsgCollisionProxyMode::GreedyXAxisAABB;
        u32 resolutionX = 0;
        u32 resolutionY = 0;
        u32 resolutionZ = 0;
        u64 sourceSolidVoxels = 0;
        u64 toolSolidVoxels = 0;
        u64 resultSolidVoxels = 0;
        u64 changedVoxels = 0;
        u64 triangleTests = 0;
        u32 collisionProxyCount = 0;
        u64 estimatedCollisionBytes = 0;
        bool watertightInputRequired = false;
        bool ok = false;
        std::vector<std::string> warnings;
    };

    struct CsgBooleanResult
    {
        CsgVoxelGrid grid{};
        std::vector<CsgCollisionProxy> collisionProxies;
        CsgBooleanStats stats{};
        std::string summary;
    };

    struct CsgProbeResult
    {
        bool ok = false;
        CsgBooleanResult primitiveDifference{};
        CsgBooleanResult meshDifference{};
        std::string summary;
    };

    const char* ToString(CsgPrimitiveKind kind);
    const char* ToString(CsgBooleanOperation operation);
    const char* ToString(CsgVoxelizationMode mode);
    const char* ToString(CsgCollisionProxyMode mode);

    std::string ToDebugString(const CsgVoxelGrid& grid);
    std::string ToDebugString(const CsgCollisionProxy& proxy);
    std::string ToDebugString(const CsgBooleanStats& stats);

    CsgPrimitiveDesc MakeCsgBox(Vec3 center, Vec3 halfExtents);
    CsgPrimitiveDesc MakeCsgSphere(Vec3 center, float radius);
    CsgPrimitiveDesc MakeCsgCylinderY(Vec3 center, float radius, float height);
    CsgPrimitiveDesc MakeCsgCapsuleY(Vec3 center, float radius, float height);

    CsgMeshAsset MakeCsgBoxMeshAsset(std::string name, Vec3 center, Vec3 halfExtents);

    float EvaluateSignedDistance(const CsgPrimitiveDesc& primitive, Vec3 point);

    CsgVoxelGrid MakeCsgVoxelGrid(AABB3 bounds, u32 resolutionX, u32 resolutionY, u32 resolutionZ);
    u64 CsgCellCount(const CsgVoxelGrid& grid);
    u64 CsgCellIndex(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z);
    Vec3 CsgCellSize(const CsgVoxelGrid& grid);
    Vec3 CsgCellCenter(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z);
    AABB3 CsgCellBounds(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z);
    bool CsgIsSolid(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z);
    void CsgSetSolid(CsgVoxelGrid& grid, u32 x, u32 y, u32 z, bool solid);
    u64 CountSolidVoxels(const CsgVoxelGrid& grid);

    CsgVoxelGrid VoxelizePrimitive(const CsgPrimitiveDesc& primitive, AABB3 bounds, u32 resolutionX, u32 resolutionY, u32 resolutionZ);
    CsgVoxelGrid VoxelizeMeshOddEven(const CsgMeshAsset& mesh, AABB3 bounds, u32 resolutionX, u32 resolutionY, u32 resolutionZ, u64* outTriangleTests = nullptr);

    CsgVoxelGrid ApplyBooleanOperation(const CsgVoxelGrid& left, const CsgVoxelGrid& right, CsgBooleanOperation operation, u64* outChangedVoxels = nullptr);
    std::vector<CsgCollisionProxy> ExtractCollisionProxies(const CsgVoxelGrid& grid, CsgCollisionProxyMode mode);

    CsgBooleanResult BuildPrimitiveBoolean(
        const CsgPrimitiveDesc& left,
        const CsgPrimitiveDesc& right,
        CsgBooleanOperation operation,
        AABB3 bounds,
        u32 resolutionX,
        u32 resolutionY,
        u32 resolutionZ,
        CsgCollisionProxyMode proxyMode = CsgCollisionProxyMode::GreedyXAxisAABB);

    CsgBooleanResult BuildMeshPrimitiveBoolean(
        const CsgMeshAsset& leftMesh,
        const CsgPrimitiveDesc& right,
        CsgBooleanOperation operation,
        AABB3 bounds,
        u32 resolutionX,
        u32 resolutionY,
        u32 resolutionZ,
        CsgCollisionProxyMode proxyMode = CsgCollisionProxyMode::GreedyXAxisAABB);

    CsgProbeResult BuildCsgProbe();
    std::string BuildCsgProbeSummary();
}
