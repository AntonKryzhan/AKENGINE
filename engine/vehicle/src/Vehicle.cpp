#include <AK/Vehicle/Vehicle.hpp>

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
        constexpr float MinimumVehicleMass = 1.0e-4f;
        constexpr float MinimumWheelRadius = 0.01f;
        constexpr float MinimumDeltaSeconds = 1.0e-6f;
        constexpr float MaximumReasonableForce = 1.0e7f;

        Vec3 Negate(Vec3 value)
        {
            return {-value.x, -value.y, -value.z};
        }

        float Clamp01Signed(float value)
        {
            return std::clamp(value, -1.0f, 1.0f);
        }

        Vec3 ProjectOnPlane(Vec3 value, Vec3 normal)
        {
            return Subtract(value, Multiply(normal, Dot(value, normal)));
        }

        Quat SafeOrientation(Quat value)
        {
            if (!IsFinite(Vec3{value.x, value.y, value.z}) || !IsFinite(value.w))
            {
                return QuatIdentity();
            }
            return Normalize(value);
        }

        Vec3 SafeDirection(Vec3 value, Vec3 fallback)
        {
            return Normalize(value, fallback);
        }

        Vec3 ClampLength(Vec3 value, float maxLength)
        {
            const float length = Length(value);
            if (length <= maxLength || length <= FloatEpsilon)
            {
                return value;
            }
            return Multiply(value, maxLength / length);
        }

        float SurfaceFrictionFromHit(const PhysicsScene& scene, const PhysicsRaycastHit& hit)
        {
            if (!hit.hit || hit.collider >= scene.colliders.size())
            {
                return 1.0f;
            }
            const PhysicsCollider& collider = scene.colliders[hit.collider];
            return std::clamp(collider.material.dynamicFriction, 0.05f, 4.0f);
        }

        VehicleWheelConfig MakeWheel(VehicleWheelRole role, Vec3 localPosition, u32 flags)
        {
            VehicleWheelConfig wheel{};
            wheel.role = role;
            wheel.localPosition = localPosition;
            wheel.flags = flags;
            return wheel;
        }

        bool IsFrontWheel(VehicleWheelRole role)
        {
            return role == VehicleWheelRole::FrontLeft || role == VehicleWheelRole::FrontRight;
        }

        bool IsRearWheel(VehicleWheelRole role)
        {
            return role == VehicleWheelRole::RearLeft || role == VehicleWheelRole::RearRight;
        }

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }

        float SafeMassInverse(float mass)
        {
            return mass > MinimumVehicleMass ? 1.0f / mass : 0.0f;
        }
    }

    const char* ToString(VehicleDriveMode mode)
    {
        switch (mode)
        {
        case VehicleDriveMode::FrontWheelDrive:
            return "FrontWheelDrive";
        case VehicleDriveMode::RearWheelDrive:
            return "RearWheelDrive";
        case VehicleDriveMode::AllWheelDrive:
            return "AllWheelDrive";
        default:
            return "Unknown";
        }
    }

    const char* ToString(VehicleWheelRole role)
    {
        switch (role)
        {
        case VehicleWheelRole::FrontLeft:
            return "FrontLeft";
        case VehicleWheelRole::FrontRight:
            return "FrontRight";
        case VehicleWheelRole::RearLeft:
            return "RearLeft";
        case VehicleWheelRole::RearRight:
            return "RearRight";
        case VehicleWheelRole::Auxiliary:
            return "Auxiliary";
        default:
            return "Unknown";
        }
    }

    bool HasVehicleWheelFlag(u32 flags, VehicleWheelFlags flag)
    {
        return (flags & static_cast<u32>(flag)) != 0u;
    }

    VehicleConfig MakeDefaultVehicleConfig()
    {
        VehicleConfig config{};
        config.massKilograms = 1450.0f;
        config.inverseMass = SafeMassInverse(config.massKilograms);
        config.driveMode = VehicleDriveMode::RearWheelDrive;
        config.wheels = {
            MakeWheel(VehicleWheelRole::FrontLeft, {-0.82f, 0.10f, 1.35f}, VehicleWheel_Steer | VehicleWheel_Brake),
            MakeWheel(VehicleWheelRole::FrontRight, {0.82f, 0.10f, 1.35f}, VehicleWheel_Steer | VehicleWheel_Brake),
            MakeWheel(VehicleWheelRole::RearLeft, {-0.82f, 0.10f, -1.25f}, VehicleWheel_Drive | VehicleWheel_Brake | VehicleWheel_Handbrake),
            MakeWheel(VehicleWheelRole::RearRight, {0.82f, 0.10f, -1.25f}, VehicleWheel_Drive | VehicleWheel_Brake | VehicleWheel_Handbrake)
        };
        return config;
    }

    VehicleConfig SanitizeVehicleConfig(VehicleConfig config)
    {
        config.massKilograms = std::max(MinimumVehicleMass, config.massKilograms);
        config.inverseMass = SafeMassInverse(config.massKilograms);
        config.maxEngineForceNewton = std::clamp(config.maxEngineForceNewton, 0.0f, MaximumReasonableForce);
        config.maxBrakeForceNewton = std::clamp(config.maxBrakeForceNewton, 0.0f, MaximumReasonableForce);
        config.maxHandbrakeForceNewton = std::clamp(config.maxHandbrakeForceNewton, 0.0f, MaximumReasonableForce);
        config.maxSteerAngleDegrees = std::clamp(config.maxSteerAngleDegrees, 0.0f, 75.0f);
        config.aerodynamicDragCoefficient = std::max(0.0f, config.aerodynamicDragCoefficient);
        config.frontalAreaSquareMeters = std::max(0.01f, config.frontalAreaSquareMeters);
        config.airDensityKgPerCubicMeter = std::max(0.0f, config.airDensityKgPerCubicMeter);
        config.maxSpeedMetersPerSecond = std::max(1.0f, config.maxSpeedMetersPerSecond);
        config.antiRollStrength = std::max(0.0f, config.antiRollStrength);
        config.forward = SafeDirection(ProjectOnPlane(config.forward, config.up), {0.0f, 0.0f, 1.0f});
        config.up = SafeDirection(config.up, {0.0f, 1.0f, 0.0f});
        config.right = SafeDirection(Cross(config.forward, config.up), {1.0f, 0.0f, 0.0f});

        if (config.wheels.empty())
        {
            config = MakeDefaultVehicleConfig();
        }

        for (VehicleWheelConfig& wheel : config.wheels)
        {
            wheel.radius = std::max(MinimumWheelRadius, wheel.radius);
            wheel.width = std::max(0.01f, wheel.width);
            wheel.suspensionRestLength = std::max(0.02f, wheel.suspensionRestLength);
            wheel.suspensionMinLength = std::clamp(wheel.suspensionMinLength, 0.0f, wheel.suspensionRestLength);
            wheel.suspensionMaxLength = std::max(wheel.suspensionRestLength, wheel.suspensionMaxLength);
            wheel.springStrength = std::clamp(wheel.springStrength, 0.0f, MaximumReasonableForce);
            wheel.damperStrength = std::clamp(wheel.damperStrength, 0.0f, MaximumReasonableForce);
            wheel.tireLongitudinalGrip = std::clamp(wheel.tireLongitudinalGrip, 0.05f, 5.0f);
            wheel.tireLateralGrip = std::clamp(wheel.tireLateralGrip, 0.05f, 5.0f);
            wheel.rollingResistance = std::clamp(wheel.rollingResistance, 0.0f, 1.0f);
            wheel.sprungMassKilograms = std::max(1.0f, wheel.sprungMassKilograms);
        }

        return config;
    }

    VehicleBodyState MakeVehicleBodyState(Vec3 position, Vec3 linearVelocity)
    {
        VehicleBodyState state{};
        state.position = position;
        state.orientation = QuatIdentity();
        state.linearVelocity = linearVelocity;
        return state;
    }

    VehicleControlInput SanitizeVehicleInput(VehicleControlInput input)
    {
        input.throttle = std::clamp(input.throttle, -1.0f, 1.0f);
        input.brake = std::clamp(input.brake, 0.0f, 1.0f);
        input.steering = Clamp01Signed(input.steering);
        input.handbrake = std::clamp(input.handbrake, 0.0f, 1.0f);
        return input;
    }

    bool IsDriveWheel(const VehicleConfig& config, const VehicleWheelConfig& wheel)
    {
        if (HasVehicleWheelFlag(wheel.flags, VehicleWheel_Drive))
        {
            return true;
        }
        if (config.driveMode == VehicleDriveMode::AllWheelDrive)
        {
            return true;
        }
        if (config.driveMode == VehicleDriveMode::FrontWheelDrive)
        {
            return IsFrontWheel(wheel.role);
        }
        if (config.driveMode == VehicleDriveMode::RearWheelDrive)
        {
            return IsRearWheel(wheel.role);
        }
        return false;
    }

    VehicleStepResult StepVehicle(const PhysicsScene& scene, const VehicleConfig& rawConfig, const VehicleBodyState& rawState, const VehicleControlInput& rawInput, float deltaSeconds)
    {
        const VehicleConfig config = SanitizeVehicleConfig(rawConfig);
        const VehicleControlInput input = SanitizeVehicleInput(rawInput);
        const float dt = std::max(MinimumDeltaSeconds, deltaSeconds);

        VehicleStepResult result{};
        result.state = rawState;
        result.state.orientation = SafeOrientation(result.state.orientation);
        result.wheels.resize(config.wheels.size());

        const Vec3 up = SafeDirection(Rotate(result.state.orientation, config.up), {0.0f, 1.0f, 0.0f});
        const Vec3 bodyForward = SafeDirection(ProjectOnPlane(Rotate(result.state.orientation, config.forward), up), {0.0f, 0.0f, 1.0f});
        const Vec3 bodyRight = SafeDirection(Cross(bodyForward, up), {1.0f, 0.0f, 0.0f});
        const float forwardSpeed = Dot(result.state.linearVelocity, bodyForward);

        Vec3 totalForce = Multiply(up, -9.80665f * config.massKilograms);
        const float speed = Length(result.state.linearVelocity);
        if (speed > FloatEpsilon)
        {
            const float dragMagnitude = 0.5f * config.airDensityKgPerCubicMeter * config.aerodynamicDragCoefficient * config.frontalAreaSquareMeters * speed * speed;
            totalForce = Add(totalForce, Multiply(Normalize(result.state.linearVelocity, bodyForward), -dragMagnitude));
        }

        const float steerAngle = input.steering * config.maxSteerAngleDegrees;
        const Quat steerRotation = QuatFromAxisAngle(up, steerAngle * DegToRad32);
        std::vector<float> leftCompression;
        std::vector<float> rightCompression;
        leftCompression.reserve(config.wheels.size());
        rightCompression.reserve(config.wheels.size());

        for (std::size_t wheelIndex = 0; wheelIndex < config.wheels.size(); ++wheelIndex)
        {
            const VehicleWheelConfig& wheel = config.wheels[wheelIndex];
            VehicleWheelState wheelState{};
            wheelState.steerAngleDegrees = HasVehicleWheelFlag(wheel.flags, VehicleWheel_Steer) ? steerAngle : 0.0f;
            wheelState.attachPoint = Add(result.state.position, Rotate(result.state.orientation, wheel.localPosition));
            wheelState.forward = bodyForward;
            if (HasVehicleWheelFlag(wheel.flags, VehicleWheel_Steer))
            {
                wheelState.forward = SafeDirection(ProjectOnPlane(Rotate(steerRotation, bodyForward), up), bodyForward);
            }
            wheelState.right = SafeDirection(Cross(wheelState.forward, up), bodyRight);

            const float rayLength = wheel.suspensionMaxLength + wheel.radius;
            const Ray3 ray{wheelState.attachPoint, Negate(up)};
            wheelState.ground = RaycastPhysicsScene(scene, ray, rayLength, config.queryFlags);
            result.raycastCount++;

            if (wheelState.ground.hit)
            {
                wheelState.grounded = true;
                result.groundedWheelCount++;
                wheelState.contactPoint = wheelState.ground.point;
                wheelState.contactNormal = Normalize(wheelState.ground.normal, up);
                wheelState.suspensionLength = std::clamp(wheelState.ground.distance - wheel.radius, wheel.suspensionMinLength, wheel.suspensionMaxLength);
                wheelState.suspensionCompression = std::clamp(wheel.suspensionRestLength - wheelState.suspensionLength, 0.0f, wheel.suspensionMaxLength);
                wheelState.suspensionVelocity = -Dot(result.state.linearVelocity, up);

                const float springForce = wheelState.suspensionCompression * wheel.springStrength;
                const float damperForce = wheelState.suspensionVelocity * wheel.damperStrength;
                wheelState.suspensionForceNewton = std::max(0.0f, springForce + damperForce);
                wheelState.normalForceNewton = wheelState.suspensionForceNewton;
                wheelState.surfaceFriction = SurfaceFrictionFromHit(scene, wheelState.ground);

                const float localForwardSpeed = Dot(result.state.linearVelocity, wheelState.forward);
                const float localSideSpeed = Dot(result.state.linearVelocity, wheelState.right);
                wheelState.longitudinalSlip = SafeDivide(localForwardSpeed, std::max(1.0f, std::fabs(forwardSpeed)), 0.0f);
                wheelState.lateralSlip = localSideSpeed;

                const float wheelFrictionLimit = wheelState.surfaceFriction * wheelState.normalForceNewton;
                if (IsDriveWheel(config, wheel))
                {
                    wheelState.driveForceNewton = input.throttle * config.maxEngineForceNewton / std::max(1.0f, static_cast<float>(config.wheels.size()));
                }
                if (HasVehicleWheelFlag(wheel.flags, VehicleWheel_Brake))
                {
                    wheelState.brakeForceNewton = input.brake * config.maxBrakeForceNewton / std::max(1.0f, static_cast<float>(config.wheels.size()));
                }
                if (HasVehicleWheelFlag(wheel.flags, VehicleWheel_Handbrake))
                {
                    wheelState.brakeForceNewton += input.handbrake * config.maxHandbrakeForceNewton / std::max(1.0f, static_cast<float>(config.wheels.size()));
                }

                float longitudinalForce = wheelState.driveForceNewton;
                if (std::fabs(localForwardSpeed) > 0.05f)
                {
                    longitudinalForce -= std::copysign(std::min(wheelState.brakeForceNewton, wheelFrictionLimit), localForwardSpeed);
                }
                longitudinalForce -= localForwardSpeed * wheel.rollingResistance * wheelState.normalForceNewton;
                longitudinalForce = std::clamp(longitudinalForce, -wheel.tireLongitudinalGrip * wheelFrictionLimit, wheel.tireLongitudinalGrip * wheelFrictionLimit);

                wheelState.lateralForceNewton = std::clamp(-localSideSpeed * wheel.tireLateralGrip * wheelState.normalForceNewton, -wheel.tireLateralGrip * wheelFrictionLimit, wheel.tireLateralGrip * wheelFrictionLimit);
                wheelState.sliding = std::fabs(wheelState.lateralForceNewton) >= 0.98f * wheel.tireLateralGrip * wheelFrictionLimit && std::fabs(localSideSpeed) > 0.5f;
                if (wheelState.sliding)
                {
                    result.slidingWheelCount++;
                }

                const Vec3 suspensionForce = Multiply(wheelState.contactNormal, wheelState.suspensionForceNewton);
                const Vec3 longitudinal = Multiply(wheelState.forward, longitudinalForce);
                const Vec3 lateral = Multiply(wheelState.right, wheelState.lateralForceNewton);
                totalForce = Add(totalForce, Add(suspensionForce, Add(longitudinal, lateral)));

                wheelState.wheelAngularVelocityRadPerSec = SafeDivide(localForwardSpeed, wheel.radius, 0.0f);
                wheelState.wheelRpm = wheelState.wheelAngularVelocityRadPerSec * 60.0f / TwoPi32;
            }
            else
            {
                wheelState.suspensionLength = wheel.suspensionMaxLength;
                wheelState.contactNormal = up;
                wheelState.surfaceFriction = 1.0f;
                wheelState.wheelAngularVelocityRadPerSec = SafeDivide(forwardSpeed, wheel.radius, 0.0f);
                wheelState.wheelRpm = wheelState.wheelAngularVelocityRadPerSec * 60.0f / TwoPi32;
            }

            if (wheel.localPosition.x < 0.0f)
            {
                leftCompression.push_back(wheelState.suspensionCompression);
            }
            else
            {
                rightCompression.push_back(wheelState.suspensionCompression);
            }

            result.wheels[wheelIndex] = wheelState;
        }

        if (!leftCompression.empty() && !rightCompression.empty() && config.antiRollStrength > 0.0f)
        {
            float left = 0.0f;
            float right = 0.0f;
            for (float value : leftCompression)
            {
                left += value;
            }
            for (float value : rightCompression)
            {
                right += value;
            }
            left /= static_cast<float>(leftCompression.size());
            right /= static_cast<float>(rightCompression.size());
            const float antiRollForce = (left - right) * config.antiRollStrength;
            totalForce = Add(totalForce, Multiply(bodyRight, -antiRollForce * 0.01f));
        }

        totalForce = ClampLength(totalForce, MaximumReasonableForce);
        result.totalForceNewton = totalForce;
        result.accelerationMetersPerSecondSquared = Multiply(totalForce, config.inverseMass);
        result.state.linearVelocity = Add(result.state.linearVelocity, Multiply(result.accelerationMetersPerSecondSquared, dt));
        if (Length(result.state.linearVelocity) > config.maxSpeedMetersPerSecond)
        {
            result.state.linearVelocity = ClampLength(result.state.linearVelocity, config.maxSpeedMetersPerSecond);
        }
        result.state.position = Add(result.state.position, Multiply(result.state.linearVelocity, dt));
        result.speedMetersPerSecond = Length(result.state.linearVelocity);
        result.forwardSpeedMetersPerSecond = Dot(result.state.linearVelocity, bodyForward);

        result.finite = IsFinite(result.state.position) && IsFinite(result.state.linearVelocity) && IsFinite(result.totalForceNewton) && IsFinite(result.accelerationMetersPerSecondSquared);
        if (!result.finite)
        {
            AddWarning(result.warnings, "vehicle step produced non-finite state");
        }
        if (result.groundedWheelCount == 0)
        {
            AddWarning(result.warnings, "vehicle has no grounded wheels");
        }

        return result;
    }

    VehicleSystemStats FixedUpdateVehicles(const PhysicsScene& scene, std::vector<VehicleComponent>& vehicles, float deltaSeconds)
    {
        VehicleSystemStats stats{};
        stats.vehicleCount = vehicles.size();
        stats.finite = true;

        for (VehicleComponent& vehicle : vehicles)
        {
            if (!vehicle.enabled)
            {
                continue;
            }
            VehicleStepResult step = StepVehicle(scene, vehicle.config, vehicle.state, vehicle.input, deltaSeconds);
            vehicle.state = step.state;
            vehicle.wheelStates = step.wheels;
            stats.updatedCount++;
            stats.wheelCount += step.wheels.size();
            stats.groundedWheelCount += step.groundedWheelCount;
            stats.slidingWheelCount += step.slidingWheelCount;
            stats.raycastCount += step.raycastCount;
            stats.finite = stats.finite && step.finite;
            for (const std::string& warning : step.warnings)
            {
                if (stats.warnings.size() < 16)
                {
                    stats.warnings.push_back(warning);
                }
            }
        }

        return stats;
    }

    std::string ToDebugString(const VehicleConfig& config, int precision)
    {
        const VehicleConfig sanitized = SanitizeVehicleConfig(config);
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "vehicle_config mass=" << sanitized.massKilograms
            << " drive=" << ToString(sanitized.driveMode)
            << " wheels=" << sanitized.wheels.size()
            << " engine=" << sanitized.maxEngineForceNewton
            << " brake=" << sanitized.maxBrakeForceNewton
            << " steer=" << sanitized.maxSteerAngleDegrees
            << " max_speed=" << sanitized.maxSpeedMetersPerSecond;
        return out.str();
    }

    std::string ToDebugString(const VehicleWheelState& wheel, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "wheel grounded=" << (wheel.grounded ? "true" : "false")
            << " sliding=" << (wheel.sliding ? "true" : "false")
            << " steer=" << wheel.steerAngleDegrees
            << " compression=" << wheel.suspensionCompression
            << " normal=" << wheel.normalForceNewton
            << " drive=" << wheel.driveForceNewton
            << " lateral=" << wheel.lateralForceNewton
            << " mu=" << wheel.surfaceFriction
            << " rpm=" << wheel.wheelRpm;
        return out.str();
    }

    std::string ToDebugString(const VehicleStepResult& result, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "vehicle_result grounded=" << result.groundedWheelCount
            << " sliding=" << result.slidingWheelCount
            << " raycasts=" << result.raycastCount
            << " speed=" << result.speedMetersPerSecond
            << " forward_speed=" << result.forwardSpeedMetersPerSecond
            << " force=" << ToDebugString(result.totalForceNewton, precision)
            << " pos=" << ToDebugString(result.state.position, precision)
            << " vel=" << ToDebugString(result.state.linearVelocity, precision)
            << " finite=" << (result.finite ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const VehicleSystemStats& stats)
    {
        std::ostringstream out;
        out << "vehicle_system vehicles=" << stats.vehicleCount
            << " updated=" << stats.updatedCount
            << " wheels=" << stats.wheelCount
            << " grounded=" << stats.groundedWheelCount
            << " sliding=" << stats.slidingWheelCount
            << " raycasts=" << stats.raycastCount
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.warnings.size();
        return out.str();
    }

    std::string BuildVehicleProbeSummary()
    {
        VehicleProbeResult probe = BuildVehicleProbe();
        return probe.summary;
    }

    VehicleProbeResult BuildVehicleProbe()
    {
        VehicleProbeResult probe{};

        PhysicsScene scene{};
        scene.config.gravity = {0.0f, -9.80665f, 0.0f};
        scene.config.broadphaseGridCellSize = 2.0f;

        PhysicsBody groundBody = MakeStaticBody(1, {0.0f, -0.08f, 0.0f});
        PhysicsCollider ground = MakeBoxCollider(1, {12.0f, 0.08f, 24.0f});
        ground.material = MakePhysicsMaterial(0.95f, 0.82f, 0.02f, 2200.0f);
        ground.filter.layerMask = PhysicsLayer_Static;
        ground.filter.collidesWithMask = PhysicsLayer_All;
        scene.bodies.push_back(groundBody);
        scene.colliders.push_back(ground);

        PhysicsBody rampBody = MakeStaticBody(2, {2.5f, 0.20f, 3.0f});
        rampBody.orientation = QuatFromAxisAngle({1.0f, 0.0f, 0.0f}, -8.0f * DegToRad32);
        PhysicsCollider ramp = MakeBoxCollider(2, {1.4f, 0.10f, 1.8f});
        ramp.material = MakePhysicsMaterial(0.85f, 0.76f, 0.03f, 2400.0f);
        ramp.filter.layerMask = PhysicsLayer_Static;
        ramp.filter.collidesWithMask = PhysicsLayer_All;
        scene.bodies.push_back(rampBody);
        scene.colliders.push_back(ramp);

        VehicleConfig config = MakeDefaultVehicleConfig();
        config.driveMode = VehicleDriveMode::AllWheelDrive;
        config.maxEngineForceNewton = 9800.0f;
        config.maxBrakeForceNewton = 14000.0f;

        VehicleBodyState before = MakeVehicleBodyState({0.0f, 0.54f, 0.0f}, {4.0f, 0.0f, 7.0f});
        VehicleControlInput input{};
        input.throttle = 0.75f;
        input.steering = 0.28f;
        input.brake = 0.0f;

        VehicleStepResult result = StepVehicle(scene, config, before, input, 1.0f / 60.0f);

        std::vector<VehicleComponent> components;
        VehicleComponent component{};
        component.config = config;
        component.state = before;
        component.input = input;
        components.push_back(component);
        VehicleSystemStats stats = FixedUpdateVehicles(scene, components, 1.0f / 60.0f);

        probe.scene = scene;
        probe.config = config;
        probe.before = before;
        probe.result = result;
        probe.stats = stats;
        probe.ok = result.finite && stats.finite && result.groundedWheelCount >= 3 && result.raycastCount == config.wheels.size() && result.speedMetersPerSecond > 0.1f;

        std::ostringstream summary;
        summary << "[ " << (probe.ok ? "ok" : "failed") << " ] vehicle physics foundation"
            << " drive=" << ToString(config.driveMode)
            << " wheels=" << config.wheels.size()
            << " grounded=" << result.groundedWheelCount
            << " sliding=" << result.slidingWheelCount
            << " raycasts=" << result.raycastCount
            << " speed=" << std::fixed << std::setprecision(2) << result.speedMetersPerSecond
            << " forward=" << result.forwardSpeedMetersPerSecond
            << " forceY=" << result.totalForceNewton.y
            << " system_updates=" << stats.updatedCount
            << " finite=" << (result.finite ? "true" : "false")
            << " warnings=" << result.warnings.size();
        probe.summary = summary.str();
        return probe;
    }
}
