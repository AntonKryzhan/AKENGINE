#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/World/WorldCoordinates.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class WorldTopologyMode
    {
        PlanarTerrain,
        SphericalPlanet,
        ToroidalWrap
    };

    struct DVec3
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct GeodeticPosition
    {
        double latitudeRadians = 0.0;
        double longitudeRadians = 0.0;
        double altitudeMeters = 0.0;
    };

    struct PlanetSurfaceConfig
    {
        double radiusMeters = 6371000.0;
        double gravityMetersPerSecondSquared = 9.80665;
        double atmosphereHeightMeters = 100000.0;
        double maxTerrainAltitudeMeters = 12000.0;
        double minTerrainAltitudeMeters = -12000.0;
        bool inverseSquareGravity = true;
    };

    struct PlanarTerrainConfig
    {
        double chunkSizeMeters = 1024.0;
        double heightScaleMeters = 1.0;
        double maxTerrainHeightMeters = 12000.0;
        bool yUp = true;
    };

    struct ToroidalWorldConfig
    {
        double widthMeters = 1024.0;
        double depthMeters = 1024.0;
        bool preserveUnwrappedTile = true;
    };

    struct ToroidalPosition
    {
        double x = 0.0;
        double z = 0.0;
        i64 tileX = 0;
        i64 tileZ = 0;
    };

    struct SurfaceFrame
    {
        DVec3 positionMeters{};
        DVec3 up{};
        DVec3 east{};
        DVec3 north{};
        DVec3 gravity{};
        double gravityMagnitudeMetersPerSecondSquared = 0.0;
        double altitudeMeters = 0.0;
        bool finite = true;
    };

    struct WorldTopologyConfig
    {
        WorldTopologyMode mode = WorldTopologyMode::PlanarTerrain;
        PlanetSurfaceConfig planet{};
        PlanarTerrainConfig planar{};
        ToroidalWorldConfig toroidal{};
    };

    struct TopologyValidationReport
    {
        bool ok = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct WorldTopologyProbeResult
    {
        bool ok = false;
        std::string summary;
        GeodeticPosition earthEquator{};
        SurfaceFrame planetFrame{};
        ToroidalPosition wrapped{};
        DVec3 shortestToroidalDelta{};
        TopologyValidationReport validation{};
    };

    constexpr double EarthMeanRadiusMeters = 6371000.0;
    constexpr double EarthStandardGravityMetersPerSecondSquared = 9.80665;
    constexpr double Pi = 3.141592653589793238462643383279502884;

    const char* ToString(WorldTopologyMode mode);

    DVec3 MakeDVec3(double x, double y, double z);
    bool IsFinite(DVec3 value);
    double Dot(DVec3 a, DVec3 b);
    DVec3 Cross(DVec3 a, DVec3 b);
    DVec3 Add(DVec3 a, DVec3 b);
    DVec3 Subtract(DVec3 a, DVec3 b);
    DVec3 Multiply(DVec3 value, double scalar);
    double LengthSquared(DVec3 value);
    double Length(DVec3 value);
    DVec3 Normalize(DVec3 value, DVec3 fallback = {0.0, 1.0, 0.0});
    Vec3 ToFloatVec3(DVec3 value);

    double DegreesToRadians(double degrees);
    double RadiansToDegrees(double radians);
    double WrapLongitudeRadians(double longitudeRadians);
    double ClampLatitudeRadians(double latitudeRadians);

    DVec3 PlanetGeodeticToCartesian(GeodeticPosition geodetic, const PlanetSurfaceConfig& config = {});
    GeodeticPosition PlanetCartesianToGeodetic(DVec3 positionMeters, const PlanetSurfaceConfig& config = {});
    SurfaceFrame BuildPlanetSurfaceFrame(GeodeticPosition geodetic, const PlanetSurfaceConfig& config = {});
    SurfaceFrame BuildPlanetSurfaceFrame(DVec3 positionMeters, const PlanetSurfaceConfig& config = {});
    GeodeticPosition MoveOnPlanetSurface(GeodeticPosition start, double eastMeters, double northMeters, const PlanetSurfaceConfig& config = {});
    double PlanetGravityAtAltitude(double altitudeMeters, const PlanetSurfaceConfig& config = {});
    WorldPosition PlanetGeodeticToWorldPosition(GeodeticPosition geodetic, double cellSizeMeters = DefaultWorldCellSizeMeters, const PlanetSurfaceConfig& config = {});

    ToroidalPosition WrapToroidalPosition(double xMeters, double zMeters, const ToroidalWorldConfig& config = {});
    ToroidalPosition MoveToroidal(ToroidalPosition position, double deltaXMeters, double deltaZMeters, const ToroidalWorldConfig& config = {});
    DVec3 ShortestToroidalDelta(ToroidalPosition from, ToroidalPosition to, const ToroidalWorldConfig& config = {});
    WorldPosition ToroidalToWorldPosition(ToroidalPosition position, double yMeters = 0.0, double cellSizeMeters = DefaultWorldCellSizeMeters);

    WorldCell PlanarTerrainCellFromXZ(double xMeters, double zMeters, const PlanarTerrainConfig& config = {});
    DVec3 PlanarTerrainLocalInCell(double xMeters, double yMeters, double zMeters, const PlanarTerrainConfig& config = {});
    SurfaceFrame BuildPlanarTerrainFrame(double xMeters, double yMeters, double zMeters, const PlanarTerrainConfig& config = {});

    TopologyValidationReport ValidateWorldTopologyConfig(const WorldTopologyConfig& config);
    std::string ToDebugString(DVec3 value, int precision = 3);
    std::string ToDebugString(GeodeticPosition value, int precision = 4);
    std::string ToDebugString(const SurfaceFrame& frame, int precision = 3);
    std::string ToDebugString(const ToroidalPosition& position, int precision = 3);
    std::string ToDebugString(const TopologyValidationReport& report);
    std::string BuildWorldTopologyProbeSummary();
    WorldTopologyProbeResult BuildWorldTopologyProbe();
}
