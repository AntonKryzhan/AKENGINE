#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class CharacterGroundState : u32
    {
        Airborne = 0,
        Grounded = 1,
        Sliding = 2
    };

    enum CharacterCollisionFlags : u32
    {
        CharacterCollision_None = 0u,
        CharacterCollision_Sides = 1u << 0u,
        CharacterCollision_Above = 1u << 1u,
        CharacterCollision_Below = 1u << 2u,
        CharacterCollision_Step = 1u << 3u
    };

    struct CharacterControllerConfig
    {
        float radius = 0.35f;
        float height = 1.80f;
        float skinWidth = 0.04f;
        float stepHeight = 0.35f;
        float groundSnapDistance = 0.25f;
        float maxSlopeAngleDegrees = 50.0f;
        float maxSpeedMetersPerSecond = 8.0f;
        float accelerationMetersPerSecondSquared = 35.0f;
        float airAccelerationMetersPerSecondSquared = 10.0f;
        float gravityScale = 1.0f;
        u32 maxSlideIterations = 4;
        Vec3 up{0.0f, 1.0f, 0.0f};
        PhysicsQueryFlags queryFlags = PhysicsQueryFlags::Default;
    };

    struct CharacterControllerState
    {
        Vec3 position{};
        Vec3 velocity{};
        Vec3 groundNormal{0.0f, 1.0f, 0.0f};
        CharacterGroundState groundState = CharacterGroundState::Airborne;
        bool grounded = false;
        bool onSteepSlope = false;
    };

    struct CharacterControllerInput
    {
        Vec3 desiredVelocity{};
        Vec3 gravity{0.0f, -9.80665f, 0.0f};
        float jumpSpeed = 0.0f;
        bool jumpPressed = false;
    };

    struct CharacterGroundHit
    {
        bool hit = false;
        bool walkable = false;
        PhysicsSweepHit sweep{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float slopeAngleDegrees = 0.0f;
        float distance = 0.0f;
    };

    struct CharacterControllerResult
    {
        CharacterControllerState state{};
        CharacterGroundHit ground{};
        u32 collisionFlags = CharacterCollision_None;
        u32 sweepCount = 0;
        u32 hitCount = 0;
        u32 slideCount = 0;
        bool stepped = false;
        bool snappedToGround = false;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct CharacterControllerComponent
    {
        CharacterControllerConfig config{};
        CharacterControllerState state{};
        bool enabled = true;
    };

    struct CharacterControllerStats
    {
        std::size_t controllerCount = 0;
        std::size_t updatedCount = 0;
        std::size_t groundedCount = 0;
        std::size_t slidingCount = 0;
        std::size_t sweepCount = 0;
        std::size_t hitCount = 0;
        std::size_t stepCount = 0;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct CharacterProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        CharacterControllerConfig config{};
        CharacterControllerState before{};
        CharacterControllerResult result{};
        CharacterControllerStats stats{};
    };

    const char* ToString(CharacterGroundState state);
    bool HasCharacterCollisionFlag(u32 flags, CharacterCollisionFlags flag);

    CharacterControllerConfig SanitizeCharacterControllerConfig(CharacterControllerConfig config);
    CharacterControllerState MakeCharacterControllerState(Vec3 position = {}, Vec3 velocity = {});
    CharacterGroundHit ProbeCharacterGround(const PhysicsScene& scene, const CharacterControllerConfig& config, const CharacterControllerState& state);
    CharacterControllerResult MoveCharacterController(const PhysicsScene& scene, const CharacterControllerConfig& config, const CharacterControllerState& state, const CharacterControllerInput& input, float deltaSeconds);
    CharacterControllerStats FixedUpdateCharacterControllers(const PhysicsScene& scene, std::vector<CharacterControllerComponent>& controllers, const std::vector<CharacterControllerInput>& inputs, float deltaSeconds);

    std::string ToDebugString(const CharacterControllerConfig& config, int precision = 2);
    std::string ToDebugString(const CharacterGroundHit& hit, int precision = 3);
    std::string ToDebugString(const CharacterControllerResult& result, int precision = 3);
    std::string ToDebugString(const CharacterControllerStats& stats);
    std::string BuildCharacterProbeSummary();
    CharacterProbeResult BuildCharacterProbe();
}
