#include <AK/CSG/Boolean.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr float CsgEpsilon = 1.0e-5f;

        float Abs(float value)
        {
            return std::fabs(value);
        }

        float MaxComponent(Vec3 value)
        {
            return std::max(value.x, std::max(value.y, value.z));
        }

        Vec3 AbsVec3(Vec3 value)
        {
            return {Abs(value.x), Abs(value.y), Abs(value.z)};
        }

        float Length2D(float x, float y)
        {
            return std::sqrt(x * x + y * y);
        }

        AABB3 BuildTriangleBounds(const CsgTriangle& triangle)
        {
            AABB3 bounds = MakeEmptyAABB3();
            bounds = Expand(bounds, triangle.a);
            bounds = Expand(bounds, triangle.b);
            bounds = Expand(bounds, triangle.c);
            return bounds;
        }

        bool RayIntersectsTriangleX(Vec3 origin, const CsgTriangle& triangle, float* outT)
        {
            const Vec3 direction{1.0f, 0.0f, 0.0f};
            const Vec3 edge1 = Subtract(triangle.b, triangle.a);
            const Vec3 edge2 = Subtract(triangle.c, triangle.a);
            const Vec3 p = Cross(direction, edge2);
            const float determinant = Dot(edge1, p);
            if (Abs(determinant) <= CsgEpsilon)
            {
                return false;
            }

            const float invDeterminant = 1.0f / determinant;
            const Vec3 tvec = Subtract(origin, triangle.a);
            const float u = Dot(tvec, p) * invDeterminant;
            if (u < -CsgEpsilon || u > 1.0f + CsgEpsilon)
            {
                return false;
            }

            const Vec3 q = Cross(tvec, edge1);
            const float v = Dot(direction, q) * invDeterminant;
            if (v < -CsgEpsilon || (u + v) > 1.0f + CsgEpsilon)
            {
                return false;
            }

            const float t = Dot(edge2, q) * invDeterminant;
            if (t <= CsgEpsilon)
            {
                return false;
            }

            if (outT)
            {
                *outT = t;
            }
            return true;
        }

        bool IsPointInsideMeshOddEven(Vec3 point, const CsgMeshAsset& mesh, u64* triangleTests)
        {
            std::vector<float> hits;
            hits.reserve(16);

            for (const CsgTriangle& triangle : mesh.triangles)
            {
                if (triangleTests)
                {
                    ++(*triangleTests);
                }

                const AABB3 triangleBounds = BuildTriangleBounds(triangle);
                if (point.y < triangleBounds.min.y - CsgEpsilon || point.y > triangleBounds.max.y + CsgEpsilon)
                {
                    continue;
                }
                if (point.z < triangleBounds.min.z - CsgEpsilon || point.z > triangleBounds.max.z + CsgEpsilon)
                {
                    continue;
                }

                float t = 0.0f;
                if (RayIntersectsTriangleX(point, triangle, &t))
                {
                    hits.push_back(t);
                }
            }

            if (hits.empty())
            {
                return false;
            }

            std::sort(hits.begin(), hits.end());

            u32 uniqueHits = 0;
            float previous = -std::numeric_limits<float>::max();
            for (float t : hits)
            {
                if (Abs(t - previous) > 1.0e-4f)
                {
                    ++uniqueHits;
                    previous = t;
                }
            }

            return (uniqueHits & 1u) == 1u;
        }

        std::string BuildSummary(const CsgBooleanStats& stats)
        {
            std::ostringstream out;
            out << "CSG " << ToString(stats.operation)
                << " " << stats.resolutionX << "x" << stats.resolutionY << "x" << stats.resolutionZ
                << " source=" << stats.sourceSolidVoxels
                << " tool=" << stats.toolSolidVoxels
                << " result=" << stats.resultSolidVoxels
                << " changed=" << stats.changedVoxels
                << " proxies=" << stats.collisionProxyCount
                << " ok=" << (stats.ok ? "yes" : "no");
            return out.str();
        }

        void AddWarning(CsgBooleanStats& stats, std::string warning)
        {
            stats.warnings.push_back(std::move(warning));
        }
    }

    const char* ToString(CsgPrimitiveKind kind)
    {
        switch (kind)
        {
        case CsgPrimitiveKind::Box:
            return "Box";
        case CsgPrimitiveKind::Sphere:
            return "Sphere";
        case CsgPrimitiveKind::CylinderY:
            return "CylinderY";
        case CsgPrimitiveKind::CapsuleY:
            return "CapsuleY";
        default:
            return "Unknown";
        }
    }

    const char* ToString(CsgBooleanOperation operation)
    {
        switch (operation)
        {
        case CsgBooleanOperation::Union:
            return "Union";
        case CsgBooleanOperation::Intersection:
            return "Intersection";
        case CsgBooleanOperation::Difference:
            return "Difference";
        default:
            return "Unknown";
        }
    }

    const char* ToString(CsgVoxelizationMode mode)
    {
        switch (mode)
        {
        case CsgVoxelizationMode::PrimitiveSdf:
            return "PrimitiveSdf";
        case CsgVoxelizationMode::MeshOddEvenRaycast:
            return "MeshOddEvenRaycast";
        default:
            return "Unknown";
        }
    }

    const char* ToString(CsgCollisionProxyMode mode)
    {
        switch (mode)
        {
        case CsgCollisionProxyMode::None:
            return "None";
        case CsgCollisionProxyMode::PerVoxelAABB:
            return "PerVoxelAABB";
        case CsgCollisionProxyMode::GreedyXAxisAABB:
            return "GreedyXAxisAABB";
        default:
            return "Unknown";
        }
    }

    std::string ToDebugString(const CsgVoxelGrid& grid)
    {
        std::ostringstream out;
        out << "grid " << grid.resolutionX << "x" << grid.resolutionY << "x" << grid.resolutionZ
            << " cells=" << CsgCellCount(grid)
            << " solid=" << CountSolidVoxels(grid)
            << " bounds=[" << ToDebugString(grid.bounds.min, 2) << "]..[" << ToDebugString(grid.bounds.max, 2) << "]";
        return out.str();
    }

    std::string ToDebugString(const CsgCollisionProxy& proxy)
    {
        std::ostringstream out;
        out << "proxy voxels=" << proxy.voxelCount
            << " bounds=[" << ToDebugString(proxy.bounds.min, 2) << "]..[" << ToDebugString(proxy.bounds.max, 2) << "]";
        return out.str();
    }

    std::string ToDebugString(const CsgBooleanStats& stats)
    {
        std::ostringstream out;
        out << "CSG stats op=" << ToString(stats.operation)
            << " left=" << ToString(stats.leftMode)
            << " right=" << ToString(stats.rightMode)
            << " proxy=" << ToString(stats.proxyMode)
            << " res=" << stats.resolutionX << "x" << stats.resolutionY << "x" << stats.resolutionZ
            << " source=" << stats.sourceSolidVoxels
            << " tool=" << stats.toolSolidVoxels
            << " result=" << stats.resultSolidVoxels
            << " changed=" << stats.changedVoxels
            << " triTests=" << stats.triangleTests
            << " proxies=" << stats.collisionProxyCount
            << " collisionBytes~" << stats.estimatedCollisionBytes
            << " ok=" << (stats.ok ? "yes" : "no");
        if (!stats.warnings.empty())
        {
            out << " warnings=" << stats.warnings.size();
        }
        return out.str();
    }

    CsgPrimitiveDesc MakeCsgBox(Vec3 center, Vec3 halfExtents)
    {
        CsgPrimitiveDesc primitive{};
        primitive.kind = CsgPrimitiveKind::Box;
        primitive.center = center;
        primitive.halfExtents = Max(halfExtents, {CsgEpsilon, CsgEpsilon, CsgEpsilon});
        return primitive;
    }

    CsgPrimitiveDesc MakeCsgSphere(Vec3 center, float radius)
    {
        CsgPrimitiveDesc primitive{};
        primitive.kind = CsgPrimitiveKind::Sphere;
        primitive.center = center;
        primitive.radius = std::max(radius, CsgEpsilon);
        primitive.halfExtents = {primitive.radius, primitive.radius, primitive.radius};
        return primitive;
    }

    CsgPrimitiveDesc MakeCsgCylinderY(Vec3 center, float radius, float height)
    {
        CsgPrimitiveDesc primitive{};
        primitive.kind = CsgPrimitiveKind::CylinderY;
        primitive.center = center;
        primitive.radius = std::max(radius, CsgEpsilon);
        primitive.height = std::max(height, CsgEpsilon);
        primitive.halfExtents = {primitive.radius, primitive.height * 0.5f, primitive.radius};
        return primitive;
    }

    CsgPrimitiveDesc MakeCsgCapsuleY(Vec3 center, float radius, float height)
    {
        CsgPrimitiveDesc primitive{};
        primitive.kind = CsgPrimitiveKind::CapsuleY;
        primitive.center = center;
        primitive.radius = std::max(radius, CsgEpsilon);
        primitive.height = std::max(height, primitive.radius * 2.0f);
        primitive.halfExtents = {primitive.radius, primitive.height * 0.5f, primitive.radius};
        return primitive;
    }

    CsgMeshAsset MakeCsgBoxMeshAsset(std::string name, Vec3 center, Vec3 halfExtents)
    {
        const Vec3 he = Max(halfExtents, {CsgEpsilon, CsgEpsilon, CsgEpsilon});
        const std::array<Vec3, 8> p{
            Add(center, {-he.x, -he.y, -he.z}),
            Add(center, { he.x, -he.y, -he.z}),
            Add(center, { he.x,  he.y, -he.z}),
            Add(center, {-he.x,  he.y, -he.z}),
            Add(center, {-he.x, -he.y,  he.z}),
            Add(center, { he.x, -he.y,  he.z}),
            Add(center, { he.x,  he.y,  he.z}),
            Add(center, {-he.x,  he.y,  he.z})
        };

        const std::array<std::array<u32, 3>, 12> indices{{
            {{0u, 2u, 1u}}, {{0u, 3u, 2u}},
            {{4u, 5u, 6u}}, {{4u, 6u, 7u}},
            {{0u, 1u, 5u}}, {{0u, 5u, 4u}},
            {{3u, 6u, 2u}}, {{3u, 7u, 6u}},
            {{1u, 2u, 6u}}, {{1u, 6u, 5u}},
            {{0u, 4u, 7u}}, {{0u, 7u, 3u}}
        }};

        CsgMeshAsset mesh{};
        mesh.name = std::move(name);
        mesh.bounds = MakeAABB3FromCenterExtents(center, he);
        mesh.declaredWatertight = true;
        mesh.triangles.reserve(indices.size());
        for (const std::array<u32, 3>& triangle : indices)
        {
            mesh.triangles.push_back({p[triangle[0]], p[triangle[1]], p[triangle[2]]});
        }
        return mesh;
    }

    float EvaluateSignedDistance(const CsgPrimitiveDesc& primitive, Vec3 point)
    {
        const Vec3 local = Subtract(point, primitive.center);
        switch (primitive.kind)
        {
        case CsgPrimitiveKind::Box:
        {
            const Vec3 q = Subtract(AbsVec3(local), primitive.halfExtents);
            const Vec3 outside{std::max(q.x, 0.0f), std::max(q.y, 0.0f), std::max(q.z, 0.0f)};
            return Length(outside) + std::min(MaxComponent(q), 0.0f);
        }
        case CsgPrimitiveKind::Sphere:
            return Length(local) - primitive.radius;
        case CsgPrimitiveKind::CylinderY:
        {
            const float radial = Length2D(local.x, local.z) - primitive.radius;
            const float axial = Abs(local.y) - primitive.height * 0.5f;
            const float outsideX = std::max(radial, 0.0f);
            const float outsideY = std::max(axial, 0.0f);
            return Length2D(outsideX, outsideY) + std::min(std::max(radial, axial), 0.0f);
        }
        case CsgPrimitiveKind::CapsuleY:
        {
            const float segmentHalf = std::max(0.0f, primitive.height * 0.5f - primitive.radius);
            const Vec3 closest{0.0f, std::clamp(local.y, -segmentHalf, segmentHalf), 0.0f};
            return Length(Subtract(local, closest)) - primitive.radius;
        }
        default:
            return std::numeric_limits<float>::max();
        }
    }

    CsgVoxelGrid MakeCsgVoxelGrid(AABB3 bounds, u32 resolutionX, u32 resolutionY, u32 resolutionZ)
    {
        CsgVoxelGrid grid{};
        grid.bounds = bounds;
        grid.resolutionX = std::max(1u, resolutionX);
        grid.resolutionY = std::max(1u, resolutionY);
        grid.resolutionZ = std::max(1u, resolutionZ);
        grid.solid.assign(static_cast<std::size_t>(CsgCellCount(grid)), 0u);
        return grid;
    }

    u64 CsgCellCount(const CsgVoxelGrid& grid)
    {
        return static_cast<u64>(grid.resolutionX) * static_cast<u64>(grid.resolutionY) * static_cast<u64>(grid.resolutionZ);
    }

    u64 CsgCellIndex(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z)
    {
        return static_cast<u64>(x)
            + static_cast<u64>(grid.resolutionX) * (static_cast<u64>(y) + static_cast<u64>(grid.resolutionY) * static_cast<u64>(z));
    }

    Vec3 CsgCellSize(const CsgVoxelGrid& grid)
    {
        const Vec3 size = Size(grid.bounds);
        return {
            SafeDivide(size.x, static_cast<float>(grid.resolutionX)),
            SafeDivide(size.y, static_cast<float>(grid.resolutionY)),
            SafeDivide(size.z, static_cast<float>(grid.resolutionZ))
        };
    }

    Vec3 CsgCellCenter(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z)
    {
        const Vec3 cell = CsgCellSize(grid);
        return {
            grid.bounds.min.x + (static_cast<float>(x) + 0.5f) * cell.x,
            grid.bounds.min.y + (static_cast<float>(y) + 0.5f) * cell.y,
            grid.bounds.min.z + (static_cast<float>(z) + 0.5f) * cell.z
        };
    }

    AABB3 CsgCellBounds(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z)
    {
        const Vec3 cell = CsgCellSize(grid);
        const Vec3 minValue{
            grid.bounds.min.x + static_cast<float>(x) * cell.x,
            grid.bounds.min.y + static_cast<float>(y) * cell.y,
            grid.bounds.min.z + static_cast<float>(z) * cell.z
        };
        return MakeAABB3(minValue, Add(minValue, cell));
    }

    bool CsgIsSolid(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z)
    {
        const u64 index = CsgCellIndex(grid, x, y, z);
        if (index >= grid.solid.size())
        {
            return false;
        }
        return grid.solid[static_cast<std::size_t>(index)] != 0u;
    }

    void CsgSetSolid(CsgVoxelGrid& grid, u32 x, u32 y, u32 z, bool solid)
    {
        const u64 index = CsgCellIndex(grid, x, y, z);
        if (index < grid.solid.size())
        {
            grid.solid[static_cast<std::size_t>(index)] = solid ? 1u : 0u;
        }
    }

    u64 CountSolidVoxels(const CsgVoxelGrid& grid)
    {
        return static_cast<u64>(std::count(grid.solid.begin(), grid.solid.end(), static_cast<u8>(1u)));
    }

    CsgVoxelGrid VoxelizePrimitive(const CsgPrimitiveDesc& primitive, AABB3 bounds, u32 resolutionX, u32 resolutionY, u32 resolutionZ)
    {
        CsgVoxelGrid grid = MakeCsgVoxelGrid(bounds, resolutionX, resolutionY, resolutionZ);
        for (u32 z = 0; z < grid.resolutionZ; ++z)
        {
            for (u32 y = 0; y < grid.resolutionY; ++y)
            {
                for (u32 x = 0; x < grid.resolutionX; ++x)
                {
                    const Vec3 center = CsgCellCenter(grid, x, y, z);
                    CsgSetSolid(grid, x, y, z, EvaluateSignedDistance(primitive, center) <= 0.0f);
                }
            }
        }
        return grid;
    }

    CsgVoxelGrid VoxelizeMeshOddEven(const CsgMeshAsset& mesh, AABB3 bounds, u32 resolutionX, u32 resolutionY, u32 resolutionZ, u64* outTriangleTests)
    {
        if (outTriangleTests)
        {
            *outTriangleTests = 0;
        }

        CsgVoxelGrid grid = MakeCsgVoxelGrid(bounds, resolutionX, resolutionY, resolutionZ);
        if (mesh.triangles.empty())
        {
            return grid;
        }

        for (u32 z = 0; z < grid.resolutionZ; ++z)
        {
            for (u32 y = 0; y < grid.resolutionY; ++y)
            {
                for (u32 x = 0; x < grid.resolutionX; ++x)
                {
                    const Vec3 center = CsgCellCenter(grid, x, y, z);
                    if (!Contains(mesh.bounds, center))
                    {
                        continue;
                    }
                    CsgSetSolid(grid, x, y, z, IsPointInsideMeshOddEven(center, mesh, outTriangleTests));
                }
            }
        }

        return grid;
    }

    CsgVoxelGrid ApplyBooleanOperation(const CsgVoxelGrid& left, const CsgVoxelGrid& right, CsgBooleanOperation operation, u64* outChangedVoxels)
    {
        CsgVoxelGrid result = MakeCsgVoxelGrid(left.bounds, left.resolutionX, left.resolutionY, left.resolutionZ);
        if (outChangedVoxels)
        {
            *outChangedVoxels = 0;
        }

        const bool compatible = left.resolutionX == right.resolutionX
            && left.resolutionY == right.resolutionY
            && left.resolutionZ == right.resolutionZ
            && left.solid.size() == right.solid.size();
        if (!compatible)
        {
            return result;
        }

        for (std::size_t index = 0; index < left.solid.size(); ++index)
        {
            const bool a = left.solid[index] != 0u;
            const bool b = right.solid[index] != 0u;
            bool value = false;
            switch (operation)
            {
            case CsgBooleanOperation::Union:
                value = a || b;
                break;
            case CsgBooleanOperation::Intersection:
                value = a && b;
                break;
            case CsgBooleanOperation::Difference:
                value = a && !b;
                break;
            default:
                value = false;
                break;
            }

            result.solid[index] = value ? 1u : 0u;
            if (outChangedVoxels && value != a)
            {
                ++(*outChangedVoxels);
            }
        }
        return result;
    }

    std::vector<CsgCollisionProxy> ExtractCollisionProxies(const CsgVoxelGrid& grid, CsgCollisionProxyMode mode)
    {
        std::vector<CsgCollisionProxy> proxies;
        if (mode == CsgCollisionProxyMode::None)
        {
            return proxies;
        }

        if (mode == CsgCollisionProxyMode::PerVoxelAABB)
        {
            proxies.reserve(static_cast<std::size_t>(CountSolidVoxels(grid)));
            for (u32 z = 0; z < grid.resolutionZ; ++z)
            {
                for (u32 y = 0; y < grid.resolutionY; ++y)
                {
                    for (u32 x = 0; x < grid.resolutionX; ++x)
                    {
                        if (CsgIsSolid(grid, x, y, z))
                        {
                            proxies.push_back({CsgCellBounds(grid, x, y, z), 1u});
                        }
                    }
                }
            }
            return proxies;
        }

        for (u32 z = 0; z < grid.resolutionZ; ++z)
        {
            for (u32 y = 0; y < grid.resolutionY; ++y)
            {
                u32 x = 0;
                while (x < grid.resolutionX)
                {
                    while (x < grid.resolutionX && !CsgIsSolid(grid, x, y, z))
                    {
                        ++x;
                    }

                    if (x >= grid.resolutionX)
                    {
                        break;
                    }

                    const u32 startX = x;
                    while (x < grid.resolutionX && CsgIsSolid(grid, x, y, z))
                    {
                        ++x;
                    }

                    const u32 endXExclusive = x;
                    AABB3 runBounds = CsgCellBounds(grid, startX, y, z);
                    for (u32 mergeX = startX + 1u; mergeX < endXExclusive; ++mergeX)
                    {
                        runBounds = Union(runBounds, CsgCellBounds(grid, mergeX, y, z));
                    }

                    proxies.push_back({runBounds, endXExclusive - startX});
                }
            }
        }

        return proxies;
    }

    CsgBooleanResult BuildPrimitiveBoolean(
        const CsgPrimitiveDesc& left,
        const CsgPrimitiveDesc& right,
        CsgBooleanOperation operation,
        AABB3 bounds,
        u32 resolutionX,
        u32 resolutionY,
        u32 resolutionZ,
        CsgCollisionProxyMode proxyMode)
    {
        CsgBooleanResult result{};
        CsgVoxelGrid leftGrid = VoxelizePrimitive(left, bounds, resolutionX, resolutionY, resolutionZ);
        CsgVoxelGrid rightGrid = VoxelizePrimitive(right, bounds, resolutionX, resolutionY, resolutionZ);
        u64 changedVoxels = 0;
        result.grid = ApplyBooleanOperation(leftGrid, rightGrid, operation, &changedVoxels);
        result.collisionProxies = ExtractCollisionProxies(result.grid, proxyMode);
        result.stats.operation = operation;
        result.stats.leftMode = CsgVoxelizationMode::PrimitiveSdf;
        result.stats.rightMode = CsgVoxelizationMode::PrimitiveSdf;
        result.stats.proxyMode = proxyMode;
        result.stats.resolutionX = result.grid.resolutionX;
        result.stats.resolutionY = result.grid.resolutionY;
        result.stats.resolutionZ = result.grid.resolutionZ;
        result.stats.sourceSolidVoxels = CountSolidVoxels(leftGrid);
        result.stats.toolSolidVoxels = CountSolidVoxels(rightGrid);
        result.stats.resultSolidVoxels = CountSolidVoxels(result.grid);
        result.stats.changedVoxels = changedVoxels;
        result.stats.collisionProxyCount = static_cast<u32>(result.collisionProxies.size());
        result.stats.estimatedCollisionBytes = static_cast<u64>(result.collisionProxies.size() * sizeof(CsgCollisionProxy));
        result.stats.ok = result.stats.resultSolidVoxels > 0u && !result.collisionProxies.empty();
        if (proxyMode == CsgCollisionProxyMode::PerVoxelAABB)
        {
            AddWarning(result.stats, "per-voxel proxies are for debug only; use greedy proxies for physics broadphase");
        }
        result.summary = BuildSummary(result.stats);
        return result;
    }

    CsgBooleanResult BuildMeshPrimitiveBoolean(
        const CsgMeshAsset& leftMesh,
        const CsgPrimitiveDesc& right,
        CsgBooleanOperation operation,
        AABB3 bounds,
        u32 resolutionX,
        u32 resolutionY,
        u32 resolutionZ,
        CsgCollisionProxyMode proxyMode)
    {
        CsgBooleanResult result{};
        u64 triangleTests = 0;
        CsgVoxelGrid leftGrid = VoxelizeMeshOddEven(leftMesh, bounds, resolutionX, resolutionY, resolutionZ, &triangleTests);
        CsgVoxelGrid rightGrid = VoxelizePrimitive(right, bounds, resolutionX, resolutionY, resolutionZ);
        u64 changedVoxels = 0;
        result.grid = ApplyBooleanOperation(leftGrid, rightGrid, operation, &changedVoxels);
        result.collisionProxies = ExtractCollisionProxies(result.grid, proxyMode);
        result.stats.operation = operation;
        result.stats.leftMode = CsgVoxelizationMode::MeshOddEvenRaycast;
        result.stats.rightMode = CsgVoxelizationMode::PrimitiveSdf;
        result.stats.proxyMode = proxyMode;
        result.stats.resolutionX = result.grid.resolutionX;
        result.stats.resolutionY = result.grid.resolutionY;
        result.stats.resolutionZ = result.grid.resolutionZ;
        result.stats.sourceSolidVoxels = CountSolidVoxels(leftGrid);
        result.stats.toolSolidVoxels = CountSolidVoxels(rightGrid);
        result.stats.resultSolidVoxels = CountSolidVoxels(result.grid);
        result.stats.changedVoxels = changedVoxels;
        result.stats.triangleTests = triangleTests;
        result.stats.collisionProxyCount = static_cast<u32>(result.collisionProxies.size());
        result.stats.estimatedCollisionBytes = static_cast<u64>(result.collisionProxies.size() * sizeof(CsgCollisionProxy));
        result.stats.watertightInputRequired = true;
        result.stats.ok = result.stats.sourceSolidVoxels > 0u && result.stats.resultSolidVoxels > 0u && !result.collisionProxies.empty();
        if (!leftMesh.declaredWatertight)
        {
            AddWarning(result.stats, "odd-even mesh voxelization expects closed watertight input");
        }
        result.summary = BuildSummary(result.stats);
        return result;
    }

    CsgProbeResult BuildCsgProbe()
    {
        const AABB3 bounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {1.25f, 1.25f, 1.25f});
        const CsgPrimitiveDesc sourceBox = MakeCsgBox({0.0f, 0.0f, 0.0f}, {0.85f, 0.85f, 0.85f});
        const CsgPrimitiveDesc cutterSphere = MakeCsgSphere({0.45f, 0.25f, 0.0f}, 0.62f);
        const CsgBooleanResult primitiveDifference = BuildPrimitiveBoolean(
            sourceBox,
            cutterSphere,
            CsgBooleanOperation::Difference,
            bounds,
            32u,
            24u,
            32u);

        const CsgMeshAsset boxMesh = MakeCsgBoxMeshAsset("probe_box_mesh", {0.0f, 0.0f, 0.0f}, {0.85f, 0.85f, 0.85f});
        const CsgBooleanResult meshDifference = BuildMeshPrimitiveBoolean(
            boxMesh,
            MakeCsgCylinderY({0.0f, 0.0f, 0.0f}, 0.42f, 2.25f),
            CsgBooleanOperation::Difference,
            bounds,
            24u,
            24u,
            24u);

        CsgProbeResult probe{};
        probe.primitiveDifference = primitiveDifference;
        probe.meshDifference = meshDifference;
        probe.ok = primitiveDifference.stats.ok && meshDifference.stats.ok;
        std::ostringstream out;
        out << "CSG probe: " << (probe.ok ? "ok" : "failed")
            << " primitiveResult=" << primitiveDifference.stats.resultSolidVoxels
            << " meshResult=" << meshDifference.stats.resultSolidVoxels
            << " proxies=" << (primitiveDifference.stats.collisionProxyCount + meshDifference.stats.collisionProxyCount);
        probe.summary = out.str();
        return probe;
    }

    std::string BuildCsgProbeSummary()
    {
        return BuildCsgProbe().summary;
    }
}
