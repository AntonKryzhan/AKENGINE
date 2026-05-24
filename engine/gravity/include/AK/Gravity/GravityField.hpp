#pragma once

#include <AK/Core/Types.hpp>
#include <AK/World/WorldCoordinates.hpp>
#include <AK/WorldTopology/WorldTopology.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class GravityFieldKind : u32
    {
        Uniform,
        Planet,
        Point,
        SphericalZone,
        Zero
    };

    enum class GravityBlendMode : u32
    {
        Replace,
        Additive,
        Strongest
    };

    enum class SurfaceMovementMode : u32
    {
        FreeSpace,
        PlanarSurface,
        PlanetSurface,
        ToroidalSurface
    };

    struct GravityFieldDesc
    {
        GravityFieldKind kind = GravityFieldKind::Uniform;
        GravityBlendMode blendMode = GravityBlendMode::Additive;
        DVec3 centerMeters{};
        DVec3 direction{0.0, -1.0, 0.0};
        double accelerationMetersPerSecondSquared = EarthStandardGravityMetersPerSecondSquared;
        double radiusMeters = EarthMeanRadiusMeters;
        double innerRadiusMeters = 0.0;
        double falloffRadiusMeters = 0.0;
        double priority = 0.0;
        bool inverseSquare = false;
        bool enabled = true;
        std::string debugName = "gravity";
    };

    struct GravitySample
    {
        DVec3 accelerationMetersPerSecondSquared{};
        DVec3 up{0.0, 1.0, 0.0};
        double magnitudeMetersPerSecondSquared = 0.0;
        GravityFieldKind sourceKind = GravityFieldKind::Zero;
        bool finite = true;
        bool insideField = false;
        std::string sourceName;
    };

    struct GravitySolverConfig
    {
        double maxAccelerationMetersPerSecondSquared = 500.0;
        double zeroGravityThreshold = 1.0e-7;
        bool clampAcceleration = true;
    };

    struct SurfaceAttachmentDesc
    {
        SurfaceMovementMode mode = SurfaceMovementMode::PlanarSurface;
        WorldTopologyConfig topology{};
        GeodeticPosition geodetic{};
        ToroidalPosition toroidal{};
        DVec3 positionMeters{};
        DVec3 velocityMetersPerSecond{};
        bool grounded = true;
        double altitudeMeters = 0.0;
    };

    struct SurfaceMovementInput
    {
        double eastMeters = 0.0;
        double northMeters = 0.0;
        double verticalMeters = 0.0;
        double deltaSeconds = 1.0;
        bool keepAttachedToSurface = true;
    };

    struct SurfaceMovementResult
    {
        SurfaceAttachmentDesc attachment{};
        SurfaceFrame frame{};
        GravitySample gravity{};
        DVec3 displacementMeters{};
        bool wrapped = false;
        bool finite = true;
    };

    struct GravityProbeResult
    {
        bool ok = false;
        std::string summary;
        GravitySample uniform{};
        GravitySample planet{};
        GravitySample point{};
        SurfaceMovementResult planetMove{};
        SurfaceMovementResult toroidalMove{};
    };

    const char* ToString(GravityFieldKind kind);
    const char* ToString(GravityBlendMode mode);
    const char* ToString(SurfaceMovementMode mode);

    GravityFieldDesc MakeUniformGravityField(DVec3 direction = {0.0, -1.0, 0.0}, double accelerationMetersPerSecondSquared = EarthStandardGravityMetersPerSecondSquared);
    GravityFieldDesc MakePlanetGravityField(const PlanetSurfaceConfig& config = {}, DVec3 centerMeters = {0.0, 0.0, 0.0});
    GravityFieldDesc MakePointGravityField(DVec3 centerMeters, double accelerationMetersPerSecondSquared, double radiusMeters, bool inverseSquare = true);
    GravityFieldDesc MakeZeroGravityField();

    GravitySample SampleGravityField(const GravityFieldDesc& field, DVec3 positionMeters, const GravitySolverConfig& config = {});
    GravitySample CombineGravitySamples(const std::vector<GravitySample>& samples, const GravitySolverConfig& config = {});
    GravitySample SampleGravityFields(const std::vector<GravityFieldDesc>& fields, DVec3 positionMeters, const GravitySolverConfig& config = {});

    SurfaceFrame BuildSurfaceFrame(const SurfaceAttachmentDesc& attachment);
    SurfaceAttachmentDesc AttachToPlanetSurface(GeodeticPosition position, const PlanetSurfaceConfig& planet = {});
    SurfaceAttachmentDesc AttachToPlanarSurface(double xMeters, double yMeters, double zMeters, const PlanarTerrainConfig& planar = {});
    SurfaceAttachmentDesc AttachToToroidalSurface(ToroidalPosition position, const ToroidalWorldConfig& toroidal = {});
    SurfaceMovementResult MoveSurfaceAttached(const SurfaceAttachmentDesc& attachment, const SurfaceMovementInput& input, const GravitySolverConfig& gravityConfig = {});

    DVec3 ProjectVectorOnSurface(DVec3 vector, DVec3 surfaceUp);
    DVec3 BuildSurfaceVelocity(DVec3 east, DVec3 north, double eastMetersPerSecond, double northMetersPerSecond);
    bool IsGravitySampleStable(const GravitySample& sample);

    std::string ToDebugString(const GravitySample& sample, int precision = 3);
    std::string ToDebugString(const SurfaceMovementResult& result, int precision = 3);
    std::string BuildGravityProbeSummary();
    GravityProbeResult BuildGravityProbe();
}
