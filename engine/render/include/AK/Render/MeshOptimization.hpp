#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Render/PrimitiveMesh.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class RenderGeometryOptimizationStrategy : u32
    {
        Conservative = 0,
        Aggressive = 1,
        StreamingWorld = 2,
        NaniteInspired = 3
    };

    enum class RenderVertexPackingMode : u32
    {
        FullFloat = 0,
        Packed32Byte = 1,
        PackedPosition16Planned = 2
    };

    enum class RenderLodPolicy : u32
    {
        Disabled = 0,
        TriangleBudget = 1,
        ScreenError = 2,
        QemPlanned = 3
    };

    enum class RenderSectionState : u32
    {
        Clean = 0,
        Dirty = 1,
        RebuildRequired = 2,
        Evictable = 3
    };

    struct PackedRenderVertex32
    {
        float px = 0.0f;
        float py = 0.0f;
        float pz = 0.0f;
        u32 normalOctSnorm16x2 = 0;
        u32 uvUnorm16x2 = 0;
        u32 colorRgba8 = 0xFFFFFFFFu;
        u32 materialAndFlags = 0;
        u32 padding = 0;
    };

    struct RenderMeshOptimizationConfig
    {
        RenderGeometryOptimizationStrategy strategy = RenderGeometryOptimizationStrategy::StreamingWorld;
        RenderVertexPackingMode vertexPacking = RenderVertexPackingMode::Packed32Byte;
        RenderLodPolicy lodPolicy = RenderLodPolicy::TriangleBudget;
        bool combineVertexBuffers = true;
        bool combineIndexBuffers = true;
        bool weldVertices = true;
        bool optimizeTriangleOrder = true;
        bool generateMeshlets = true;
        bool generateLodIndices = true;
        bool frontToBackSort = true;
        bool dirtySectionSkipping = true;
        bool preserveMaterialBoundaries = true;
        bool cameraRelative = true;
        u32 targetVertexCacheSize = 32;
        u32 maxMeshletVertices = 64;
        u32 maxMeshletTriangles = 96;
        u32 lodLevels = 4;
        float quantizationPositionStep = 0.0005f;
        float quantizationNormalStep = 0.001f;
        float nearPlaneMeters = 0.05f;
        float lod0ScreenHeight = 0.25f;
        float lodTransitionFactor = 0.45f;
    };

    struct RenderMeshLodRange
    {
        u32 level = 0;
        u32 firstIndex = 0;
        u32 indexCount = 0;
        u32 triangleCount = 0;
        float screenHeightThreshold = 1.0f;
        bool generated = false;
    };

    struct RenderMeshSection
    {
        std::string name;
        u32 meshIndex = 0;
        u32 objectIndex = 0;
        u32 firstVertex = 0;
        u32 vertexCount = 0;
        u32 firstIndex = 0;
        u32 indexCount = 0;
        u32 materialId = 0;
        RenderSectionState state = RenderSectionState::Dirty;
        AABB3 bounds{};
        bool visible = true;
        bool valid = false;
    };

    struct RenderMeshlet
    {
        u32 sectionIndex = 0;
        u32 firstIndex = 0;
        u32 indexCount = 0;
        u32 firstVertex = 0;
        u32 vertexCount = 0;
        u32 triangleCount = 0;
        AABB3 bounds{};
        Vec3 coneAxis{0.0f, 1.0f, 0.0f};
        float coneCutoff = -1.0f;
        bool valid = false;
    };

    struct RenderDrawCommand
    {
        std::string name;
        u32 sectionIndex = 0;
        u32 lodLevel = 0;
        u32 firstIndex = 0;
        u32 indexCount = 0;
        u32 vertexOffset = 0;
        u32 instanceCount = 1;
        u32 materialId = 0;
        float cameraDistance = 0.0f;
        bool indexed = true;
        bool valid = false;
    };

    struct OptimizedRenderGeometry
    {
        std::vector<PackedRenderVertex32> vertices;
        std::vector<u32> indices;
        std::vector<RenderMeshSection> sections;
        std::vector<RenderMeshLodRange> lodRanges;
        std::vector<RenderMeshlet> meshlets;
        std::vector<RenderDrawCommand> drawCommands;
        AABB3 bounds{};
        bool valid = false;
    };

    struct RenderMeshOptimizationStats
    {
        u32 sourceMeshes = 0;
        u32 sourceObjects = 0;
        u32 sourceVertices = 0;
        u32 sourceIndices = 0;
        u32 sourceTriangles = 0;
        u32 outputVertices = 0;
        u32 outputIndices = 0;
        u32 outputTriangles = 0;
        u32 sections = 0;
        u32 lodRanges = 0;
        u32 meshlets = 0;
        u32 drawCommands = 0;
        u32 weldedVertices = 0;
        u32 skippedCleanSections = 0;
        u32 validationWarnings = 0;
        u64 sourceVertexBytes = 0;
        u64 packedVertexBytes = 0;
        u64 indexBytes = 0;
        double vertexByteSavingsPercent = 0.0;
        float cacheScoreBefore = 0.0f;
        float cacheScoreAfter = 0.0f;
        bool combinedBuffers = false;
        bool packedVertices = false;
        bool lodGenerated = false;
        bool finite = true;
        bool valid = false;
        std::string summary;
    };

    struct MeshingSourceStudyReport
    {
        bool studied = true;
        bool adaptedToPolygonMeshes = true;
        bool vulkanFriendly = true;
        u32 usefulIdeas = 0;
        u32 rejectedUnityOrOpenGlLimits = 0;
        std::vector<std::string> usefulIdeaNames;
        std::vector<std::string> akEngineAdaptations;
        std::string summary;
    };

    struct RenderMeshOptimizationProbe
    {
        MeshingSourceStudyReport sourceStudy{};
        RenderMeshOptimizationConfig config{};
        PrimitiveTestScene inputScene{};
        OptimizedRenderGeometry geometry{};
        RenderMeshOptimizationStats stats{};
        bool ok = false;
        std::string summary;
    };

    RenderMeshOptimizationConfig MakeDefaultRenderMeshOptimizationConfig();
    MeshingSourceStudyReport BuildMeshingMainStudyReport();
    OptimizedRenderGeometry OptimizePrimitiveSceneForVulkan(const PrimitiveTestScene& scene, const RenderMeshOptimizationConfig& config = {});
    RenderMeshOptimizationStats AnalyzeOptimizedRenderGeometry(const PrimitiveTestScene& source, const OptimizedRenderGeometry& geometry, const RenderMeshOptimizationConfig& config = {});
    RenderMeshOptimizationProbe BuildRenderMeshOptimizationProbe();

    PackedRenderVertex32 PackRenderVertex32(const PrimitiveVertex& vertex, u32 materialId = 0, u32 flags = 0);
    PrimitiveVertex UnpackRenderVertex32(const PackedRenderVertex32& packed);

    const char* ToString(RenderGeometryOptimizationStrategy strategy);
    const char* ToString(RenderVertexPackingMode mode);
    const char* ToString(RenderLodPolicy policy);
    const char* ToString(RenderSectionState state);
    std::string ToDebugString(const MeshingSourceStudyReport& report);
    std::string ToDebugString(const RenderMeshOptimizationStats& stats);
    std::string ToDebugString(const RenderMeshOptimizationProbe& probe);
}
