#include <AK/Gravity/GravityField.hpp>

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
        double ClampMagnitude(double magnitude, double limit)
        {
            if (!IsFinite(magnitude) || magnitude < 0.0)
            {
                return 0.0;
            }
            if (limit > 0.0 && magnitude > limit)
            {
                return limit;
            }
            return magnitude;
        }

        double ZoneAttenuation(double distance, double innerRadius, double falloffRadius)
        {
            if (falloffRadius <= 0.0)
            {
                return 1.0;
            }
            if (distance <= innerRadius)
            {
                return 1.0;
            }
            if (distance >= falloffRadius)
            {
                return 0.0;
            }
            const double t = (distance - innerRadius) / std::max(1.0e-9, falloffRadius - innerRadius);
            return std::clamp(1.0 - t, 0.0, 1.0);
        }
    }

    const char* ToString(GravityFieldKind kind)
    {
        switch (kind)
        {
        case GravityFieldKind::Uniform:
            return "Uniform";
        case GravityFieldKind::Planet:
            return "Planet";
        case GravityFieldKind::Point:
            return "Point";
        case GravityFieldKind::SphericalZone:
            return "SphericalZone";
        case GravityFieldKind::Zero:
            return "Zero";
        default:
            return "Unknown";
        }
    }

    const char* ToString(GravityBlendMode mode)
    {
        switch (mode)
        {
        case GravityBlendMode::Replace:
            return "Replace";
        case GravityBlendMode::Additive:
            return "Additive";
        case GravityBlendMode::Strongest:
            return "Strongest";
        default:
            return "Unknown";
        }
    }

    const char* ToString(SurfaceMovementMode mode)
    {
        switch (mode)
        {
        case SurfaceMovementMode::FreeSpace:
            return "FreeSpace";
        case SurfaceMovementMode::PlanarSurface:
            return "PlanarSurface";
        case SurfaceMovementMode::PlanetSurface:
            return "PlanetSurface";
        case SurfaceMovementMode::ToroidalSurface:
            return "ToroidalSurface";
        default:
            return "Unknown";
        }
    }

    GravityFieldDesc MakeUniformGravityField(DVec3 direction, double accelerationMetersPerSecondSquared)
    {
        GravityFieldDesc field{};
        field.kind = GravityFieldKind::Uniform;
        field.blendMode = GravityBlendMode::Additive;
        field.direction = Normalize(direction, {0.0, -1.0, 0.0});
        field.accelerationMetersPerSecondSquared = std::max(0.0, accelerationMetersPerSecondSquared);
        field.debugName = "uniform";
        return field;
    }

    GravityFieldDesc MakePlanetGravityField(const PlanetSurfaceConfig& config, DVec3 centerMeters)
    {
        GravityFieldDesc field{};
        field.kind = GravityFieldKind::Planet;
        field.blendMode = GravityBlendMode::Additive;
        field.centerMeters = centerMeters;
        field.accelerationMetersPerSecondSquared = config.gravityMetersPerSecondSquared;
        field.radiusMeters = config.radiusMeters;
        field.inverseSquare = config.inverseSquareGravity;
        field.debugName = "planet";
        return field;
    }

    GravityFieldDesc MakePointGravityField(DVec3 centerMeters, double accelerationMetersPerSecondSquared, double radiusMeters, bool inverseSquare)
    {
        GravityFieldDesc field{};
        field.kind = GravityFieldKind::Point;
        field.blendMode = GravityBlendMode::Additive;
        field.centerMeters = centerMeters;
        field.accelerationMetersPerSecondSquared = std::max(0.0, accelerationMetersPerSecondSquared);
        field.radiusMeters = std::max(1.0, radiusMeters);
        field.inverseSquare = inverseSquare;
        field.debugName = "point";
        return field;
    }

    GravityFieldDesc MakeZeroGravityField()
    {
        GravityFieldDesc field{};
        field.kind = GravityFieldKind::Zero;
        field.blendMode = GravityBlendMode::Replace;
        field.accelerationMetersPerSecondSquared = 0.0;
        field.enabled = true;
        field.debugName = "zero";
        return field;
    }

    GravitySample SampleGravityField(const GravityFieldDesc& field, DVec3 positionMeters, const GravitySolverConfig& config)
    {
        GravitySample sample{};
        sample.sourceKind = field.kind;
        sample.sourceName = field.debugName;
        sample.finite = field.enabled && IsFinite(positionMeters);
        sample.insideField = field.enabled;

        if (!field.enabled || !sample.finite)
        {
            sample.finite = false;
            sample.insideField = false;
            return sample;
        }

        DVec3 acceleration{};
        switch (field.kind)
        {
        case GravityFieldKind::Uniform:
        {
            acceleration = Multiply(Normalize(field.direction, {0.0, -1.0, 0.0}), field.accelerationMetersPerSecondSquared);
            break;
        }
        case GravityFieldKind::Planet:
        case GravityFieldKind::Point:
        case GravityFieldKind::SphericalZone:
        {
            const DVec3 toCenter = Subtract(field.centerMeters, positionMeters);
            const double distance = Length(toCenter);
            if (distance <= 1.0e-7 || !IsFinite(distance))
            {
                sample.finite = false;
                sample.insideField = false;
                return sample;
            }

            double magnitude = field.accelerationMetersPerSecondSquared;
            if (field.inverseSquare)
            {
                const double referenceRadius = std::max(1.0, field.radiusMeters);
                const double ratio = referenceRadius / std::max(1.0, distance);
                magnitude *= ratio * ratio;
            }

            if (field.kind == GravityFieldKind::SphericalZone)
            {
                const double attenuation = ZoneAttenuation(distance, field.innerRadiusMeters, field.falloffRadiusMeters > 0.0 ? field.falloffRadiusMeters : field.radiusMeters);
                magnitude *= attenuation;
                sample.insideField = attenuation > 0.0;
            }

            acceleration = Multiply(Normalize(toCenter, {0.0, -1.0, 0.0}), magnitude);
            break;
        }
        case GravityFieldKind::Zero:
            acceleration = {};
            break;
        default:
            sample.finite = false;
            return sample;
        }

        double magnitude = Length(acceleration);
        if (config.clampAcceleration)
        {
            const double clampedMagnitude = ClampMagnitude(magnitude, config.maxAccelerationMetersPerSecondSquared);
            if (magnitude > 0.0 && clampedMagnitude != magnitude)
            {
                acceleration = Multiply(acceleration, clampedMagnitude / magnitude);
                magnitude = clampedMagnitude;
            }
        }

        sample.accelerationMetersPerSecondSquared = acceleration;
        sample.magnitudeMetersPerSecondSquared = magnitude;
        sample.up = magnitude <= config.zeroGravityThreshold ? DVec3{0.0, 1.0, 0.0} : Normalize(Multiply(acceleration, -1.0), {0.0, 1.0, 0.0});
        sample.finite = IsFinite(sample.accelerationMetersPerSecondSquared) && IsFinite(sample.up) && IsFinite(sample.magnitudeMetersPerSecondSquared);
        return sample;
    }

    GravitySample CombineGravitySamples(const std::vector<GravitySample>& samples, const GravitySolverConfig& config)
    {
        GravitySample combined{};
        combined.sourceKind = GravityFieldKind::Zero;
        combined.sourceName = "combined";
        combined.finite = true;
        combined.insideField = false;

        DVec3 acceleration{};
        double strongest = -1.0;
        bool hasReplace = false;

        for (const GravitySample& sample : samples)
        {
            if (!sample.finite || !sample.insideField)
            {
                continue;
            }

            combined.insideField = true;
            if (sample.magnitudeMetersPerSecondSquared > strongest)
            {
                strongest = sample.magnitudeMetersPerSecondSquared;
                combined.sourceKind = sample.sourceKind;
                combined.sourceName = sample.sourceName;
            }

            if (sample.sourceKind == GravityFieldKind::Zero)
            {
                acceleration = {};
                hasReplace = true;
                continue;
            }

            if (!hasReplace)
            {
                acceleration = Add(acceleration, sample.accelerationMetersPerSecondSquared);
            }
        }

        double magnitude = Length(acceleration);
        if (config.clampAcceleration)
        {
            const double clampedMagnitude = ClampMagnitude(magnitude, config.maxAccelerationMetersPerSecondSquared);
            if (magnitude > 0.0 && clampedMagnitude != magnitude)
            {
                acceleration = Multiply(acceleration, clampedMagnitude / magnitude);
                magnitude = clampedMagnitude;
            }
        }

        combined.accelerationMetersPerSecondSquared = acceleration;
        combined.magnitudeMetersPerSecondSquared = magnitude;
        combined.up = magnitude <= config.zeroGravityThreshold ? DVec3{0.0, 1.0, 0.0} : Normalize(Multiply(acceleration, -1.0), {0.0, 1.0, 0.0});
        combined.finite = IsFinite(combined.accelerationMetersPerSecondSquared) && IsFinite(combined.up);
        return combined;
    }

    GravitySample SampleGravityFields(const std::vector<GravityFieldDesc>& fields, DVec3 positionMeters, const GravitySolverConfig& config)
    {
        std::vector<GravitySample> additive;
        additive.reserve(fields.size());

        const GravityFieldDesc* strongestField = nullptr;
        GravitySample strongestSample{};
        double strongestPriority = -std::numeric_limits<double>::infinity();

        for (const GravityFieldDesc& field : fields)
        {
            const GravitySample sample = SampleGravityField(field, positionMeters, config);
            if (!sample.finite || !sample.insideField)
            {
                continue;
            }

            if (field.blendMode == GravityBlendMode::Replace)
            {
                if (!strongestField || field.priority >= strongestPriority)
                {
                    strongestField = &field;
                    strongestSample = sample;
                    strongestPriority = field.priority;
                }
            }
            else if (field.blendMode == GravityBlendMode::Strongest)
            {
                if (!strongestField || sample.magnitudeMetersPerSecondSquared > strongestSample.magnitudeMetersPerSecondSquared)
                {
                    strongestField = &field;
                    strongestSample = sample;
                    strongestPriority = field.priority;
                }
            }
            else
            {
                additive.push_back(sample);
            }
        }

        if (strongestField)
        {
            return strongestSample;
        }
        return CombineGravitySamples(additive, config);
    }

    SurfaceFrame BuildSurfaceFrame(const SurfaceAttachmentDesc& attachment)
    {
        switch (attachment.mode)
        {
        case SurfaceMovementMode::PlanetSurface:
            return BuildPlanetSurfaceFrame(attachment.geodetic, attachment.topology.planet);
        case SurfaceMovementMode::ToroidalSurface:
            return BuildPlanarTerrainFrame(attachment.toroidal.x, attachment.altitudeMeters, attachment.toroidal.z, attachment.topology.planar);
        case SurfaceMovementMode::PlanarSurface:
            return BuildPlanarTerrainFrame(attachment.positionMeters.x, attachment.positionMeters.y, attachment.positionMeters.z, attachment.topology.planar);
        case SurfaceMovementMode::FreeSpace:
        default:
        {
            SurfaceFrame frame{};
            frame.positionMeters = attachment.positionMeters;
            frame.up = {0.0, 1.0, 0.0};
            frame.east = {1.0, 0.0, 0.0};
            frame.north = {0.0, 0.0, 1.0};
            frame.gravity = {0.0, 0.0, 0.0};
            frame.gravityMagnitudeMetersPerSecondSquared = 0.0;
            frame.altitudeMeters = attachment.altitudeMeters;
            frame.finite = IsFinite(frame.positionMeters);
            return frame;
        }
        }
    }

    SurfaceAttachmentDesc AttachToPlanetSurface(GeodeticPosition position, const PlanetSurfaceConfig& planet)
    {
        SurfaceAttachmentDesc attachment{};
        attachment.mode = SurfaceMovementMode::PlanetSurface;
        attachment.topology.mode = WorldTopologyMode::SphericalPlanet;
        attachment.topology.planet = planet;
        attachment.geodetic = position;
        attachment.positionMeters = PlanetGeodeticToCartesian(position, planet);
        attachment.altitudeMeters = position.altitudeMeters;
        attachment.grounded = true;
        return attachment;
    }

    SurfaceAttachmentDesc AttachToPlanarSurface(double xMeters, double yMeters, double zMeters, const PlanarTerrainConfig& planar)
    {
        SurfaceAttachmentDesc attachment{};
        attachment.mode = SurfaceMovementMode::PlanarSurface;
        attachment.topology.mode = WorldTopologyMode::PlanarTerrain;
        attachment.topology.planar = planar;
        attachment.positionMeters = {xMeters, yMeters, zMeters};
        attachment.altitudeMeters = yMeters;
        attachment.grounded = true;
        return attachment;
    }

    SurfaceAttachmentDesc AttachToToroidalSurface(ToroidalPosition position, const ToroidalWorldConfig& toroidal)
    {
        SurfaceAttachmentDesc attachment{};
        attachment.mode = SurfaceMovementMode::ToroidalSurface;
        attachment.topology.mode = WorldTopologyMode::ToroidalWrap;
        attachment.topology.toroidal = toroidal;
        attachment.toroidal = position;
        attachment.positionMeters = {position.x, 0.0, position.z};
        attachment.altitudeMeters = 0.0;
        attachment.grounded = true;
        return attachment;
    }

    SurfaceMovementResult MoveSurfaceAttached(const SurfaceAttachmentDesc& attachment, const SurfaceMovementInput& input, const GravitySolverConfig& gravityConfig)
    {
        SurfaceMovementResult result{};
        result.attachment = attachment;
        result.finite = true;

        const SurfaceFrame startFrame = BuildSurfaceFrame(attachment);
        switch (attachment.mode)
        {
        case SurfaceMovementMode::PlanetSurface:
        {
            GeodeticPosition moved = MoveOnPlanetSurface(attachment.geodetic, input.eastMeters, input.northMeters, attachment.topology.planet);
            moved.altitudeMeters += input.verticalMeters;
            result.attachment = AttachToPlanetSurface(moved, attachment.topology.planet);
            result.frame = BuildPlanetSurfaceFrame(moved, attachment.topology.planet);
            result.gravity = SampleGravityField(MakePlanetGravityField(attachment.topology.planet), result.frame.positionMeters, gravityConfig);
            break;
        }
        case SurfaceMovementMode::ToroidalSurface:
        {
            const ToroidalPosition moved = MoveToroidal(attachment.toroidal, input.eastMeters, input.northMeters, attachment.topology.toroidal);
            result.wrapped = moved.tileX != attachment.toroidal.tileX || moved.tileZ != attachment.toroidal.tileZ;
            result.attachment = AttachToToroidalSurface(moved, attachment.topology.toroidal);
            result.attachment.altitudeMeters = attachment.altitudeMeters + input.verticalMeters;
            result.attachment.positionMeters.y = result.attachment.altitudeMeters;
            result.frame = BuildSurfaceFrame(result.attachment);
            result.gravity = SampleGravityField(MakeUniformGravityField(), result.frame.positionMeters, gravityConfig);
            break;
        }
        case SurfaceMovementMode::PlanarSurface:
        {
            result.attachment = attachment;
            result.attachment.positionMeters = Add(attachment.positionMeters, {input.eastMeters, input.verticalMeters, input.northMeters});
            result.attachment.altitudeMeters = result.attachment.positionMeters.y;
            result.frame = BuildSurfaceFrame(result.attachment);
            result.gravity = SampleGravityField(MakeUniformGravityField(), result.frame.positionMeters, gravityConfig);
            break;
        }
        case SurfaceMovementMode::FreeSpace:
        default:
        {
            result.attachment = attachment;
            result.attachment.positionMeters = Add(attachment.positionMeters, Add(Multiply(startFrame.east, input.eastMeters), Add(Multiply(startFrame.north, input.northMeters), Multiply(startFrame.up, input.verticalMeters))));
            result.frame = BuildSurfaceFrame(result.attachment);
            result.gravity = SampleGravityField(MakeZeroGravityField(), result.frame.positionMeters, gravityConfig);
            break;
        }
        }

        result.displacementMeters = Subtract(result.frame.positionMeters, startFrame.positionMeters);
        result.finite = result.frame.finite && result.gravity.finite && IsFinite(result.displacementMeters);
        (void)input.deltaSeconds;
        (void)input.keepAttachedToSurface;
        return result;
    }

    DVec3 ProjectVectorOnSurface(DVec3 vector, DVec3 surfaceUp)
    {
        const DVec3 up = Normalize(surfaceUp, {0.0, 1.0, 0.0});
        return Subtract(vector, Multiply(up, Dot(vector, up)));
    }

    DVec3 BuildSurfaceVelocity(DVec3 east, DVec3 north, double eastMetersPerSecond, double northMetersPerSecond)
    {
        return Add(Multiply(Normalize(east, {1.0, 0.0, 0.0}), eastMetersPerSecond), Multiply(Normalize(north, {0.0, 0.0, 1.0}), northMetersPerSecond));
    }

    bool IsGravitySampleStable(const GravitySample& sample)
    {
        return sample.finite
            && IsFinite(sample.accelerationMetersPerSecondSquared)
            && IsFinite(sample.up)
            && sample.magnitudeMetersPerSecondSquared >= 0.0
            && sample.magnitudeMetersPerSecondSquared < 100000.0;
    }

    std::string ToDebugString(const GravitySample& sample, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "gravity source=" << ToString(sample.sourceKind)
            << " g=" << sample.magnitudeMetersPerSecondSquared
            << " up=" << ToDebugString(sample.up, precision)
            << " accel=" << ToDebugString(sample.accelerationMetersPerSecondSquared, precision)
            << " finite=" << (sample.finite ? "yes" : "no");
        if (!sample.sourceName.empty())
        {
            out << " name=" << sample.sourceName;
        }
        return out.str();
    }

    std::string ToDebugString(const SurfaceMovementResult& result, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "surface mode=" << ToString(result.attachment.mode)
            << " pos=" << ToDebugString(result.frame.positionMeters, precision)
            << " disp=" << ToDebugString(result.displacementMeters, precision)
            << " g=" << result.gravity.magnitudeMetersPerSecondSquared
            << " wrapped=" << (result.wrapped ? "yes" : "no")
            << " finite=" << (result.finite ? "yes" : "no");
        return out.str();
    }

    GravityProbeResult BuildGravityProbe()
    {
        GravityProbeResult result{};

        const PlanetSurfaceConfig planet{};
        const GeodeticPosition equator{0.0, 0.0, 120.0};
        const DVec3 planetPosition = PlanetGeodeticToCartesian(equator, planet);

        result.uniform = SampleGravityField(MakeUniformGravityField(), {10.0, 100.0, 10.0});
        result.planet = SampleGravityField(MakePlanetGravityField(planet), planetPosition);
        result.point = SampleGravityField(MakePointGravityField({0.0, 0.0, 0.0}, 3.0, 100.0), {100.0, 0.0, 0.0});

        SurfaceAttachmentDesc planetAttachment = AttachToPlanetSurface(equator, planet);
        result.planetMove = MoveSurfaceAttached(planetAttachment, {250.0, 125.0, 0.0, 1.0, true});

        ToroidalWorldConfig torus{};
        torus.widthMeters = 64.0;
        torus.depthMeters = 64.0;
        SurfaceAttachmentDesc torusAttachment = AttachToToroidalSurface(WrapToroidalPosition(62.0, 63.0, torus), torus);
        result.toroidalMove = MoveSurfaceAttached(torusAttachment, {5.0, 4.0, 0.0, 1.0, true});

        result.ok = IsGravitySampleStable(result.uniform)
            && IsGravitySampleStable(result.planet)
            && IsGravitySampleStable(result.point)
            && result.planetMove.finite
            && result.toroidalMove.finite
            && result.toroidalMove.wrapped
            && std::fabs(result.planet.magnitudeMetersPerSecondSquared - EarthStandardGravityMetersPerSecondSquared) < 0.01;

        std::ostringstream out;
        out << "Gravity probe: " << (result.ok ? "ok" : "failed")
            << " uniform=" << std::fixed << std::setprecision(3) << result.uniform.magnitudeMetersPerSecondSquared
            << " planet=" << result.planet.magnitudeMetersPerSecondSquared
            << " moved=" << ToDebugString(result.planetMove.displacementMeters, 2)
            << " torusWrapped=" << (result.toroidalMove.wrapped ? "yes" : "no");
        result.summary = out.str();
        return result;
    }

    std::string BuildGravityProbeSummary()
    {
        return BuildGravityProbe().summary;
    }
}
