#include <AK/Remesh/Remesh.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr u32 MinimumMeshBudget = 6;
        constexpr u32 LatticeBits = 21;
        constexpr u64 LatticeMask = (1ull << LatticeBits) - 1ull;

        struct LatticeCorner
        {
            u32 x = 0;
            u32 y = 0;
            u32 z = 0;
        };

        struct EdgeKey
        {
            u64 a = 0;
            u64 b = 0;

            bool operator==(const EdgeKey& other) const
            {
                return a == other.a && b == other.b;
            }
        };

        struct EdgeKeyHash
        {
            std::size_t operator()(const EdgeKey& key) const
            {
                return static_cast<std::size_t>((key.a * 11400714819323198485ull) ^ (key.b + 0x9e3779b97f4a7c15ull + (key.a << 6u) + (key.a >> 2u)));
            }
        };

        u64 PackCorner(LatticeCorner corner)
        {
            const u64 x = static_cast<u64>(corner.x) & LatticeMask;
            const u64 y = static_cast<u64>(corner.y) & LatticeMask;
            const u64 z = static_cast<u64>(corner.z) & LatticeMask;
            return x | (y << LatticeBits) | (z << (LatticeBits * 2u));
        }

        EdgeKey MakeEdgeKey(LatticeCorner first, LatticeCorner second)
        {
            const u64 a = PackCorner(first);
            const u64 b = PackCorner(second);
            return a < b ? EdgeKey{a, b} : EdgeKey{b, a};
        }

        Vec3 LatticePosition(const CsgVoxelGrid& grid, LatticeCorner corner)
        {
            const Vec3 cell = CsgCellSize(grid);
            return {
                grid.bounds.min.x + static_cast<float>(corner.x) * cell.x,
                grid.bounds.min.y + static_cast<float>(corner.y) * cell.y,
                grid.bounds.min.z + static_cast<float>(corner.z) * cell.z
            };
        }

        bool IsSolidOrOutOfRange(const CsgVoxelGrid& grid, i32 x, i32 y, i32 z)
        {
            if (x < 0 || y < 0 || z < 0)
            {
                return false;
            }
            if (x >= static_cast<i32>(grid.resolutionX) || y >= static_cast<i32>(grid.resolutionY) || z >= static_cast<i32>(grid.resolutionZ))
            {
                return false;
            }
            return CsgIsSolid(grid, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z));
        }

        void PushWarning(RemeshStats& stats, std::string warning)
        {
            if (!warning.empty())
            {
                stats.warnings.push_back(std::move(warning));
            }
        }

        void AddEdge(std::unordered_map<EdgeKey, u32, EdgeKeyHash>& edgeUseCounts, LatticeCorner a, LatticeCorner b)
        {
            ++edgeUseCounts[MakeEdgeKey(a, b)];
        }

        bool AddQuad(
            const CsgVoxelGrid& grid,
            const std::array<LatticeCorner, 4>& corners,
            Vec3 normal,
            GeneratedSurfaceMesh& mesh,
            RemeshStats& stats,
            std::unordered_map<EdgeKey, u32, EdgeKeyHash>& edgeUseCounts,
            const RemeshSettings& settings)
        {
            if (mesh.vertices.size() + 4u > settings.maxVertices || mesh.triangles.size() + 2u > settings.maxTriangles)
            {
                stats.clippedByBudget = true;
                return false;
            }

            const u32 base = static_cast<u32>(mesh.vertices.size());
            const std::array<Vec2, 4> uvs{{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};

            for (std::size_t index = 0; index < corners.size(); ++index)
            {
                const Vec3 position = LatticePosition(grid, corners[index]);
                mesh.vertices.push_back({position, normal, uvs[index]});
                mesh.bounds = IsValid(mesh.bounds) ? Expand(mesh.bounds, position) : MakeAABB3(position, position);
            }

            mesh.triangles.push_back({base + 0u, base + 1u, base + 2u});
            mesh.triangles.push_back({base + 0u, base + 2u, base + 3u});

            AddEdge(edgeUseCounts, corners[0], corners[1]);
            AddEdge(edgeUseCounts, corners[1], corners[2]);
            AddEdge(edgeUseCounts, corners[2], corners[3]);
            AddEdge(edgeUseCounts, corners[3], corners[0]);

            ++stats.exposedFaces;
            return true;
        }

        void RefreshSummary(RemeshResult& result)
        {
            std::ostringstream out;
            out << (result.ok ? "ok" : "failed")
                << " algorithm=" << ToString(result.stats.actualAlgorithm)
                << " solids=" << result.stats.solidVoxels
                << " faces=" << result.stats.exposedFaces
                << " vertices=" << result.stats.vertices
                << " triangles=" << result.stats.triangles
                << " closed=" << (result.stats.closedSurface ? "yes" : "no")
                << " clipped=" << (result.stats.clippedByBudget ? "yes" : "no")
                << " warnings=" << result.stats.warnings.size();
            result.summary = out.str();
        }
    }

    const char* ToString(RemeshAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case RemeshAlgorithm::VoxelFaceSurface:
                return "VoxelFaceSurface";
            case RemeshAlgorithm::MarchingCubesPlanned:
                return "MarchingCubesPlanned";
            case RemeshAlgorithm::DualContouringPlanned:
                return "DualContouringPlanned";
            default:
                return "Unknown";
        }
    }

    const char* ToString(RemeshNormalMode mode)
    {
        switch (mode)
        {
            case RemeshNormalMode::FaceNormals:
                return "FaceNormals";
            case RemeshNormalMode::SmoothNormalsPlanned:
                return "SmoothNormalsPlanned";
            default:
                return "Unknown";
        }
    }

    RemeshSettings SanitizeRemeshSettings(RemeshSettings settings)
    {
        settings.maxVertices = std::max(settings.maxVertices, MinimumMeshBudget);
        settings.maxTriangles = std::max(settings.maxTriangles, MinimumMeshBudget);
        return settings;
    }

    u64 EstimateSurfaceMeshBytes(u64 vertexCount, u64 triangleCount)
    {
        return vertexCount * sizeof(RemeshVertex) + triangleCount * sizeof(RemeshTriangle);
    }

    RemeshResult ExtractVoxelSurfaceMesh(const CsgVoxelGrid& grid, RemeshSettings inputSettings)
    {
        RemeshSettings settings = SanitizeRemeshSettings(inputSettings);

        RemeshResult result{};
        result.stats.requestedAlgorithm = settings.algorithm;
        result.stats.actualAlgorithm = settings.algorithm;
        result.stats.normalMode = settings.normalMode;
        result.stats.resolutionX = grid.resolutionX;
        result.stats.resolutionY = grid.resolutionY;
        result.stats.resolutionZ = grid.resolutionZ;
        result.stats.solidVoxels = CountSolidVoxels(grid);

        if (settings.algorithm != RemeshAlgorithm::VoxelFaceSurface)
        {
            PushWarning(result.stats, "requested remesh algorithm is planned; using voxel face surface foundation path");
            result.stats.actualAlgorithm = RemeshAlgorithm::VoxelFaceSurface;
            settings.algorithm = RemeshAlgorithm::VoxelFaceSurface;
        }

        if (settings.normalMode != RemeshNormalMode::FaceNormals)
        {
            PushWarning(result.stats, "smooth remesh normals are planned; using face normals for deterministic voxel surface output");
            settings.normalMode = RemeshNormalMode::FaceNormals;
            result.stats.normalMode = RemeshNormalMode::FaceNormals;
        }

        if (!IsValid(grid.bounds) || grid.solid.empty() || result.stats.solidVoxels == 0)
        {
            PushWarning(result.stats, "voxel surface extraction skipped: empty or invalid grid");
            RefreshSummary(result);
            return result;
        }

        result.mesh.vertices.reserve(static_cast<std::size_t>(std::min<u64>(settings.maxVertices, result.stats.solidVoxels * 8u)));
        result.mesh.triangles.reserve(static_cast<std::size_t>(std::min<u64>(settings.maxTriangles, result.stats.solidVoxels * 12u)));

        std::unordered_map<EdgeKey, u32, EdgeKeyHash> edgeUseCounts;
        edgeUseCounts.reserve(static_cast<std::size_t>(std::min<u64>(settings.maxTriangles * 3ull, result.stats.solidVoxels * 24ull)));

        bool budgetExhausted = false;
        for (u32 z = 0; z < grid.resolutionZ && !budgetExhausted; ++z)
        {
            for (u32 y = 0; y < grid.resolutionY && !budgetExhausted; ++y)
            {
                for (u32 x = 0; x < grid.resolutionX && !budgetExhausted; ++x)
                {
                    if (!CsgIsSolid(grid, x, y, z))
                    {
                        continue;
                    }

                    const i32 ix = static_cast<i32>(x);
                    const i32 iy = static_cast<i32>(y);
                    const i32 iz = static_cast<i32>(z);

                    if (!IsSolidOrOutOfRange(grid, ix + 1, iy, iz))
                    {
                        const std::array<LatticeCorner, 4> corners{{{x + 1u, y, z}, {x + 1u, y + 1u, z}, {x + 1u, y + 1u, z + 1u}, {x + 1u, y, z + 1u}}};
                        budgetExhausted = !AddQuad(grid, corners, {1.0f, 0.0f, 0.0f}, result.mesh, result.stats, edgeUseCounts, settings);
                    }
                    else
                    {
                        ++result.stats.interiorFacesSkipped;
                    }

                    if (!budgetExhausted && !IsSolidOrOutOfRange(grid, ix - 1, iy, iz))
                    {
                        const std::array<LatticeCorner, 4> corners{{{x, y, z + 1u}, {x, y + 1u, z + 1u}, {x, y + 1u, z}, {x, y, z}}};
                        budgetExhausted = !AddQuad(grid, corners, {-1.0f, 0.0f, 0.0f}, result.mesh, result.stats, edgeUseCounts, settings);
                    }
                    else if (!budgetExhausted)
                    {
                        ++result.stats.interiorFacesSkipped;
                    }

                    if (!budgetExhausted && !IsSolidOrOutOfRange(grid, ix, iy + 1, iz))
                    {
                        const std::array<LatticeCorner, 4> corners{{{x, y + 1u, z + 1u}, {x + 1u, y + 1u, z + 1u}, {x + 1u, y + 1u, z}, {x, y + 1u, z}}};
                        budgetExhausted = !AddQuad(grid, corners, {0.0f, 1.0f, 0.0f}, result.mesh, result.stats, edgeUseCounts, settings);
                    }
                    else if (!budgetExhausted)
                    {
                        ++result.stats.interiorFacesSkipped;
                    }

                    if (!budgetExhausted && !IsSolidOrOutOfRange(grid, ix, iy - 1, iz))
                    {
                        const std::array<LatticeCorner, 4> corners{{{x, y, z}, {x + 1u, y, z}, {x + 1u, y, z + 1u}, {x, y, z + 1u}}};
                        budgetExhausted = !AddQuad(grid, corners, {0.0f, -1.0f, 0.0f}, result.mesh, result.stats, edgeUseCounts, settings);
                    }
                    else if (!budgetExhausted)
                    {
                        ++result.stats.interiorFacesSkipped;
                    }

                    if (!budgetExhausted && !IsSolidOrOutOfRange(grid, ix, iy, iz + 1))
                    {
                        const std::array<LatticeCorner, 4> corners{{{x, y, z + 1u}, {x + 1u, y, z + 1u}, {x + 1u, y + 1u, z + 1u}, {x, y + 1u, z + 1u}}};
                        budgetExhausted = !AddQuad(grid, corners, {0.0f, 0.0f, 1.0f}, result.mesh, result.stats, edgeUseCounts, settings);
                    }
                    else if (!budgetExhausted)
                    {
                        ++result.stats.interiorFacesSkipped;
                    }

                    if (!budgetExhausted && !IsSolidOrOutOfRange(grid, ix, iy, iz - 1))
                    {
                        const std::array<LatticeCorner, 4> corners{{{x + 1u, y, z}, {x, y, z}, {x, y + 1u, z}, {x + 1u, y + 1u, z}}};
                        budgetExhausted = !AddQuad(grid, corners, {0.0f, 0.0f, -1.0f}, result.mesh, result.stats, edgeUseCounts, settings);
                    }
                    else if (!budgetExhausted)
                    {
                        ++result.stats.interiorFacesSkipped;
                    }
                }
            }
        }

        result.stats.vertices = result.mesh.vertices.size();
        result.stats.triangles = result.mesh.triangles.size();
        result.stats.estimatedBytes = EstimateSurfaceMeshBytes(result.stats.vertices, result.stats.triangles);

        if (settings.validateClosedSurface)
        {
            for (const auto& item : edgeUseCounts)
            {
                if (item.second != 2u)
                {
                    ++result.stats.nonManifoldEdges;
                }
            }
            result.stats.closedSurface = result.stats.nonManifoldEdges == 0u && result.stats.exposedFaces > 0u;
            if (!result.stats.closedSurface)
            {
                PushWarning(result.stats, "voxel surface has boundary/non-manifold edges; keep physics proxies authoritative until repair/remesh stage");
            }
        }
        else
        {
            result.stats.closedSurface = result.stats.exposedFaces > 0u;
        }

        if (result.stats.clippedByBudget)
        {
            PushWarning(result.stats, "voxel surface extraction clipped by mesh budget");
        }

        result.ok = result.stats.exposedFaces > 0u
            && result.stats.vertices > 0u
            && result.stats.triangles > 0u
            && !result.stats.clippedByBudget
            && IsValid(result.mesh.bounds);
        result.stats.ok = result.ok;
        RefreshSummary(result);
        return result;
    }

    std::string ToDebugString(const RemeshSettings& settings)
    {
        std::ostringstream out;
        out << "remesh settings algorithm=" << ToString(settings.algorithm)
            << " normals=" << ToString(settings.normalMode)
            << " max_vertices=" << settings.maxVertices
            << " max_triangles=" << settings.maxTriangles
            << " boundary_only=" << (settings.boundaryOnly ? "yes" : "no")
            << " validate_closed=" << (settings.validateClosedSurface ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const RemeshStats& stats)
    {
        std::ostringstream out;
        out << "remesh stats algorithm=" << ToString(stats.actualAlgorithm)
            << " requested=" << ToString(stats.requestedAlgorithm)
            << " normals=" << ToString(stats.normalMode)
            << " res=" << stats.resolutionX << "x" << stats.resolutionY << "x" << stats.resolutionZ
            << " solids=" << stats.solidVoxels
            << " exposed_faces=" << stats.exposedFaces
            << " interior_faces_skipped=" << stats.interiorFacesSkipped
            << " vertices=" << stats.vertices
            << " triangles=" << stats.triangles
            << " bytes~" << stats.estimatedBytes
            << " closed=" << (stats.closedSurface ? "yes" : "no")
            << " non_manifold_edges=" << stats.nonManifoldEdges
            << " clipped=" << (stats.clippedByBudget ? "yes" : "no")
            << " warnings=" << stats.warnings.size()
            << " ok=" << (stats.ok ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const GeneratedSurfaceMesh& mesh)
    {
        std::ostringstream out;
        out << "surface mesh vertices=" << mesh.vertices.size()
            << " triangles=" << mesh.triangles.size();
        if (IsValid(mesh.bounds))
        {
            out << " bounds_min=" << AK::ToDebugString(mesh.bounds.min, 3)
                << " bounds_max=" << AK::ToDebugString(mesh.bounds.max, 3)
                << " size=" << AK::ToDebugString(Size(mesh.bounds), 3);
        }
        else
        {
            out << " bounds=invalid";
        }
        return out.str();
    }

    RemeshProbeResult BuildRemeshProbe()
    {
        const AABB3 bounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {1.1f, 1.1f, 1.1f});
        const CsgPrimitiveDesc source = MakeCsgBox({0.0f, 0.0f, 0.0f}, {0.95f, 0.95f, 0.95f});
        const CsgPrimitiveDesc cutter = MakeCsgSphere({0.45f, 0.12f, 0.0f}, 0.58f);
        CsgBooleanResult csg = BuildPrimitiveBoolean(
            source,
            cutter,
            CsgBooleanOperation::Difference,
            bounds,
            18u,
            18u,
            18u,
            CsgCollisionProxyMode::GreedyXAxisAABB);

        RemeshSettings settings{};
        settings.algorithm = RemeshAlgorithm::VoxelFaceSurface;
        settings.normalMode = RemeshNormalMode::FaceNormals;
        settings.maxVertices = 250'000;
        settings.maxTriangles = 500'000;
        settings.validateClosedSurface = true;

        RemeshResult remesh = ExtractVoxelSurfaceMesh(csg.grid, settings);

        RemeshProbeResult probe{};
        probe.csg = std::move(csg);
        probe.remesh = std::move(remesh);
        probe.ok = probe.csg.stats.ok
            && probe.remesh.ok
            && probe.remesh.stats.solidVoxels == probe.csg.stats.resultSolidVoxels
            && probe.remesh.stats.exposedFaces > 0u
            && probe.remesh.stats.vertices == probe.remesh.stats.exposedFaces * 4u
            && probe.remesh.stats.triangles == probe.remesh.stats.exposedFaces * 2u
            && probe.remesh.stats.closedSurface
            && IsValid(probe.remesh.mesh.bounds);

        std::ostringstream out;
        out << "Remesh probe: " << (probe.ok ? "ok" : "failed")
            << " csg_solids=" << probe.csg.stats.resultSolidVoxels
            << " faces=" << probe.remesh.stats.exposedFaces
            << " vertices=" << probe.remesh.stats.vertices
            << " triangles=" << probe.remesh.stats.triangles
            << " closed=" << (probe.remesh.stats.closedSurface ? "yes" : "no")
            << " bytes~" << probe.remesh.stats.estimatedBytes;
        probe.summary = out.str();
        return probe;
    }

    std::string BuildRemeshProbeSummary()
    {
        return BuildRemeshProbe().summary;
    }
}
