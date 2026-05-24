#include <AK/Character/CharacterController.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr float MinimumCharacterRadius = 0.05f;
        constexpr float MinimumCharacterHeight = 0.25f;
        constexpr float MinimumMoveDistance = 1.0e-5f;
        constexpr float MinimumUpLength = 1.0e-6f;
        constexpr float MaximumGroundAngleDegrees = 89.0f;

        Vec3 ProjectOnPlane(Vec3 value, Vec3 normal)
        {
            return Subtract(value, Multiply(normal, Dot(value, normal)));
        }

        float ComponentAlong(Vec3 value, Vec3 axis)
        {
            return Dot(value, axis);
        }

        Vec3 RemoveComponentAlong(Vec3 value, Vec3 axis)
        {
            return Subtract(value, Multiply(axis, ComponentAlong(value, axis)));
        }

        Vec3 ClampMagnitude(Vec3 value, float maxLength)
        {
            const float length = Length(value);
            const float safeMaxLength = std::max(0.0f, maxLength);
            if (length <= safeMaxLength || length <= FloatEpsilon)
            {
                return value;
            }
            return Multiply(value, safeMaxLength / length);
        }

        Vec3 MoveTowards(Vec3 current, Vec3 target, float maxDelta)
        {
            const Vec3 delta = Subtract(target, current);
            const float distance = Length(delta);
            if (distance <= maxDelta || distance <= FloatEpsilon)
            {
                return target;
            }
            return Add(current, Multiply(delta, maxDelta / distance));
        }

        float SlopeAngleDegrees(Vec3 normal, Vec3 up)
        {
            const float cosine = std::clamp(Dot(Normalize(normal, up), up), -1.0f, 1.0f);
            return std::acos(cosine) * RadToDeg32;
        }

        bool IsWalkableSlope(Vec3 normal, const CharacterControllerConfig& config)
        {
            const float maxSlopeCosine = std::cos(std::clamp(config.maxSlopeAngleDegrees, 0.0f, MaximumGroundAngleDegrees) * DegToRad32);
            return Dot(Normalize(normal, config.up), config.up) >= maxSlopeCosine;
        }

        Vec3 CapsuleBottomSphereCenter(const CharacterControllerConfig& config, Vec3 controllerPosition)
        {
            const float halfHeight = std::max(config.height * 0.5f, config.radius);
            const float centerOffset = std::max(0.0f, halfHeight - config.radius);
            return Subtract(controllerPosition, Multiply(config.up, centerOffset));
        }

        Vec3 ControllerCenterFromBottomSphereCenter(const CharacterControllerConfig& config, Vec3 bottomSphereCenter)
        {
            const float halfHeight = std::max(config.height * 0.5f, config.radius);
            const float centerOffset = std::max(0.0f, halfHeight - config.radius);
            return Add(bottomSphereCenter, Multiply(config.up, centerOffset + config.skinWidth));
        }

        float SafeSweepRadius(const CharacterControllerConfig& config)
        {
            return std::max(MinimumCharacterRadius, config.radius - std::max(0.0f, config.skinWidth));
        }

        void AppendWarnings(std::vector<std::string>& dst, const std::vector<std::string>& src)
        {
            dst.insert(dst.end(), src.begin(), src.end());
        }

        CharacterControllerResult MakeInvalidResult(CharacterControllerState state, const char* warning)
        {
            CharacterControllerResult result{};
            result.state = state;
            result.finite = false;
            result.warnings.push_back(warning);
            return result;
        }

        CharacterControllerResult TryCharacterStep(
            const PhysicsScene& scene,
            const CharacterControllerConfig& config,
            const CharacterControllerState& state,
            Vec3 horizontalMove)
        {
            CharacterControllerResult result{};
            result.state = state;

            const float horizontalDistance = Length(horizontalMove);
            if (config.stepHeight <= 0.0f || horizontalDistance <= MinimumMoveDistance)
            {
                return result;
            }

            const Vec3 upOffset = Multiply(config.up, config.stepHeight + config.skinWidth);
            const Vec3 raisedStart = Add(state.position, upOffset);
            const Vec3 raisedEnd = Add(raisedStart, horizontalMove);
            const float radius = SafeSweepRadius(config);

            ++result.sweepCount;
            const PhysicsSweepHit forward = SweepSpherePhysicsScene(scene, raisedStart, raisedEnd, radius, config.queryFlags);
            if (forward.hit)
            {
                ++result.hitCount;
                return result;
            }

            const Vec3 downEnd = Subtract(raisedEnd, Multiply(config.up, config.stepHeight + config.groundSnapDistance + config.skinWidth));
            ++result.sweepCount;
            const PhysicsSweepHit down = SweepSpherePhysicsScene(scene, raisedEnd, downEnd, radius, config.queryFlags);
            if (!down.hit)
            {
                return result;
            }

            const float angle = SlopeAngleDegrees(down.normal, config.up);
            const bool walkable = IsWalkableSlope(down.normal, config);
            ++result.hitCount;
            if (!walkable)
            {
                return result;
            }

            result.state.position = ControllerCenterFromBottomSphereCenter(config, down.point);
            result.state.grounded = true;
            result.state.onSteepSlope = false;
            result.state.groundState = CharacterGroundState::Grounded;
            result.state.groundNormal = Normalize(down.normal, config.up);
            result.ground.hit = true;
            result.ground.walkable = true;
            result.ground.sweep = down;
            result.ground.normal = result.state.groundNormal;
            result.ground.slopeAngleDegrees = angle;
            result.ground.distance = down.distance;
            result.collisionFlags |= CharacterCollision_Below | CharacterCollision_Step;
            result.stepped = true;
            result.finite = IsFinite(result.state.position) && IsFinite(result.state.velocity);
            return result;
        }
    }

    const char* ToString(CharacterGroundState state)
    {
        switch (state)
        {
        case CharacterGroundState::Grounded:
            return "Grounded";
        case CharacterGroundState::Sliding:
            return "Sliding";
        case CharacterGroundState::Airborne:
        default:
            return "Airborne";
        }
    }

    bool HasCharacterCollisionFlag(u32 flags, CharacterCollisionFlags flag)
    {
        return (flags & static_cast<u32>(flag)) != 0u;
    }

    CharacterControllerConfig SanitizeCharacterControllerConfig(CharacterControllerConfig config)
    {
        config.radius = std::max(MinimumCharacterRadius, config.radius);
        config.height = std::max(std::max(MinimumCharacterHeight, config.radius * 2.0f), config.height);
        config.skinWidth = std::clamp(config.skinWidth, 0.0f, config.radius * 0.45f);
        config.stepHeight = std::clamp(config.stepHeight, 0.0f, config.height * 0.5f);
        config.groundSnapDistance = std::clamp(config.groundSnapDistance, 0.0f, config.height);
        config.maxSlopeAngleDegrees = std::clamp(config.maxSlopeAngleDegrees, 0.0f, MaximumGroundAngleDegrees);
        config.maxSpeedMetersPerSecond = std::max(0.0f, config.maxSpeedMetersPerSecond);
        config.accelerationMetersPerSecondSquared = std::max(0.0f, config.accelerationMetersPerSecondSquared);
        config.airAccelerationMetersPerSecondSquared = std::max(0.0f, config.airAccelerationMetersPerSecondSquared);
        config.gravityScale = std::max(0.0f, config.gravityScale);
        config.maxSlideIterations = std::clamp(config.maxSlideIterations, 1u, 8u);
        config.up = Normalize(config.up, {0.0f, 1.0f, 0.0f});
        if (LengthSquared(config.up) <= MinimumUpLength)
        {
            config.up = {0.0f, 1.0f, 0.0f};
        }
        return config;
    }

    CharacterControllerState MakeCharacterControllerState(Vec3 position, Vec3 velocity)
    {
        CharacterControllerState state{};
        state.position = position;
        state.velocity = velocity;
        state.groundNormal = {0.0f, 1.0f, 0.0f};
        return state;
    }

    CharacterGroundHit ProbeCharacterGround(const PhysicsScene& scene, const CharacterControllerConfig& rawConfig, const CharacterControllerState& state)
    {
        const CharacterControllerConfig config = SanitizeCharacterControllerConfig(rawConfig);
        CharacterGroundHit ground{};

        const Vec3 bottom = CapsuleBottomSphereCenter(config, state.position);
        const Vec3 end = Subtract(bottom, Multiply(config.up, config.groundSnapDistance + config.skinWidth));
        const PhysicsSweepHit sweep = SweepSpherePhysicsScene(scene, bottom, end, SafeSweepRadius(config), config.queryFlags);
        if (!sweep.hit)
        {
            return ground;
        }

        ground.hit = true;
        ground.sweep = sweep;
        ground.normal = Normalize(sweep.normal, config.up);
        ground.slopeAngleDegrees = SlopeAngleDegrees(ground.normal, config.up);
        ground.walkable = ground.slopeAngleDegrees <= config.maxSlopeAngleDegrees;
        ground.distance = sweep.distance;
        return ground;
    }

    CharacterControllerResult MoveCharacterController(
        const PhysicsScene& scene,
        const CharacterControllerConfig& rawConfig,
        const CharacterControllerState& rawState,
        const CharacterControllerInput& input,
        float deltaSeconds)
    {
        CharacterControllerConfig config = SanitizeCharacterControllerConfig(rawConfig);
        CharacterControllerState state = rawState;
        state.groundNormal = Normalize(state.groundNormal, config.up);

        if (!IsFinite(deltaSeconds) || deltaSeconds <= 0.0f)
        {
            return MakeInvalidResult(state, "character_delta_seconds_not_positive");
        }
        if (!IsFinite(state.position) || !IsFinite(state.velocity) || !IsFinite(input.desiredVelocity) || !IsFinite(input.gravity))
        {
            return MakeInvalidResult(state, "character_non_finite_input");
        }

        CharacterControllerResult result{};
        result.state = state;

        const CharacterGroundHit initialGround = ProbeCharacterGround(scene, config, state);
        result.ground = initialGround;
        result.sweepCount += 1;
        if (initialGround.hit)
        {
            ++result.hitCount;
        }

        state.grounded = initialGround.hit && initialGround.walkable;
        state.onSteepSlope = initialGround.hit && !initialGround.walkable;
        state.groundState = state.grounded ? CharacterGroundState::Grounded : (state.onSteepSlope ? CharacterGroundState::Sliding : CharacterGroundState::Airborne);
        state.groundNormal = initialGround.hit ? initialGround.normal : config.up;

        Vec3 horizontalVelocity = ClampMagnitude(ProjectOnPlane(input.desiredVelocity, config.up), config.maxSpeedMetersPerSecond);
        const float acceleration = state.grounded ? config.accelerationMetersPerSecondSquared : config.airAccelerationMetersPerSecondSquared;
        Vec3 currentHorizontal = RemoveComponentAlong(state.velocity, config.up);
        currentHorizontal = MoveTowards(currentHorizontal, horizontalVelocity, acceleration * deltaSeconds);

        float verticalSpeed = ComponentAlong(state.velocity, config.up);
        if (state.grounded && verticalSpeed < 0.0f)
        {
            verticalSpeed = 0.0f;
        }
        if (input.jumpPressed && state.grounded && input.jumpSpeed > 0.0f)
        {
            verticalSpeed = input.jumpSpeed;
            state.grounded = false;
            state.groundState = CharacterGroundState::Airborne;
        }
        else
        {
            verticalSpeed += Dot(input.gravity, config.up) * config.gravityScale * deltaSeconds;
        }

        state.velocity = Add(currentHorizontal, Multiply(config.up, verticalSpeed));
        Vec3 remainingMove = Multiply(state.velocity, deltaSeconds);
        Vec3 position = state.position;
        bool hitSide = false;

        for (u32 iteration = 0; iteration < config.maxSlideIterations; ++iteration)
        {
            const float distance = Length(remainingMove);
            if (distance <= MinimumMoveDistance)
            {
                break;
            }

            const Vec3 start = position;
            const Vec3 end = Add(position, remainingMove);
            ++result.sweepCount;
            const PhysicsSweepHit hit = SweepSpherePhysicsScene(scene, start, end, SafeSweepRadius(config), config.queryFlags);
            if (!hit.hit)
            {
                position = end;
                break;
            }

            ++result.hitCount;
            const Vec3 normal = Normalize(hit.normal, config.up);
            const float upDot = Dot(normal, config.up);
            const float safeTravel = std::max(0.0f, hit.distance - config.skinWidth);
            const Vec3 direction = Normalize(remainingMove, {0.0f, 0.0f, 1.0f});
            position = Add(start, Multiply(direction, safeTravel));

            if (upDot > 0.35f)
            {
                result.collisionFlags |= CharacterCollision_Below;
                state.grounded = IsWalkableSlope(normal, config);
                state.onSteepSlope = !state.grounded;
                state.groundNormal = normal;
            }
            else if (upDot < -0.35f)
            {
                result.collisionFlags |= CharacterCollision_Above;
                verticalSpeed = std::min(0.0f, verticalSpeed);
            }
            else
            {
                result.collisionFlags |= CharacterCollision_Sides;
                hitSide = true;
            }

            Vec3 remaining = Multiply(direction, std::max(0.0f, distance - hit.distance));
            remaining = ProjectOnPlane(remaining, normal);
            state.velocity = ProjectOnPlane(state.velocity, normal);
            remainingMove = remaining;
            ++result.slideCount;
        }

        if (hitSide && config.stepHeight > 0.0f)
        {
            const Vec3 requestedHorizontal = Multiply(horizontalVelocity, deltaSeconds);
            CharacterControllerState stepState = state;
            stepState.position = rawState.position;
            CharacterControllerResult stepResult = TryCharacterStep(scene, config, stepState, requestedHorizontal);
            result.sweepCount += stepResult.sweepCount;
            result.hitCount += stepResult.hitCount;
            if (stepResult.stepped)
            {
                position = stepResult.state.position;
                state.grounded = true;
                state.onSteepSlope = false;
                state.groundNormal = stepResult.state.groundNormal;
                state.groundState = CharacterGroundState::Grounded;
                result.ground = stepResult.ground;
                result.stepped = true;
                result.collisionFlags |= CharacterCollision_Step | CharacterCollision_Below;
                ++result.slideCount;
            }
        }

        state.position = position;
        CharacterGroundHit finalGround = ProbeCharacterGround(scene, config, state);
        ++result.sweepCount;
        if (finalGround.hit)
        {
            ++result.hitCount;
            result.ground = finalGround;
            if (finalGround.walkable && verticalSpeed <= 0.0f)
            {
                state.position = ControllerCenterFromBottomSphereCenter(config, finalGround.sweep.point);
                state.grounded = true;
                state.onSteepSlope = false;
                state.groundNormal = finalGround.normal;
                state.groundState = CharacterGroundState::Grounded;
                state.velocity = RemoveComponentAlong(state.velocity, config.up);
                result.snappedToGround = true;
                result.collisionFlags |= CharacterCollision_Below;
            }
            else if (!finalGround.walkable)
            {
                state.grounded = false;
                state.onSteepSlope = true;
                state.groundNormal = finalGround.normal;
                state.groundState = CharacterGroundState::Sliding;
            }
        }
        else
        {
            state.grounded = false;
            state.onSteepSlope = false;
            state.groundState = CharacterGroundState::Airborne;
            state.groundNormal = config.up;
        }

        result.state = state;
        result.finite = IsFinite(result.state.position) && IsFinite(result.state.velocity) && IsFinite(result.state.groundNormal);
        if (!result.finite)
        {
            result.warnings.push_back("character_result_non_finite");
        }
        return result;
    }

    CharacterControllerStats FixedUpdateCharacterControllers(
        const PhysicsScene& scene,
        std::vector<CharacterControllerComponent>& controllers,
        const std::vector<CharacterControllerInput>& inputs,
        float deltaSeconds)
    {
        CharacterControllerStats stats{};
        stats.controllerCount = controllers.size();
        stats.finite = true;

        if (!IsFinite(deltaSeconds) || deltaSeconds <= 0.0f)
        {
            stats.finite = false;
            stats.warnings.push_back("character_system_delta_seconds_not_positive");
            return stats;
        }

        for (std::size_t index = 0; index < controllers.size(); ++index)
        {
            CharacterControllerComponent& controller = controllers[index];
            if (!controller.enabled)
            {
                continue;
            }

            const CharacterControllerInput input = index < inputs.size() ? inputs[index] : CharacterControllerInput{};
            const CharacterControllerResult result = MoveCharacterController(scene, controller.config, controller.state, input, deltaSeconds);
            controller.config = SanitizeCharacterControllerConfig(controller.config);
            controller.state = result.state;
            ++stats.updatedCount;
            stats.sweepCount += result.sweepCount;
            stats.hitCount += result.hitCount;
            if (result.stepped)
            {
                ++stats.stepCount;
            }
            if (result.state.grounded)
            {
                ++stats.groundedCount;
            }
            if (result.state.groundState == CharacterGroundState::Sliding)
            {
                ++stats.slidingCount;
            }
            stats.finite = stats.finite && result.finite;
            AppendWarnings(stats.warnings, result.warnings);
        }

        return stats;
    }

    std::string ToDebugString(const CharacterControllerConfig& config, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision)
               << "character_config radius=" << config.radius
               << " height=" << config.height
               << " skin=" << config.skinWidth
               << " step=" << config.stepHeight
               << " snap=" << config.groundSnapDistance
               << " slope=" << config.maxSlopeAngleDegrees
               << " max_speed=" << config.maxSpeedMetersPerSecond;
        return stream.str();
    }

    std::string ToDebugString(const CharacterGroundHit& hit, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision)
               << "ground hit=" << (hit.hit ? "true" : "false")
               << " walkable=" << (hit.walkable ? "true" : "false")
               << " slope=" << hit.slopeAngleDegrees
               << " distance=" << hit.distance
               << " normal=" << ToDebugString(hit.normal, precision);
        return stream.str();
    }

    std::string ToDebugString(const CharacterControllerResult& result, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision)
               << "character_result state=" << ToString(result.state.groundState)
               << " grounded=" << (result.state.grounded ? "true" : "false")
               << " stepped=" << (result.stepped ? "true" : "false")
               << " snapped=" << (result.snappedToGround ? "true" : "false")
               << " sweeps=" << result.sweepCount
               << " hits=" << result.hitCount
               << " slides=" << result.slideCount
               << " flags=" << result.collisionFlags
               << " pos=" << ToDebugString(result.state.position, precision)
               << " vel=" << ToDebugString(result.state.velocity, precision)
               << " finite=" << (result.finite ? "true" : "false");
        if (!result.warnings.empty())
        {
            stream << " warnings=" << result.warnings.size();
        }
        return stream.str();
    }

    std::string ToDebugString(const CharacterControllerStats& stats)
    {
        std::ostringstream stream;
        stream << "character_system controllers=" << stats.controllerCount
               << " updated=" << stats.updatedCount
               << " grounded=" << stats.groundedCount
               << " sliding=" << stats.slidingCount
               << " sweeps=" << stats.sweepCount
               << " hits=" << stats.hitCount
               << " steps=" << stats.stepCount
               << " finite=" << (stats.finite ? "true" : "false");
        if (!stats.warnings.empty())
        {
            stream << " warnings=" << stats.warnings.size();
        }
        return stream.str();
    }

    CharacterProbeResult BuildCharacterProbe()
    {
        CharacterProbeResult probe{};
        probe.scene.config.enableSpatialBroadphase = true;
        probe.scene.config.broadphaseGridCellSize = 1.0f;

        PhysicsBody floorBody = MakeStaticBody(1, {0.0f, -0.10f, 0.0f});
        PhysicsCollider floor = MakeBoxCollider(1, {8.0f, 0.10f, 8.0f});
        floor.filter.layerMask = PhysicsLayer_Static;
        floor.filter.collidesWithMask = PhysicsLayer_All;
        floor.material = MakePhysicsMaterial(0.9f, 0.7f, 0.0f, 2400.0f);
        probe.scene.bodies.push_back(floorBody);
        probe.scene.colliders.push_back(floor);

        PhysicsBody wallBody = MakeStaticBody(2, {1.75f, 0.55f, 0.0f});
        PhysicsCollider wall = MakeBoxCollider(2, {0.20f, 0.55f, 1.25f});
        wall.filter.layerMask = PhysicsLayer_Static;
        wall.filter.collidesWithMask = PhysicsLayer_All;
        wall.material = MakePhysicsMaterial(0.8f, 0.6f, 0.0f, 2200.0f);
        probe.scene.bodies.push_back(wallBody);
        probe.scene.colliders.push_back(wall);

        PhysicsBody stepBody = MakeStaticBody(3, {0.80f, 0.12f, 0.0f});
        PhysicsCollider step = MakeBoxCollider(3, {0.28f, 0.12f, 0.60f});
        step.filter.layerMask = PhysicsLayer_Static;
        step.filter.collidesWithMask = PhysicsLayer_All;
        step.material = MakePhysicsMaterial(0.85f, 0.65f, 0.0f, 2200.0f);
        probe.scene.bodies.push_back(stepBody);
        probe.scene.colliders.push_back(step);

        PhysicsBody triggerBody = MakeStaticBody(4, {0.25f, 0.75f, 0.0f});
        PhysicsCollider trigger = MakeBoxCollider(4, {0.20f, 0.40f, 0.40f});
        trigger.trigger = true;
        trigger.filter.layerMask = PhysicsLayer_Trigger;
        trigger.filter.collidesWithMask = PhysicsLayer_Character;
        probe.scene.bodies.push_back(triggerBody);
        probe.scene.colliders.push_back(trigger);

        probe.config = SanitizeCharacterControllerConfig({});
        probe.config.queryFlags = PhysicsQueryFlags::Default;
        probe.before = MakeCharacterControllerState({0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f});

        CharacterControllerInput input{};
        input.desiredVelocity = {5.5f, 0.0f, 0.0f};
        input.gravity = {0.0f, -9.80665f, 0.0f};
        probe.result = MoveCharacterController(probe.scene, probe.config, probe.before, input, 0.25f);

        std::vector<CharacterControllerComponent> controllers;
        CharacterControllerComponent component{};
        component.config = probe.config;
        component.state = probe.before;
        controllers.push_back(component);
        std::vector<CharacterControllerInput> inputs{input};
        probe.stats = FixedUpdateCharacterControllers(probe.scene, controllers, inputs, 0.25f);

        probe.ok = probe.result.finite
            && probe.stats.finite
            && probe.result.hitCount > 0
            && probe.result.sweepCount > 0
            && probe.stats.updatedCount == 1
            && probe.result.state.groundState != CharacterGroundState::Airborne;

        std::ostringstream stream;
        stream << "[ " << (probe.ok ? "ok" : "fail") << " ] character controller"
               << " state=" << ToString(probe.result.state.groundState)
               << " grounded=" << (probe.result.state.grounded ? "true" : "false")
               << " stepped=" << (probe.result.stepped ? "true" : "false")
               << " snapped=" << (probe.result.snappedToGround ? "true" : "false")
               << " sweeps=" << probe.result.sweepCount
               << " hits=" << probe.result.hitCount
               << " slides=" << probe.result.slideCount
               << " system_updates=" << probe.stats.updatedCount
               << " pos=" << ToDebugString(probe.result.state.position, 2)
               << " finite=" << (probe.result.finite ? "true" : "false");
        probe.summary = stream.str();
        return probe;
    }

    std::string BuildCharacterProbeSummary()
    {
        return BuildCharacterProbe().summary;
    }
}
