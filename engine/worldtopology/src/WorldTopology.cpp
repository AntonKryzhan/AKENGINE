#include <AK/WorldTopology/WorldTopology.hpp>

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
        double PositiveModulo(double value, double period)
        {
            if (period <= 0.0 || !IsFinite(period) || !IsFinite(value))
            {
                return 0.0;
            }

            double wrapped = std::fmod(value, period);
            if (wrapped < 0.0)
            {
                wrapped += period;
            }
            return wrapped;
        }

        i64 FloorDivToTile(double value, double period)
        {
            if (period <= 0.0 || !IsFinite(period) || !IsFinite(value))
            {
                return 0;
            }
            return static_cast<i64>(std::floor(value / period));
        }

        double ShortestWrappedAxisDelta(double from, double to, double period)
        {
            if (period <= 0.0 || !IsFinite(period))
            {
                return to - from;
            }

            double delta = to - from;
            const double half = period * 0.5;
            if (delta > half)
            {
                delta -= period;
            }
            else if (delta < -half)
            {
                delta += period;
            }
            return delta;
        }
    }

    const char* ToString(WorldTopologyMode mode)
    {
        switch (mode)
        {
        case WorldTopologyMode::PlanarTerrain:
            return "PlanarTerrain";
        case WorldTopologyMode::SphericalPlanet:
            return "SphericalPlanet";
        case WorldTopologyMode::ToroidalWrap:
            return "ToroidalWrap";
        default:
            return "Unknown";
        }
    }

    DVec3 MakeDVec3(double x, double y, double z)
    {
        return {x, y, z};
    }

    bool IsFinite(DVec3 value)
    {
        return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
    }

    double Dot(DVec3 a, DVec3 b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    DVec3 Cross(DVec3 a, DVec3 b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    DVec3 Add(DVec3 a, DVec3 b)
    {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    DVec3 Subtract(DVec3 a, DVec3 b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    DVec3 Multiply(DVec3 value, double scalar)
    {
        return {value.x * scalar, value.y * scalar, value.z * scalar};
    }

    double LengthSquared(DVec3 value)
    {
        return Dot(value, value);
    }

    double Length(DVec3 value)
    {
        return std::sqrt(std::max(0.0, LengthSquared(value)));
    }

    DVec3 Normalize(DVec3 value, DVec3 fallback)
    {
        const double length = Length(value);
        if (length <= std::numeric_limits<double>::epsilon() || !IsFinite(length))
        {
            return fallback;
        }
        return {value.x / length, value.y / length, value.z / length};
    }

    Vec3 ToFloatVec3(DVec3 value)
    {
        return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
    }

    double DegreesToRadians(double degrees)
    {
        return degrees * Pi / 180.0;
    }

    double RadiansToDegrees(double radians)
    {
        return radians * 180.0 / Pi;
    }

    double WrapLongitudeRadians(double longitudeRadians)
    {
        if (!IsFinite(longitudeRadians))
        {
            return 0.0;
        }

        double wrapped = std::fmod(longitudeRadians + Pi, Pi * 2.0);
        if (wrapped < 0.0)
        {
            wrapped += Pi * 2.0;
        }
        return wrapped - Pi;
    }

    double ClampLatitudeRadians(double latitudeRadians)
    {
        if (!IsFinite(latitudeRadians))
        {
            return 0.0;
        }
        constexpr double MaxLatitude = Pi * 0.5;
        return std::clamp(latitudeRadians, -MaxLatitude, MaxLatitude);
    }

    DVec3 PlanetGeodeticToCartesian(GeodeticPosition geodetic, const PlanetSurfaceConfig& config)
    {
        const double radius = std::max(1.0, config.radiusMeters + geodetic.altitudeMeters);
        const double latitude = ClampLatitudeRadians(geodetic.latitudeRadians);
        const double longitude = WrapLongitudeRadians(geodetic.longitudeRadians);
        const double cosLat = std::cos(latitude);
        return {
            radius * cosLat * std::cos(longitude),
            radius * std::sin(latitude),
            radius * cosLat * std::sin(longitude)
        };
    }

    GeodeticPosition PlanetCartesianToGeodetic(DVec3 positionMeters, const PlanetSurfaceConfig& config)
    {
        const double radius = Length(positionMeters);
        if (radius <= 1.0 || !IsFinite(radius))
        {
            return {};
        }

        GeodeticPosition result{};
        result.latitudeRadians = std::asin(std::clamp(positionMeters.y / radius, -1.0, 1.0));
        result.longitudeRadians = WrapLongitudeRadians(std::atan2(positionMeters.z, positionMeters.x));
        result.altitudeMeters = radius - config.radiusMeters;
        return result;
    }

    double PlanetGravityAtAltitude(double altitudeMeters, const PlanetSurfaceConfig& config)
    {
        const double radius = std::max(1.0, config.radiusMeters);
        const double distance = std::max(1.0, radius + altitudeMeters);
        if (!config.inverseSquareGravity)
        {
            return config.gravityMetersPerSecondSquared;
        }
        const double ratio = radius / distance;
        return config.gravityMetersPerSecondSquared * ratio * ratio;
    }

    SurfaceFrame BuildPlanetSurfaceFrame(GeodeticPosition geodetic, const PlanetSurfaceConfig& config)
    {
        return BuildPlanetSurfaceFrame(PlanetGeodeticToCartesian(geodetic, config), config);
    }

    SurfaceFrame BuildPlanetSurfaceFrame(DVec3 positionMeters, const PlanetSurfaceConfig& config)
    {
        SurfaceFrame frame{};
        frame.positionMeters = positionMeters;
        frame.finite = IsFinite(positionMeters);

        const double distance = Length(positionMeters);
        if (distance <= 1.0 || !IsFinite(distance))
        {
            frame.finite = false;
            frame.up = {0.0, 1.0, 0.0};
            frame.east = {1.0, 0.0, 0.0};
            frame.north = {0.0, 0.0, 1.0};
            frame.gravity = {0.0, -config.gravityMetersPerSecondSquared, 0.0};
            frame.gravityMagnitudeMetersPerSecondSquared = config.gravityMetersPerSecondSquared;
            return frame;
        }

        frame.up = Normalize(positionMeters, {0.0, 1.0, 0.0});
        const DVec3 globalNorthPole = {0.0, 1.0, 0.0};
        DVec3 east = Cross(frame.up, globalNorthPole);
        if (LengthSquared(east) < 1.0e-12)
        {
            east = Cross(frame.up, {0.0, 0.0, 1.0});
        }
        frame.east = Normalize(east, {1.0, 0.0, 0.0});
        frame.north = Normalize(Cross(frame.east, frame.up), {0.0, 0.0, 1.0});
        frame.altitudeMeters = distance - config.radiusMeters;
        frame.gravityMagnitudeMetersPerSecondSquared = PlanetGravityAtAltitude(frame.altitudeMeters, config);
        frame.gravity = Multiply(frame.up, -frame.gravityMagnitudeMetersPerSecondSquared);
        return frame;
    }

    GeodeticPosition MoveOnPlanetSurface(GeodeticPosition start, double eastMeters, double northMeters, const PlanetSurfaceConfig& config)
    {
        const double radius = std::max(1.0, config.radiusMeters + start.altitudeMeters);
        const DVec3 startPosition = PlanetGeodeticToCartesian(start, config);
        const SurfaceFrame startFrame = BuildPlanetSurfaceFrame(startPosition, config);
        const DVec3 tangentOffset = Add(Multiply(startFrame.east, eastMeters), Multiply(startFrame.north, northMeters));
        const double distance = Length(tangentOffset);
        if (distance <= std::numeric_limits<double>::epsilon())
        {
            return start;
        }

        const DVec3 direction = Normalize(tangentOffset, startFrame.east);
        const double angle = distance / radius;
        const DVec3 rotated = Add(Multiply(startFrame.up, std::cos(angle)), Multiply(direction, std::sin(angle)));
        GeodeticPosition result = PlanetCartesianToGeodetic(Multiply(Normalize(rotated, startFrame.up), radius), config);
        result.altitudeMeters = start.altitudeMeters;
        return result;
    }

    WorldPosition PlanetGeodeticToWorldPosition(GeodeticPosition geodetic, double cellSizeMeters, const PlanetSurfaceConfig& config)
    {
        const DVec3 cartesian = PlanetGeodeticToCartesian(geodetic, config);
        return MakeWorldPosition(cartesian.x, cartesian.y, cartesian.z, cellSizeMeters);
    }

    ToroidalPosition WrapToroidalPosition(double xMeters, double zMeters, const ToroidalWorldConfig& config)
    {
        ToroidalPosition result{};
        result.x = PositiveModulo(xMeters, config.widthMeters);
        result.z = PositiveModulo(zMeters, config.depthMeters);
        result.tileX = FloorDivToTile(xMeters, config.widthMeters);
        result.tileZ = FloorDivToTile(zMeters, config.depthMeters);
        return result;
    }

    ToroidalPosition MoveToroidal(ToroidalPosition position, double deltaXMeters, double deltaZMeters, const ToroidalWorldConfig& config)
    {
        const double unwrappedX = static_cast<double>(position.tileX) * config.widthMeters + position.x + deltaXMeters;
        const double unwrappedZ = static_cast<double>(position.tileZ) * config.depthMeters + position.z + deltaZMeters;
        return WrapToroidalPosition(unwrappedX, unwrappedZ, config);
    }

    DVec3 ShortestToroidalDelta(ToroidalPosition from, ToroidalPosition to, const ToroidalWorldConfig& config)
    {
        return {
            ShortestWrappedAxisDelta(from.x, to.x, config.widthMeters),
            0.0,
            ShortestWrappedAxisDelta(from.z, to.z, config.depthMeters)
        };
    }

    WorldPosition ToroidalToWorldPosition(ToroidalPosition position, double yMeters, double cellSizeMeters)
    {
        return MakeWorldPosition(position.x, yMeters, position.z, cellSizeMeters);
    }

    WorldCell PlanarTerrainCellFromXZ(double xMeters, double zMeters, const PlanarTerrainConfig& config)
    {
        const double chunkSize = std::max(1.0, config.chunkSizeMeters);
        WorldCell cell{};
        cell.x = static_cast<i64>(std::floor(xMeters / chunkSize));
        cell.z = static_cast<i64>(std::floor(zMeters / chunkSize));
        return cell;
    }

    DVec3 PlanarTerrainLocalInCell(double xMeters, double yMeters, double zMeters, const PlanarTerrainConfig& config)
    {
        const WorldCell cell = PlanarTerrainCellFromXZ(xMeters, zMeters, config);
        const double chunkSize = std::max(1.0, config.chunkSizeMeters);
        return {
            xMeters - static_cast<double>(cell.x) * chunkSize,
            yMeters,
            zMeters - static_cast<double>(cell.z) * chunkSize
        };
    }

    SurfaceFrame BuildPlanarTerrainFrame(double xMeters, double yMeters, double zMeters, const PlanarTerrainConfig& config)
    {
        (void)config;
        SurfaceFrame frame{};
        frame.positionMeters = {xMeters, yMeters, zMeters};
        frame.up = {0.0, 1.0, 0.0};
        frame.east = {1.0, 0.0, 0.0};
        frame.north = {0.0, 0.0, 1.0};
        frame.gravityMagnitudeMetersPerSecondSquared = EarthStandardGravityMetersPerSecondSquared;
        frame.gravity = {0.0, -frame.gravityMagnitudeMetersPerSecondSquared, 0.0};
        frame.altitudeMeters = yMeters;
        frame.finite = IsFinite(frame.positionMeters);
        return frame;
    }

    TopologyValidationReport ValidateWorldTopologyConfig(const WorldTopologyConfig& config)
    {
        TopologyValidationReport report{};
        if (config.planet.radiusMeters < 1000.0)
        {
            report.errors.push_back("planet radius is too small for stable spherical-world math");
        }
        if (config.planet.gravityMetersPerSecondSquared <= 0.0 || !IsFinite(config.planet.gravityMetersPerSecondSquared))
        {
            report.errors.push_back("planet gravity must be positive and finite");
        }
        if (config.planar.chunkSizeMeters <= 0.0 || !IsFinite(config.planar.chunkSizeMeters))
        {
            report.errors.push_back("planar terrain chunk size must be positive and finite");
        }
        if (config.toroidal.widthMeters <= 0.0 || config.toroidal.depthMeters <= 0.0)
        {
            report.errors.push_back("toroidal dimensions must be positive");
        }
        if (config.mode == WorldTopologyMode::SphericalPlanet && config.planet.radiusMeters < 100000.0)
        {
            report.warnings.push_back("spherical mode uses a very small planet radius; curvature will be extreme");
        }
        if (config.mode == WorldTopologyMode::ToroidalWrap && (config.toroidal.widthMeters < 1.0 || config.toroidal.depthMeters < 1.0))
        {
            report.warnings.push_back("toroidal world is smaller than 1 meter on at least one axis");
        }
        report.ok = report.errors.empty();
        return report;
    }

    std::string ToDebugString(DVec3 value, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "(" << value.x << ", " << value.y << ", " << value.z << ")";
        return out.str();
    }

    std::string ToDebugString(GeodeticPosition value, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "lat=" << RadiansToDegrees(value.latitudeRadians)
            << " lon=" << RadiansToDegrees(value.longitudeRadians)
            << " alt=" << value.altitudeMeters << "m";
        return out.str();
    }

    std::string ToDebugString(const SurfaceFrame& frame, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "pos=" << ToDebugString(frame.positionMeters, precision)
            << " up=" << ToDebugString(frame.up, precision)
            << " g=" << frame.gravityMagnitudeMetersPerSecondSquared
            << "m/s^2 alt=" << frame.altitudeMeters << "m";
        return out.str();
    }

    std::string ToDebugString(const ToroidalPosition& position, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "local(" << position.x << ", " << position.z << ") tile(" << position.tileX << ", " << position.tileZ << ")";
        return out.str();
    }

    std::string ToDebugString(const TopologyValidationReport& report)
    {
        std::ostringstream out;
        out << "Topology validation: " << (report.ok ? "ok" : "failed")
            << " warnings=" << report.warnings.size()
            << " errors=" << report.errors.size();
        if (!report.errors.empty())
        {
            out << " first-error=" << report.errors.front();
        }
        else if (!report.warnings.empty())
        {
            out << " first-warning=" << report.warnings.front();
        }
        return out.str();
    }

    WorldTopologyProbeResult BuildWorldTopologyProbe()
    {
        WorldTopologyProbeResult result{};
        WorldTopologyConfig config{};
        config.mode = WorldTopologyMode::SphericalPlanet;
        config.planet.radiusMeters = EarthMeanRadiusMeters;
        config.toroidal.widthMeters = 256.0;
        config.toroidal.depthMeters = 128.0;
        result.validation = ValidateWorldTopologyConfig(config);

        result.earthEquator = {0.0, 0.0, 120.0};
        result.planetFrame = BuildPlanetSurfaceFrame(result.earthEquator, config.planet);
        const GeodeticPosition moved = MoveOnPlanetSurface(result.earthEquator, 1000.0, 0.0, config.planet);
        const double eastMoveErrorMeters = std::fabs((moved.longitudeRadians * config.planet.radiusMeters) - 1000.0);

        result.wrapped = MoveToroidal(WrapToroidalPosition(250.0, 120.0, config.toroidal), 20.0, 20.0, config.toroidal);
        const ToroidalPosition origin = WrapToroidalPosition(2.0, 4.0, config.toroidal);
        result.shortestToroidalDelta = ShortestToroidalDelta(result.wrapped, origin, config.toroidal);

        const bool planetOk = result.planetFrame.finite
            && std::fabs(Length(result.planetFrame.up) - 1.0) < 1.0e-6
            && result.planetFrame.gravityMagnitudeMetersPerSecondSquared > 9.0
            && eastMoveErrorMeters < 0.25;
        const bool torusOk = result.wrapped.x >= 0.0 && result.wrapped.x < config.toroidal.widthMeters
            && result.wrapped.z >= 0.0 && result.wrapped.z < config.toroidal.depthMeters
            && Length(result.shortestToroidalDelta) <= std::sqrt(config.toroidal.widthMeters * config.toroidal.widthMeters + config.toroidal.depthMeters * config.toroidal.depthMeters);

        result.ok = result.validation.ok && planetOk && torusOk;

        std::ostringstream out;
        out << "World topology probe: " << (result.ok ? "ok" : "failed")
            << " mode=" << ToString(config.mode)
            << " earthR=" << static_cast<i64>(config.planet.radiusMeters)
            << " gravity=" << std::fixed << std::setprecision(3) << result.planetFrame.gravityMagnitudeMetersPerSecondSquared
            << " torus=" << ToDebugString(result.wrapped, 2);
        result.summary = out.str();
        return result;
    }

    std::string BuildWorldTopologyProbeSummary()
    {
        return BuildWorldTopologyProbe().summary;
    }
}
