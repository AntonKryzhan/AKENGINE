#include <AK/World/WorldCoordinates.hpp>

#include <AK/Math/Geometry.hpp>
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
        i64 FloorToCell(double value, double cellSizeMeters)
        {
            return static_cast<i64>(std::floor(value / cellSizeMeters));
        }

        void NormalizeAxis(i64& cell, double& local, double cellSizeMeters)
        {
            if (!IsFinite(local) || cellSizeMeters <= 0.0 || !IsFinite(cellSizeMeters))
            {
                return;
            }

            const double half = cellSizeMeters * 0.5;
            if (local >= -half && local < half)
            {
                return;
            }

            const double shifted = local + half;
            const i64 delta = FloorToCell(shifted, cellSizeMeters);
            cell += delta;
            local -= static_cast<double>(delta) * cellSizeMeters;
        }

        double AbsoluteAxis(i64 cell, double local, double cellSizeMeters)
        {
            return static_cast<double>(cell) * cellSizeMeters + local;
        }
    }

    bool operator==(WorldCell a, WorldCell b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    bool operator!=(WorldCell a, WorldCell b)
    {
        return !(a == b);
    }

    bool IsFinite(WorldPosition position)
    {
        return IsFinite(position.localX) && IsFinite(position.localY) && IsFinite(position.localZ);
    }

    WorldPosition NormalizeWorldPosition(WorldPosition position, double cellSizeMeters)
    {
        if (cellSizeMeters <= 0.0 || !IsFinite(cellSizeMeters))
        {
            return position;
        }

        NormalizeAxis(position.cell.x, position.localX, cellSizeMeters);
        NormalizeAxis(position.cell.y, position.localY, cellSizeMeters);
        NormalizeAxis(position.cell.z, position.localZ, cellSizeMeters);
        return position;
    }

    WorldPosition MakeWorldPosition(double x, double y, double z, double cellSizeMeters)
    {
        if (cellSizeMeters <= 0.0 || !IsFinite(cellSizeMeters))
        {
            return {};
        }

        WorldPosition position{};
        position.cell.x = FloorToCell(x + cellSizeMeters * 0.5, cellSizeMeters);
        position.cell.y = FloorToCell(y + cellSizeMeters * 0.5, cellSizeMeters);
        position.cell.z = FloorToCell(z + cellSizeMeters * 0.5, cellSizeMeters);
        position.localX = x - static_cast<double>(position.cell.x) * cellSizeMeters;
        position.localY = y - static_cast<double>(position.cell.y) * cellSizeMeters;
        position.localZ = z - static_cast<double>(position.cell.z) * cellSizeMeters;
        return NormalizeWorldPosition(position, cellSizeMeters);
    }

    WorldPosition MakeWorldPosition(WorldCell cell, double localX, double localY, double localZ, double cellSizeMeters)
    {
        WorldPosition position{};
        position.cell = cell;
        position.localX = localX;
        position.localY = localY;
        position.localZ = localZ;
        return NormalizeWorldPosition(position, cellSizeMeters);
    }

    Vec3 GetLocalFloat(WorldPosition position)
    {
        return {
            static_cast<float>(position.localX),
            static_cast<float>(position.localY),
            static_cast<float>(position.localZ)
        };
    }

    Vec3 GetCellDeltaMeters(WorldCell from, WorldCell to, double cellSizeMeters)
    {
        return {
            static_cast<float>(static_cast<double>(to.x - from.x) * cellSizeMeters),
            static_cast<float>(static_cast<double>(to.y - from.y) * cellSizeMeters),
            static_cast<float>(static_cast<double>(to.z - from.z) * cellSizeMeters)
        };
    }

    Vec3 GetRelativePositionMeters(WorldPosition position, WorldPosition origin, double cellSizeMeters)
    {
        const double dx = static_cast<double>(position.cell.x - origin.cell.x) * cellSizeMeters + (position.localX - origin.localX);
        const double dy = static_cast<double>(position.cell.y - origin.cell.y) * cellSizeMeters + (position.localY - origin.localY);
        const double dz = static_cast<double>(position.cell.z - origin.cell.z) * cellSizeMeters + (position.localZ - origin.localZ);
        return {static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)};
    }

    CameraRelativePosition ToCameraRelativeFloat(WorldPosition position, WorldPosition camera, const LargeWorldConfig& config)
    {
        CameraRelativePosition result{};
        if (config.cellSizeMeters <= 0.0 || !IsFinite(config.cellSizeMeters) || !IsFinite(position) || !IsFinite(camera))
        {
            result.finite = false;
            result.precisionRisk = true;
            return result;
        }

        const double dx = static_cast<double>(position.cell.x - camera.cell.x) * config.cellSizeMeters + (position.localX - camera.localX);
        const double dy = static_cast<double>(position.cell.y - camera.cell.y) * config.cellSizeMeters + (position.localY - camera.localY);
        const double dz = static_cast<double>(position.cell.z - camera.cell.z) * config.cellSizeMeters + (position.localZ - camera.localZ);
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        result.value = {static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)};
        result.distanceMeters = distance;
        result.estimatedFloatStepMeters = EstimateFloatStepMeters(distance);
        result.finite = IsFinite(result.value);
        result.precisionRisk = !result.finite || distance > config.maxCameraRelativeMeters || result.estimatedFloatStepMeters > 0.01;
        return result;
    }

    WorldPosition AddLocalOffset(WorldPosition position, Vec3 offsetMeters, double cellSizeMeters)
    {
        position.localX += static_cast<double>(offsetMeters.x);
        position.localY += static_cast<double>(offsetMeters.y);
        position.localZ += static_cast<double>(offsetMeters.z);
        return NormalizeWorldPosition(position, cellSizeMeters);
    }

    WorldPosition RebaseToOrigin(WorldPosition position, WorldPosition newOrigin, double cellSizeMeters)
    {
        const double x = AbsoluteAxis(position.cell.x, position.localX, cellSizeMeters) - AbsoluteAxis(newOrigin.cell.x, newOrigin.localX, cellSizeMeters);
        const double y = AbsoluteAxis(position.cell.y, position.localY, cellSizeMeters) - AbsoluteAxis(newOrigin.cell.y, newOrigin.localY, cellSizeMeters);
        const double z = AbsoluteAxis(position.cell.z, position.localZ, cellSizeMeters) - AbsoluteAxis(newOrigin.cell.z, newOrigin.localZ, cellSizeMeters);
        return MakeWorldPosition(x, y, z, cellSizeMeters);
    }

    bool ShouldRebaseOrigin(WorldPosition camera, WorldOrigin origin, const LargeWorldConfig& config)
    {
        const Vec3 delta = GetRelativePositionMeters(camera, origin.position, config.cellSizeMeters);
        const double distance = static_cast<double>(Length(delta));
        return distance >= config.rebaseThresholdMeters;
    }

    WorldOrigin BuildOriginFromCamera(WorldPosition camera, double cellSizeMeters)
    {
        WorldOrigin origin{};
        origin.position = NormalizeWorldPosition(camera, cellSizeMeters);
        return origin;
    }

    double EstimateFloatStepMeters(double magnitudeMeters)
    {
        const double magnitude = std::fabs(magnitudeMeters);
        if (!IsFinite(magnitude) || magnitude == 0.0)
        {
            return std::numeric_limits<float>::denorm_min();
        }

        const double exponent = std::floor(std::log2(magnitude));
        return std::ldexp(1.0, static_cast<int>(exponent) - 23);
    }

    double EstimateDepthResolutionRatio(float nearPlane, float farPlane)
    {
        if (nearPlane <= 0.0f || farPlane <= nearPlane || !IsFinite(nearPlane) || !IsFinite(farPlane))
        {
            return 0.0;
        }

        return static_cast<double>(farPlane) / static_cast<double>(nearPlane);
    }

    std::string ToDebugString(WorldCell cell)
    {
        std::ostringstream out;
        out << "cell(" << cell.x << ", " << cell.y << ", " << cell.z << ")";
        return out.str();
    }

    std::string ToDebugString(WorldPosition position, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << ToDebugString(position.cell)
            << " local(" << position.localX << ", " << position.localY << ", " << position.localZ << ")";
        return out.str();
    }

    std::string BuildLargeWorldProbeSummary()
    {
        const LargeWorldConfig config{};
        const WorldPosition camera = MakeWorldPosition(1000000.125, 12.0, -999999.875, config.cellSizeMeters);
        const WorldPosition object = AddLocalOffset(camera, {1.0f, 0.0f, 1.0f}, config.cellSizeMeters);
        const CameraRelativePosition relative = ToCameraRelativeFloat(object, camera, config);
        const double directFloatStep = EstimateFloatStepMeters(1000000.0);

        std::ostringstream out;
        out << std::fixed << std::setprecision(6)
            << "LargeWorld: direct float step at 1e6m ~= " << directFloatStep
            << "m, camera-relative step ~= " << relative.estimatedFloatStepMeters << "m";
        return out.str();
    }
}
