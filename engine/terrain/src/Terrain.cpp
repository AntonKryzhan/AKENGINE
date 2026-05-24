#include <AK/Terrain/Terrain.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr double MinPositive = 1.0e-9;

        double ClampDouble(double value, double minValue, double maxValue)
        {
            return std::max(minValue, std::min(maxValue, value));
        }

        i64 FloorToI64(double value)
        {
            if (!IsFinite(value))
            {
                return 0;
            }
            return static_cast<i64>(std::floor(value));
        }

        u64 Mix64(u64 value)
        {
            value ^= value >> 30u;
            value *= 0xbf58476d1ce4e5b9ull;
            value ^= value >> 27u;
            value *= 0x94d049bb133111ebull;
            value ^= value >> 31u;
            return value;
        }

        u64 HashCell(i64 x, i64 z, u64 seed)
        {
            const u64 hx = static_cast<u64>(x) * 0x9e3779b185ebca87ull;
            const u64 hz = static_cast<u64>(z) * 0xc2b2ae3d27d4eb4full;
            return Mix64(seed ^ hx ^ hz);
        }

        double SmoothStep(double t)
        {
            const double clamped = ClampDouble(t, 0.0, 1.0);
            return clamped * clamped * (3.0 - 2.0 * clamped);
        }

        double LerpDouble(double a, double b, double t)
        {
            return a + (b - a) * t;
        }

        double HeightNoiseGrid(double x, double z, u64 seed)
        {
            const i64 x0 = FloorToI64(x);
            const i64 z0 = FloorToI64(z);
            const double tx = x - static_cast<double>(x0);
            const double tz = z - static_cast<double>(z0);
            const double sx = SmoothStep(tx);
            const double sz = SmoothStep(tz);

            const auto value = [seed](i64 ix, i64 iz) -> double
            {
                const u64 hash = HashCell(ix, iz, seed);
                const double unit = static_cast<double>(hash >> 11u) * (1.0 / 9007199254740992.0);
                return unit * 2.0 - 1.0;
            };

            const double a = LerpDouble(value(x0, z0), value(x0 + 1, z0), sx);
            const double b = LerpDouble(value(x0, z0 + 1), value(x0 + 1, z0 + 1), sx);
            return LerpDouble(a, b, sz);
        }

        AABB3 PatchBoundsFromCenter(DVec3 center, double halfExtent, double heightExtent)
        {
            const Vec3 c = ToFloatVec3(center);
            const Vec3 e{static_cast<float>(halfExtent), static_cast<float>(heightExtent), static_cast<float>(halfExtent)};
            return MakeAABB3FromCenterExtents(c, e);
        }

        double SlopeFromNormal(DVec3 normal)
        {
            const double upDot = ClampDouble(Dot(Normalize(normal, {0.0, 1.0, 0.0}), {0.0, 1.0, 0.0}), -1.0, 1.0);
            return RadiansToDegrees(std::acos(upDot));
        }
    }

    const char* ToString(TerrainMode mode)
    {
        switch (mode)
        {
        case TerrainMode::Heightfield:
            return "Heightfield";
        case TerrainMode::PlanetCubeSphere:
            return "PlanetCubeSphere";
        case TerrainMode::Voxel:
            return "Voxel";
        case TerrainMode::Mesh:
            return "Mesh";
        case TerrainMode::Procedural:
            return "Procedural";
        case TerrainMode::Toroidal:
            return "Toroidal";
        default:
            return "Unknown";
        }
    }

    const char* ToString(TerrainPatchState state)
    {
        switch (state)
        {
        case TerrainPatchState::Unloaded:
            return "Unloaded";
        case TerrainPatchState::Queued:
            return "Queued";
        case TerrainPatchState::Resident:
            return "Resident";
        case TerrainPatchState::Active:
            return "Active";
        default:
            return "Unknown";
        }
    }

    const char* ToString(CubeSphereFace face)
    {
        switch (face)
        {
        case CubeSphereFace::PositiveX:
            return "+X";
        case CubeSphereFace::NegativeX:
            return "-X";
        case CubeSphereFace::PositiveY:
            return "+Y";
        case CubeSphereFace::NegativeY:
            return "-Y";
        case CubeSphereFace::PositiveZ:
            return "+Z";
        case CubeSphereFace::NegativeZ:
            return "-Z";
        default:
            return "?";
        }
    }

    TerrainConfig MakeHeightfieldTerrainConfig(const HeightfieldTerrainDesc& heightfield)
    {
        TerrainConfig config{};
        config.mode = TerrainMode::Heightfield;
        config.heightfield = heightfield;
        return config;
    }

    TerrainConfig MakePlanetTerrainConfig(const PlanetTerrainDesc& planet)
    {
        TerrainConfig config{};
        config.mode = TerrainMode::PlanetCubeSphere;
        config.planet = planet;
        return config;
    }

    TerrainConfig MakeVoxelTerrainConfig(const VoxelTerrainDesc& voxel)
    {
        TerrainConfig config{};
        config.mode = TerrainMode::Voxel;
        config.voxel = voxel;
        return config;
    }

    TerrainConfig MakeMeshTerrainConfig(const MeshTerrainDesc& mesh)
    {
        TerrainConfig config{};
        config.mode = TerrainMode::Mesh;
        config.mesh = mesh;
        return config;
    }

    TerrainConfig MakeProceduralTerrainConfig(const ProceduralTerrainDesc& procedural)
    {
        TerrainConfig config{};
        config.mode = TerrainMode::Procedural;
        config.procedural = procedural;
        return config;
    }

    TerrainConfig MakeToroidalTerrainConfig(const ToroidalTerrainDesc& toroidal)
    {
        TerrainConfig config{};
        config.mode = TerrainMode::Toroidal;
        config.toroidal = toroidal;
        return config;
    }

    TerrainValidationReport ValidateTerrainConfig(const TerrainConfig& config)
    {
        TerrainValidationReport report{};
        const auto error = [&report](const std::string& message)
        {
            report.ok = false;
            report.errors.push_back(message);
        };
        const auto warning = [&report](const std::string& message)
        {
            report.warnings.push_back(message);
        };

        if (config.heightfield.chunkSizeMeters <= 0.0)
        {
            error("heightfield chunk size must be positive");
        }
        if (config.heightfield.sampleSpacingMeters <= 0.0)
        {
            error("heightfield sample spacing must be positive");
        }
        if (config.heightfield.sampleSpacingMeters > config.heightfield.chunkSizeMeters)
        {
            warning("heightfield sample spacing is larger than chunk size");
        }
        if (config.heightfield.minHeightMeters >= config.heightfield.maxHeightMeters)
        {
            error("heightfield min height must be lower than max height");
        }
        if (config.planet.planet.radiusMeters < 1000.0)
        {
            warning("planet radius is very small for planet terrain");
        }
        if (config.planet.maxLod == 0)
        {
            warning("planet terrain max LOD is zero");
        }
        if (config.voxel.voxelSizeMeters <= 0.0 || config.voxel.chunkSizeMeters <= 0.0)
        {
            error("voxel terrain chunk and voxel sizes must be positive");
        }
        if (config.procedural.octaves > 12)
        {
            warning("procedural terrain octaves above 12 can be expensive");
        }
        if (config.toroidal.toroidal.widthMeters <= 0.0 || config.toroidal.toroidal.depthMeters <= 0.0)
        {
            error("toroidal terrain dimensions must be positive");
        }
        if (!config.streamingEnabled && (config.mode == TerrainMode::PlanetCubeSphere || config.mode == TerrainMode::Voxel))
        {
            warning("large terrain modes should normally keep streaming enabled");
        }
        return report;
    }

    double DeterministicTerrainNoise2D(double x, double z, u64 seed)
    {
        if (!IsFinite(x) || !IsFinite(z))
        {
            return 0.0;
        }
        return ClampDouble(HeightNoiseGrid(x, z, seed), -1.0, 1.0);
    }

    double EvaluateProceduralHeight(double xMeters, double zMeters, const ProceduralTerrainDesc& desc)
    {
        if (!IsFinite(xMeters) || !IsFinite(zMeters) || desc.baseAmplitudeMeters == 0.0 || desc.baseFrequency == 0.0)
        {
            return 0.0;
        }

        double amplitude = desc.baseAmplitudeMeters;
        double frequency = desc.baseFrequency;
        double height = 0.0;
        double amplitudeSum = 0.0;
        const u32 octaves = std::max(1u, desc.octaves);
        for (u32 octave = 0; octave < octaves; ++octave)
        {
            height += DeterministicTerrainNoise2D(xMeters * frequency, zMeters * frequency, desc.seed + static_cast<u64>(octave) * 0x9e3779b97f4a7c15ull) * amplitude;
            amplitudeSum += amplitude;
            amplitude *= desc.persistence;
            frequency *= desc.lacunarity;
        }

        if (amplitudeSum <= MinPositive)
        {
            return 0.0;
        }
        return height;
    }

    TerrainSample SampleHeightfieldTerrain(double xMeters, double zMeters, const HeightfieldTerrainDesc& heightfield, const ProceduralTerrainDesc& procedural)
    {
        TerrainSample sample{};
        sample.finite = IsFinite(xMeters) && IsFinite(zMeters);
        if (!sample.finite)
        {
            return sample;
        }

        const double height = ClampDouble(EvaluateProceduralHeight(xMeters, zMeters, procedural), heightfield.minHeightMeters, heightfield.maxHeightMeters);
        constexpr double epsilon = 1.0;
        const double hx0 = ClampDouble(EvaluateProceduralHeight(xMeters - epsilon, zMeters, procedural), heightfield.minHeightMeters, heightfield.maxHeightMeters);
        const double hx1 = ClampDouble(EvaluateProceduralHeight(xMeters + epsilon, zMeters, procedural), heightfield.minHeightMeters, heightfield.maxHeightMeters);
        const double hz0 = ClampDouble(EvaluateProceduralHeight(xMeters, zMeters - epsilon, procedural), heightfield.minHeightMeters, heightfield.maxHeightMeters);
        const double hz1 = ClampDouble(EvaluateProceduralHeight(xMeters, zMeters + epsilon, procedural), heightfield.minHeightMeters, heightfield.maxHeightMeters);
        const DVec3 tangentX{2.0 * epsilon, hx1 - hx0, 0.0};
        const DVec3 tangentZ{0.0, hz1 - hz0, 2.0 * epsilon};
        sample.normal = Normalize(Cross(tangentZ, tangentX), {0.0, 1.0, 0.0});
        sample.heightMeters = height;
        sample.positionMeters = {xMeters, height, zMeters};
        sample.slopeDegrees = SlopeFromNormal(sample.normal);
        sample.hole = false;
        sample.surfaceMaterialWeight = ClampDouble(1.0 - sample.slopeDegrees / 90.0, 0.0, 1.0);
        return sample;
    }

    TerrainSample SamplePlanetTerrain(GeodeticPosition geodetic, const PlanetTerrainDesc& planet, const ProceduralTerrainDesc& procedural)
    {
        TerrainSample sample{};
        geodetic.latitudeRadians = ClampLatitudeRadians(geodetic.latitudeRadians);
        geodetic.longitudeRadians = WrapLongitudeRadians(geodetic.longitudeRadians);

        const double latitudeMeters = geodetic.latitudeRadians * planet.planet.radiusMeters;
        const double longitudeMeters = geodetic.longitudeRadians * planet.planet.radiusMeters * std::max(0.01, std::cos(geodetic.latitudeRadians));
        const double terrainHeight = ClampDouble(EvaluateProceduralHeight(longitudeMeters, latitudeMeters, procedural), planet.planet.minTerrainAltitudeMeters, planet.planet.maxTerrainAltitudeMeters);
        geodetic.altitudeMeters += terrainHeight;
        sample.positionMeters = PlanetGeodeticToCartesian(geodetic, planet.planet);
        sample.normal = Normalize(sample.positionMeters, {0.0, 1.0, 0.0});
        sample.heightMeters = terrainHeight;
        sample.slopeDegrees = 0.0;
        sample.surfaceMaterialWeight = 1.0;
        sample.finite = IsFinite(sample.positionMeters);
        return sample;
    }

    TerrainSample SampleToroidalTerrain(ToroidalPosition position, const ToroidalTerrainDesc& toroidal, const ProceduralTerrainDesc& procedural)
    {
        const ToroidalPosition wrapped = WrapToroidalPosition(position.x, position.z, toroidal.toroidal);
        TerrainSample sample = SampleHeightfieldTerrain(wrapped.x, wrapped.z, {}, procedural);
        sample.positionMeters.x = wrapped.x;
        sample.positionMeters.z = wrapped.z;
        return sample;
    }

    TerrainSample SampleTerrain(const TerrainConfig& config, DVec3 worldPositionMeters)
    {
        switch (config.mode)
        {
        case TerrainMode::Heightfield:
        case TerrainMode::Procedural:
        case TerrainMode::Mesh:
            return SampleHeightfieldTerrain(worldPositionMeters.x, worldPositionMeters.z, config.heightfield, config.procedural);
        case TerrainMode::PlanetCubeSphere:
            return SamplePlanetTerrain(PlanetCartesianToGeodetic(worldPositionMeters, config.planet.planet), config.planet, config.procedural);
        case TerrainMode::Toroidal:
            return SampleToroidalTerrain(WrapToroidalPosition(worldPositionMeters.x, worldPositionMeters.z, config.toroidal.toroidal), config.toroidal, config.procedural);
        case TerrainMode::Voxel:
        {
            TerrainSample sample = SampleHeightfieldTerrain(worldPositionMeters.x, worldPositionMeters.z, config.heightfield, config.procedural);
            sample.hole = config.voxel.supportsCaves && sample.heightMeters < 0.0;
            return sample;
        }
        default:
            return {};
        }
    }

    TerrainPatchId BuildHeightfieldPatchId(double xMeters, double zMeters, u32 lod, const HeightfieldTerrainDesc& heightfield)
    {
        const double size = std::max(1.0, heightfield.chunkSizeMeters * static_cast<double>(1ull << std::min(lod, 20u)));
        TerrainPatchId id{};
        id.mode = TerrainMode::Heightfield;
        id.x = FloorToI64(xMeters / size);
        id.z = FloorToI64(zMeters / size);
        id.lod = lod;
        id.face = CubeSphereFace::PositiveY;
        return id;
    }

    TerrainPatchId BuildToroidalPatchId(ToroidalPosition position, u32 lod, const ToroidalTerrainDesc& toroidal)
    {
        const ToroidalPosition wrapped = WrapToroidalPosition(position.x, position.z, toroidal.toroidal);
        const double size = std::max(1.0, toroidal.tileSizeMeters * static_cast<double>(1ull << std::min(lod, 20u)));
        TerrainPatchId id{};
        id.mode = TerrainMode::Toroidal;
        id.x = FloorToI64(wrapped.x / size);
        id.z = FloorToI64(wrapped.z / size);
        id.lod = lod;
        id.face = CubeSphereFace::PositiveY;
        return id;
    }

    TerrainPatchId BuildPlanetPatchId(GeodeticPosition geodetic, u32 lod, const PlanetTerrainDesc& planet)
    {
        geodetic.latitudeRadians = ClampLatitudeRadians(geodetic.latitudeRadians);
        geodetic.longitudeRadians = WrapLongitudeRadians(geodetic.longitudeRadians);
        const DVec3 direction = Normalize(PlanetGeodeticToCartesian({geodetic.latitudeRadians, geodetic.longitudeRadians, 0.0}, planet.planet), {0.0, 1.0, 0.0});
        const u32 clampedLod = std::min(lod, planet.maxLod);
        const i64 resolution = static_cast<i64>(1ull << std::min(clampedLod, 20u));
        const double u = (geodetic.longitudeRadians + Pi) / (2.0 * Pi);
        const double v = (geodetic.latitudeRadians + Pi * 0.5) / Pi;
        TerrainPatchId id{};
        id.mode = TerrainMode::PlanetCubeSphere;
        id.face = SelectCubeSphereFace(direction);
        id.x = std::clamp<i64>(FloorToI64(u * static_cast<double>(resolution)), 0, std::max<i64>(0, resolution - 1));
        id.y = std::clamp<i64>(FloorToI64(v * static_cast<double>(resolution)), 0, std::max<i64>(0, resolution - 1));
        id.lod = clampedLod;
        return id;
    }

    TerrainPatchDesc BuildTerrainPatchDesc(const TerrainPatchId& id, DVec3 cameraPositionMeters, const TerrainConfig& config, const TerrainLodPolicy& lodPolicy)
    {
        TerrainPatchDesc patch{};
        patch.id = id;
        patch.state = TerrainPatchState::Queued;
        patch.visible = true;
        patch.collisionResident = config.collisionEnabled;
        patch.navigationResident = config.navigationEnabled;

        const double lodScale = static_cast<double>(1ull << std::min(id.lod, 20u));
        switch (id.mode)
        {
        case TerrainMode::PlanetCubeSphere:
        {
            const double edge = std::max(config.planet.minPatchEdgeMeters, (config.planet.planet.radiusMeters * config.planet.rootTileAngularSizeRadians) / lodScale);
            patch.estimatedEdgeMeters = edge;
            patch.centerMeters = Multiply(CubeSphereFaceToNormal(id.face), config.planet.planet.radiusMeters);
            patch.localBounds = PatchBoundsFromCenter(patch.centerMeters, edge * 0.5, config.planet.planet.maxTerrainAltitudeMeters);
            break;
        }
        case TerrainMode::Toroidal:
        {
            const double edge = std::max(1.0, config.toroidal.tileSizeMeters * lodScale);
            patch.estimatedEdgeMeters = edge;
            patch.centerMeters = {static_cast<double>(id.x) * edge + edge * 0.5, 0.0, static_cast<double>(id.z) * edge + edge * 0.5};
            patch.localBounds = PatchBoundsFromCenter(patch.centerMeters, edge * 0.5, config.heightfield.maxHeightMeters);
            break;
        }
        default:
        {
            const double edge = std::max(1.0, config.heightfield.chunkSizeMeters * lodScale);
            patch.estimatedEdgeMeters = edge;
            patch.centerMeters = {static_cast<double>(id.x) * edge + edge * 0.5, 0.0, static_cast<double>(id.z) * edge + edge * 0.5};
            patch.localBounds = PatchBoundsFromCenter(patch.centerMeters, edge * 0.5, config.heightfield.maxHeightMeters);
            break;
        }
        }

        patch.distanceToCameraMeters = Length(Subtract(patch.centerMeters, cameraPositionMeters));
        if (patch.distanceToCameraMeters <= lodPolicy.nearPatchDistanceMeters)
        {
            patch.state = TerrainPatchState::Active;
        }
        else if (patch.distanceToCameraMeters <= lodPolicy.farPatchDistanceMeters)
        {
            patch.state = TerrainPatchState::Resident;
        }
        else
        {
            patch.state = TerrainPatchState::Unloaded;
            patch.visible = false;
            patch.collisionResident = false;
            patch.navigationResident = false;
        }
        return patch;
    }

    CubeSphereFace SelectCubeSphereFace(DVec3 direction)
    {
        const DVec3 n = Normalize(direction, {0.0, 1.0, 0.0});
        const double ax = std::abs(n.x);
        const double ay = std::abs(n.y);
        const double az = std::abs(n.z);
        if (ax >= ay && ax >= az)
        {
            return n.x >= 0.0 ? CubeSphereFace::PositiveX : CubeSphereFace::NegativeX;
        }
        if (ay >= ax && ay >= az)
        {
            return n.y >= 0.0 ? CubeSphereFace::PositiveY : CubeSphereFace::NegativeY;
        }
        return n.z >= 0.0 ? CubeSphereFace::PositiveZ : CubeSphereFace::NegativeZ;
    }

    DVec3 CubeSphereFaceToNormal(CubeSphereFace face)
    {
        switch (face)
        {
        case CubeSphereFace::PositiveX:
            return {1.0, 0.0, 0.0};
        case CubeSphereFace::NegativeX:
            return {-1.0, 0.0, 0.0};
        case CubeSphereFace::PositiveY:
            return {0.0, 1.0, 0.0};
        case CubeSphereFace::NegativeY:
            return {0.0, -1.0, 0.0};
        case CubeSphereFace::PositiveZ:
            return {0.0, 0.0, 1.0};
        case CubeSphereFace::NegativeZ:
            return {0.0, 0.0, -1.0};
        default:
            return {0.0, 1.0, 0.0};
        }
    }

    u32 SelectTerrainLod(double distanceMeters, double patchEdgeMeters, const TerrainLodPolicy& policy)
    {
        if (!IsFinite(distanceMeters) || distanceMeters <= policy.nearPatchDistanceMeters)
        {
            return policy.minLod;
        }
        const double normalized = std::max(1.0, distanceMeters / std::max(1.0, patchEdgeMeters));
        const double lod = std::log2(normalized);
        return std::clamp(static_cast<u32>(std::floor(lod)), policy.minLod, policy.maxLod);
    }

    std::string ToDebugString(const TerrainValidationReport& report)
    {
        std::ostringstream out;
        out << "terrain validation: " << (report.ok ? "ok" : "failed")
            << " warnings=" << report.warnings.size()
            << " errors=" << report.errors.size();
        if (!report.errors.empty())
        {
            out << " first_error=" << report.errors.front();
        }
        else if (!report.warnings.empty())
        {
            out << " first_warning=" << report.warnings.front();
        }
        return out.str();
    }

    std::string ToDebugString(const TerrainPatchId& id)
    {
        std::ostringstream out;
        out << ToString(id.mode)
            << " face=" << ToString(id.face)
            << " lod=" << id.lod
            << " x=" << id.x
            << " y=" << id.y
            << " z=" << id.z;
        return out.str();
    }

    std::string ToDebugString(const TerrainSample& sample, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "terrain sample height=" << sample.heightMeters
            << " slope=" << sample.slopeDegrees
            << " finite=" << (sample.finite ? "yes" : "no")
            << " hole=" << (sample.hole ? "yes" : "no")
            << " pos=" << ToDebugString(sample.positionMeters, precision);
        return out.str();
    }

    std::string ToDebugString(const TerrainPatchDesc& patch, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "terrain patch " << ToDebugString(patch.id)
            << " edge=" << patch.estimatedEdgeMeters
            << " distance=" << patch.distanceToCameraMeters
            << " state=" << ToString(patch.state)
            << " visible=" << (patch.visible ? "yes" : "no")
            << " collision=" << (patch.collisionResident ? "yes" : "no")
            << " nav=" << (patch.navigationResident ? "yes" : "no");
        return out.str();
    }

    std::string BuildTerrainProbeSummary()
    {
        return BuildTerrainProbe().summary;
    }

    TerrainProbeResult BuildTerrainProbe()
    {
        TerrainProbeResult probe{};

        TerrainConfig heightfield = MakeHeightfieldTerrainConfig();
        heightfield.procedural.seed = 1337;
        heightfield.procedural.baseAmplitudeMeters = 45.0;
        heightfield.procedural.baseFrequency = 0.003;
        probe.validation = ValidateTerrainConfig(heightfield);
        probe.heightfieldSample = SampleHeightfieldTerrain(128.0, -64.0, heightfield.heightfield, heightfield.procedural);
        probe.heightfieldPatch = BuildTerrainPatchDesc(BuildHeightfieldPatchId(128.0, -64.0, 1, heightfield.heightfield), {0.0, 32.0, 0.0}, heightfield, {});

        TerrainConfig planet = MakePlanetTerrainConfig();
        planet.procedural.seed = 7331;
        planet.procedural.baseAmplitudeMeters = 850.0;
        planet.procedural.baseFrequency = 0.00003;
        GeodeticPosition equator{};
        equator.latitudeRadians = DegreesToRadians(1.0);
        equator.longitudeRadians = DegreesToRadians(12.0);
        probe.planetSample = SamplePlanetTerrain(equator, planet.planet, planet.procedural);
        probe.planetPatch = BuildTerrainPatchDesc(BuildPlanetPatchId(equator, 6, planet.planet), {0.0, planet.planet.planet.radiusMeters + 1000.0, 0.0}, planet, {});

        TerrainConfig procedural = MakeProceduralTerrainConfig();
        procedural.procedural.baseAmplitudeMeters = 100.0;
        probe.proceduralSample = SampleTerrain(procedural, {512.0, 0.0, 384.0});

        TerrainConfig toroidal = MakeToroidalTerrainConfig();
        toroidal.procedural.seed = 424242;
        toroidal.procedural.baseAmplitudeMeters = 25.0;
        probe.toroidalSample = SampleToroidalTerrain({toroidal.toroidal.toroidal.widthMeters + 7.0, -9.0, 1, -1}, toroidal.toroidal, toroidal.procedural);

        probe.ok = probe.validation.ok
            && probe.heightfieldSample.finite
            && probe.planetSample.finite
            && probe.proceduralSample.finite
            && probe.toroidalSample.finite;

        std::ostringstream out;
        out << "Terrain probe: " << (probe.ok ? "ok" : "failed")
            << " modes=" << ToString(TerrainMode::Heightfield)
            << "/" << ToString(TerrainMode::PlanetCubeSphere)
            << "/" << ToString(TerrainMode::Voxel)
            << "/" << ToString(TerrainMode::Toroidal)
            << " hfHeight=" << std::fixed << std::setprecision(2) << probe.heightfieldSample.heightMeters
            << " planetHeight=" << probe.planetSample.heightMeters
            << " toroidalHeight=" << probe.toroidalSample.heightMeters
            << " planetPatch=" << ToDebugString(probe.planetPatch.id);
        probe.summary = out.str();
        return probe;
    }
}
