#pragma once

#include <AK/Core/Types.hpp>

#include <cstdint>
#include <string>

namespace AK
{
    struct WorldCell
    {
        i64 x = 0;
        i64 y = 0;
        i64 z = 0;
    };

    struct WorldPosition
    {
        WorldCell cell{};
        double localX = 0.0;
        double localY = 0.0;
        double localZ = 0.0;
    };

    struct WorldOrigin
    {
        WorldPosition position{};
    };

    struct LargeWorldConfig
    {
        double cellSizeMeters = 1024.0;
        double rebaseThresholdMeters = 256.0;
        double maxCameraRelativeMeters = 100000.0;
    };

    struct CameraRelativePosition
    {
        Vec3 value{};
        double distanceMeters = 0.0;
        double estimatedFloatStepMeters = 0.0;
        bool finite = true;
        bool precisionRisk = false;
    };

    constexpr double DefaultWorldCellSizeMeters = 1024.0;
    constexpr double DefaultWorldRebaseThresholdMeters = 256.0;

    bool operator==(WorldCell a, WorldCell b);
    bool operator!=(WorldCell a, WorldCell b);

    bool IsFinite(WorldPosition position);
    WorldPosition NormalizeWorldPosition(WorldPosition position, double cellSizeMeters = DefaultWorldCellSizeMeters);
    WorldPosition MakeWorldPosition(double x, double y, double z, double cellSizeMeters = DefaultWorldCellSizeMeters);
    WorldPosition MakeWorldPosition(WorldCell cell, double localX, double localY, double localZ, double cellSizeMeters = DefaultWorldCellSizeMeters);

    Vec3 GetLocalFloat(WorldPosition position);
    Vec3 GetCellDeltaMeters(WorldCell from, WorldCell to, double cellSizeMeters = DefaultWorldCellSizeMeters);
    Vec3 GetRelativePositionMeters(WorldPosition position, WorldPosition origin, double cellSizeMeters = DefaultWorldCellSizeMeters);
    CameraRelativePosition ToCameraRelativeFloat(WorldPosition position, WorldPosition camera, const LargeWorldConfig& config = {});

    WorldPosition AddLocalOffset(WorldPosition position, Vec3 offsetMeters, double cellSizeMeters = DefaultWorldCellSizeMeters);
    WorldPosition RebaseToOrigin(WorldPosition position, WorldPosition newOrigin, double cellSizeMeters = DefaultWorldCellSizeMeters);
    bool ShouldRebaseOrigin(WorldPosition camera, WorldOrigin origin, const LargeWorldConfig& config = {});
    WorldOrigin BuildOriginFromCamera(WorldPosition camera, double cellSizeMeters = DefaultWorldCellSizeMeters);

    double EstimateFloatStepMeters(double magnitudeMeters);
    double EstimateDepthResolutionRatio(float nearPlane, float farPlane);
    std::string ToDebugString(WorldCell cell);
    std::string ToDebugString(WorldPosition position, int precision = 3);
    std::string BuildLargeWorldProbeSummary();
}
