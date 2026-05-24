#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/WorldTopology/WorldTopology.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class TerrainMode : u32
    {
        Heightfield,
        PlanetCubeSphere,
        Voxel,
        Mesh,
        Procedural,
        Toroidal
    };

    enum class TerrainPatchState : u32
    {
        Unloaded,
        Queued,
        Resident,
        Active
    };

    enum class CubeSphereFace : u32
    {
        PositiveX,
        NegativeX,
        PositiveY,
        NegativeY,
        PositiveZ,
        NegativeZ
    };

    struct HeightfieldTerrainDesc
    {
        double chunkSizeMeters = 512.0;
        double sampleSpacingMeters = 2.0;
        double minHeightMeters = -1024.0;
        double maxHeightMeters = 8192.0;
        double collisionThicknessMeters = 64.0;
        bool supportsHoles = true;
        bool supportsSplatMaps = true;
    };

    struct PlanetTerrainDesc
    {
        PlanetSurfaceConfig planet{};
        u32 rootFaceCount = 6;
        u32 maxLod = 20;
        double rootTileAngularSizeRadians = Pi / 2.0;
        double minPatchEdgeMeters = 8.0;
        bool cubeSphere = true;
        bool supportsSpaceToGroundTransition = true;
    };

    struct VoxelTerrainDesc
    {
        double chunkSizeMeters = 32.0;
        double voxelSizeMeters = 0.25;
        bool editable = true;
        bool supportsCaves = true;
        bool supportsDestruction = true;
    };

    struct MeshTerrainDesc
    {
        bool authoredMesh = true;
        bool hasCollisionMesh = true;
        bool supportsStreamingSections = true;
    };

    struct ProceduralTerrainDesc
    {
        u64 seed = 1469598103934665603ull;
        double baseAmplitudeMeters = 120.0;
        double baseFrequency = 0.0007;
        u32 octaves = 5;
        double lacunarity = 2.0;
        double persistence = 0.5;
        bool deterministic = true;
    };

    struct ToroidalTerrainDesc
    {
        ToroidalWorldConfig toroidal{};
        double tileSizeMeters = 64.0;
        bool wrapHeightSamples = true;
        bool useShortestDeltaForQueries = true;
    };

    struct TerrainConfig
    {
        TerrainMode mode = TerrainMode::Heightfield;
        HeightfieldTerrainDesc heightfield{};
        PlanetTerrainDesc planet{};
        VoxelTerrainDesc voxel{};
        MeshTerrainDesc mesh{};
        ProceduralTerrainDesc procedural{};
        ToroidalTerrainDesc toroidal{};
        bool collisionEnabled = true;
        bool navigationEnabled = true;
        bool streamingEnabled = true;
        bool editorVisible = true;
    };

    struct TerrainPatchId
    {
        TerrainMode mode = TerrainMode::Heightfield;
        i64 x = 0;
        i64 y = 0;
        i64 z = 0;
        u32 lod = 0;
        CubeSphereFace face = CubeSphereFace::PositiveY;
    };

    struct TerrainSample
    {
        DVec3 positionMeters{};
        DVec3 normal{0.0, 1.0, 0.0};
        double heightMeters = 0.0;
        double slopeDegrees = 0.0;
        double surfaceMaterialWeight = 1.0;
        bool hole = false;
        bool finite = true;
    };

    struct TerrainPatchDesc
    {
        TerrainPatchId id{};
        DVec3 centerMeters{};
        AABB3 localBounds{};
        double estimatedEdgeMeters = 0.0;
        double distanceToCameraMeters = 0.0;
        TerrainPatchState state = TerrainPatchState::Unloaded;
        bool visible = true;
        bool collisionResident = false;
        bool navigationResident = false;
    };

    struct TerrainLodPolicy
    {
        double targetScreenErrorPixels = 2.0;
        double nearPatchDistanceMeters = 64.0;
        double farPatchDistanceMeters = 1000000.0;
        u32 minLod = 0;
        u32 maxLod = 12;
    };

    struct TerrainValidationReport
    {
        bool ok = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct TerrainProbeResult
    {
        bool ok = false;
        std::string summary;
        TerrainValidationReport validation{};
        TerrainSample heightfieldSample{};
        TerrainSample planetSample{};
        TerrainSample proceduralSample{};
        TerrainSample toroidalSample{};
        TerrainPatchDesc heightfieldPatch{};
        TerrainPatchDesc planetPatch{};
    };

    const char* ToString(TerrainMode mode);
    const char* ToString(TerrainPatchState state);
    const char* ToString(CubeSphereFace face);

    TerrainConfig MakeHeightfieldTerrainConfig(const HeightfieldTerrainDesc& heightfield = {});
    TerrainConfig MakePlanetTerrainConfig(const PlanetTerrainDesc& planet = {});
    TerrainConfig MakeVoxelTerrainConfig(const VoxelTerrainDesc& voxel = {});
    TerrainConfig MakeMeshTerrainConfig(const MeshTerrainDesc& mesh = {});
    TerrainConfig MakeProceduralTerrainConfig(const ProceduralTerrainDesc& procedural = {});
    TerrainConfig MakeToroidalTerrainConfig(const ToroidalTerrainDesc& toroidal = {});

    TerrainValidationReport ValidateTerrainConfig(const TerrainConfig& config);

    double DeterministicTerrainNoise2D(double x, double z, u64 seed);
    double EvaluateProceduralHeight(double xMeters, double zMeters, const ProceduralTerrainDesc& desc = {});
    TerrainSample SampleHeightfieldTerrain(double xMeters, double zMeters, const HeightfieldTerrainDesc& heightfield = {}, const ProceduralTerrainDesc& procedural = {});
    TerrainSample SamplePlanetTerrain(GeodeticPosition geodetic, const PlanetTerrainDesc& planet = {}, const ProceduralTerrainDesc& procedural = {});
    TerrainSample SampleToroidalTerrain(ToroidalPosition position, const ToroidalTerrainDesc& toroidal = {}, const ProceduralTerrainDesc& procedural = {});
    TerrainSample SampleTerrain(const TerrainConfig& config, DVec3 worldPositionMeters);

    TerrainPatchId BuildHeightfieldPatchId(double xMeters, double zMeters, u32 lod, const HeightfieldTerrainDesc& heightfield = {});
    TerrainPatchId BuildToroidalPatchId(ToroidalPosition position, u32 lod, const ToroidalTerrainDesc& toroidal = {});
    TerrainPatchId BuildPlanetPatchId(GeodeticPosition geodetic, u32 lod, const PlanetTerrainDesc& planet = {});
    TerrainPatchDesc BuildTerrainPatchDesc(const TerrainPatchId& id, DVec3 cameraPositionMeters, const TerrainConfig& config = {}, const TerrainLodPolicy& lodPolicy = {});

    CubeSphereFace SelectCubeSphereFace(DVec3 direction);
    DVec3 CubeSphereFaceToNormal(CubeSphereFace face);
    u32 SelectTerrainLod(double distanceMeters, double patchEdgeMeters, const TerrainLodPolicy& policy = {});

    std::string ToDebugString(const TerrainValidationReport& report);
    std::string ToDebugString(const TerrainPatchId& id);
    std::string ToDebugString(const TerrainSample& sample, int precision = 3);
    std::string ToDebugString(const TerrainPatchDesc& patch, int precision = 3);
    std::string BuildTerrainProbeSummary();
    TerrainProbeResult BuildTerrainProbe();
}
