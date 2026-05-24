#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>

#include <array>
#include <string>
#include <vector>

namespace AK
{
    enum class VehicleDriveMode : u32
    {
        FrontWheelDrive = 0,
        RearWheelDrive = 1,
        AllWheelDrive = 2
    };

    enum class VehicleWheelRole : u32
    {
        FrontLeft = 0,
        FrontRight = 1,
        RearLeft = 2,
        RearRight = 3,
        Auxiliary = 4
    };

    enum VehicleWheelFlags : u32
    {
        VehicleWheel_None = 0u,
        VehicleWheel_Steer = 1u << 0u,
        VehicleWheel_Drive = 1u << 1u,
        VehicleWheel_Brake = 1u << 2u,
        VehicleWheel_Handbrake = 1u << 3u
    };

    struct VehicleWheelConfig
    {
        VehicleWheelRole role = VehicleWheelRole::Auxiliary;
        Vec3 localPosition{};
        float radius = 0.34f;
        float width = 0.22f;
        float suspensionRestLength = 0.38f;
        float suspensionMinLength = 0.08f;
        float suspensionMaxLength = 0.55f;
        float springStrength = 36000.0f;
        float damperStrength = 5200.0f;
        float tireLongitudinalGrip = 1.1f;
        float tireLateralGrip = 1.25f;
        float rollingResistance = 0.018f;
        float sprungMassKilograms = 350.0f;
        u32 flags = VehicleWheel_Brake;
    };

    struct VehicleConfig
    {
        float massKilograms = 1450.0f;
        float inverseMass = 1.0f / 1450.0f;
        float maxEngineForceNewton = 7600.0f;
        float maxBrakeForceNewton = 12000.0f;
        float maxHandbrakeForceNewton = 16000.0f;
        float maxSteerAngleDegrees = 34.0f;
        float aerodynamicDragCoefficient = 0.42f;
        float frontalAreaSquareMeters = 2.2f;
        float airDensityKgPerCubicMeter = 1.225f;
        float maxSpeedMetersPerSecond = 90.0f;
        float antiRollStrength = 6500.0f;
        VehicleDriveMode driveMode = VehicleDriveMode::RearWheelDrive;
        Vec3 forward{0.0f, 0.0f, 1.0f};
        Vec3 right{1.0f, 0.0f, 0.0f};
        Vec3 up{0.0f, 1.0f, 0.0f};
        PhysicsQueryFlags queryFlags = PhysicsQueryFlags::Default;
        std::vector<VehicleWheelConfig> wheels;
    };

    struct VehicleControlInput
    {
        float throttle = 0.0f;
        float brake = 0.0f;
        float steering = 0.0f;
        float handbrake = 0.0f;
    };

    struct VehicleBodyState
    {
        Vec3 position{};
        Quat orientation{};
        Vec3 linearVelocity{};
        Vec3 angularVelocity{};
        bool sleeping = false;
    };

    struct VehicleWheelState
    {
        bool grounded = false;
        bool sliding = false;
        PhysicsRaycastHit ground{};
        Vec3 attachPoint{};
        Vec3 contactPoint{};
        Vec3 contactNormal{0.0f, 1.0f, 0.0f};
        Vec3 forward{0.0f, 0.0f, 1.0f};
        Vec3 right{1.0f, 0.0f, 0.0f};
        float steerAngleDegrees = 0.0f;
        float suspensionLength = 0.0f;
        float suspensionCompression = 0.0f;
        float suspensionVelocity = 0.0f;
        float suspensionForceNewton = 0.0f;
        float normalForceNewton = 0.0f;
        float driveForceNewton = 0.0f;
        float brakeForceNewton = 0.0f;
        float lateralForceNewton = 0.0f;
        float longitudinalSlip = 0.0f;
        float lateralSlip = 0.0f;
        float surfaceFriction = 1.0f;
        float wheelAngularVelocityRadPerSec = 0.0f;
        float wheelRpm = 0.0f;
    };

    struct VehicleStepResult
    {
        VehicleBodyState state{};
        std::vector<VehicleWheelState> wheels;
        Vec3 totalForceNewton{};
        Vec3 accelerationMetersPerSecondSquared{};
        float speedMetersPerSecond = 0.0f;
        float forwardSpeedMetersPerSecond = 0.0f;
        std::size_t groundedWheelCount = 0;
        std::size_t slidingWheelCount = 0;
        std::size_t raycastCount = 0;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct VehicleComponent
    {
        VehicleConfig config{};
        VehicleBodyState state{};
        VehicleControlInput input{};
        std::vector<VehicleWheelState> wheelStates;
        bool enabled = true;
    };

    struct VehicleSystemStats
    {
        std::size_t vehicleCount = 0;
        std::size_t updatedCount = 0;
        std::size_t wheelCount = 0;
        std::size_t groundedWheelCount = 0;
        std::size_t slidingWheelCount = 0;
        std::size_t raycastCount = 0;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct VehicleProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        VehicleConfig config{};
        VehicleBodyState before{};
        VehicleStepResult result{};
        VehicleSystemStats stats{};
    };

    const char* ToString(VehicleDriveMode mode);
    const char* ToString(VehicleWheelRole role);
    bool HasVehicleWheelFlag(u32 flags, VehicleWheelFlags flag);

    VehicleConfig MakeDefaultVehicleConfig();
    VehicleConfig SanitizeVehicleConfig(VehicleConfig config);
    VehicleBodyState MakeVehicleBodyState(Vec3 position = {}, Vec3 linearVelocity = {});
    VehicleControlInput SanitizeVehicleInput(VehicleControlInput input);
    bool IsDriveWheel(const VehicleConfig& config, const VehicleWheelConfig& wheel);
    VehicleStepResult StepVehicle(const PhysicsScene& scene, const VehicleConfig& config, const VehicleBodyState& state, const VehicleControlInput& input, float deltaSeconds);
    VehicleSystemStats FixedUpdateVehicles(const PhysicsScene& scene, std::vector<VehicleComponent>& vehicles, float deltaSeconds);

    std::string ToDebugString(const VehicleConfig& config, int precision = 2);
    std::string ToDebugString(const VehicleWheelState& wheel, int precision = 3);
    std::string ToDebugString(const VehicleStepResult& result, int precision = 3);
    std::string ToDebugString(const VehicleSystemStats& stats);
    std::string BuildVehicleProbeSummary();
    VehicleProbeResult BuildVehicleProbe();
}
