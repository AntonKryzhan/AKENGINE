#pragma once

#include <AK/Core/Types.hpp>
#include <AK/CSG/Boolean.hpp>
#include <AK/Math/Geometry.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class RemeshAlgorithm : u32
    {
        VoxelFaceSurface = 0,
        MarchingCubesPlanned = 1,
        DualContouringPlanned = 2
    };

    enum class RemeshNormalMode : u32
    {
        FaceNormals = 0,
        SmoothNormalsPlanned = 1
    };

    struct RemeshSettings
    {
        RemeshAlgorithm algorithm = RemeshAlgorithm::VoxelFaceSurface;
        RemeshNormalMode normalMode = RemeshNormalMode::FaceNormals;
        u32 maxVertices = 1'000'000;
        u32 maxTriangles = 2'000'000;
        bool boundaryOnly = true;
        bool validateClosedSurface = true;
    };

    struct RemeshVertex
    {
        Vec3 position{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec2 uv{};
    };

    struct RemeshTriangle
    {
        u32 a = 0;
        u32 b = 0;
        u32 c = 0;
    };

    struct GeneratedSurfaceMesh
    {
        std::vector<RemeshVertex> vertices;
        std::vector<RemeshTriangle> triangles;
        AABB3 bounds = MakeEmptyAABB3();
    };

    struct RemeshStats
    {
        RemeshAlgorithm requestedAlgorithm = RemeshAlgorithm::VoxelFaceSurface;
        RemeshAlgorithm actualAlgorithm = RemeshAlgorithm::VoxelFaceSurface;
        RemeshNormalMode normalMode = RemeshNormalMode::FaceNormals;
        u32 resolutionX = 0;
        u32 resolutionY = 0;
        u32 resolutionZ = 0;
        u64 solidVoxels = 0;
        u64 exposedFaces = 0;
        u64 interiorFacesSkipped = 0;
        u64 vertices = 0;
        u64 triangles = 0;
        u64 estimatedBytes = 0;
        u64 nonManifoldEdges = 0;
        bool closedSurface = false;
        bool clippedByBudget = false;
        bool ok = false;
        std::vector<std::string> warnings;
    };

    struct RemeshResult
    {
        bool ok = false;
        GeneratedSurfaceMesh mesh{};
        RemeshStats stats{};
        std::string summary;
    };

    struct RemeshProbeResult
    {
        bool ok = false;
        CsgBooleanResult csg{};
        RemeshResult remesh{};
        std::string summary;
    };

    const char* ToString(RemeshAlgorithm algorithm);
    const char* ToString(RemeshNormalMode mode);

    RemeshSettings SanitizeRemeshSettings(RemeshSettings settings);
    u64 EstimateSurfaceMeshBytes(u64 vertexCount, u64 triangleCount);
    RemeshResult ExtractVoxelSurfaceMesh(const CsgVoxelGrid& grid, RemeshSettings settings = {});

    std::string ToDebugString(const RemeshSettings& settings);
    std::string ToDebugString(const RemeshStats& stats);
    std::string ToDebugString(const GeneratedSurfaceMesh& mesh);

    RemeshProbeResult BuildRemeshProbe();
    std::string BuildRemeshProbeSummary();
}
