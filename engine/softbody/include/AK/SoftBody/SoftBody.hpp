#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class SoftBodyTopology : u32
    {
        ClothGrid = 0,
        Rope = 1,
        ParticleCloud = 2
    };

    enum class SoftBodyConstraintKind : u32
    {
        Distance = 0,
        Pin = 1,
        Bending = 2
    };

    enum class SoftBodyCollisionMode : u32
    {
        None = 0,
        PhysicsScene = 1
    };

    struct SoftBodyParticle
    {
        Vec3 position{};
        Vec3 previousPosition{};
        Vec3 velocity{};
        Vec3 accumulatedForce{};
        float inverseMass = 1.0f;
        float radius = 0.035f;
        bool pinned = false;
        bool enabled = true;
    };

    struct SoftBodyDistanceConstraint
    {
        u32 particleA = 0;
        u32 particleB = 0;
        float restLength = 1.0f;
        float compliance = 0.0f;
        float damping = 0.0f;
        bool enabled = true;
    };

    struct SoftBodyPinConstraint
    {
        u32 particle = 0;
        Vec3 target{};
        float compliance = 0.0f;
        bool enabled = true;
    };

    struct SoftBodyConstraint
    {
        SoftBodyConstraintKind kind = SoftBodyConstraintKind::Distance;
        SoftBodyDistanceConstraint distance{};
        SoftBodyPinConstraint pin{};
    };

    struct SoftBodyConfig
    {
        Vec3 gravity{0.0f, -9.80665f, 0.0f};
        float fixedDeltaSeconds = 1.0f / 60.0f;
        float damping = 0.012f;
        float velocityLimit = 120.0f;
        float collisionFriction = 0.35f;
        float collisionRestitution = 0.02f;
        float constraintSlop = 0.0005f;
        u32 solverIterations = 8;
        u32 substeps = 2;
        bool enableGravity = true;
        bool enableConstraints = true;
        bool enableSceneCollision = true;
    };

    struct SoftBodyClothDesc
    {
        u32 columns = 8;
        u32 rows = 6;
        float spacing = 0.25f;
        float particleMassKilograms = 0.08f;
        float particleRadius = 0.035f;
        float stretchCompliance = 1.0e-5f;
        float bendCompliance = 6.0e-4f;
        Vec3 origin{-0.875f, 2.0f, 0.0f};
        Vec3 right{1.0f, 0.0f, 0.0f};
        Vec3 down{0.0f, -1.0f, 0.0f};
        bool pinTopRow = true;
        bool addBending = true;
    };

    struct SoftBodyRopeDesc
    {
        u32 particleCount = 12;
        float spacing = 0.2f;
        float particleMassKilograms = 0.05f;
        float particleRadius = 0.03f;
        float stretchCompliance = 5.0e-6f;
        Vec3 origin{0.0f, 2.0f, 0.0f};
        Vec3 direction{1.0f, 0.0f, 0.0f};
        bool pinFirst = true;
        bool pinLast = false;
    };

    struct SoftBodyInstance
    {
        SoftBodyTopology topology = SoftBodyTopology::ParticleCloud;
        std::vector<SoftBodyParticle> particles;
        std::vector<SoftBodyConstraint> constraints;
        AABB3 bounds = MakeEmptyAABB3();
        bool enabled = true;
        bool sleeping = false;
        u64 revision = 0;
    };

    struct SoftBodyStepStats
    {
        std::size_t particleCount = 0;
        std::size_t dynamicParticleCount = 0;
        std::size_t pinnedParticleCount = 0;
        std::size_t constraintCount = 0;
        std::size_t distanceConstraintCount = 0;
        std::size_t pinConstraintCount = 0;
        std::size_t bendingConstraintCount = 0;
        std::size_t solvedConstraintCount = 0;
        std::size_t collisionQueryCount = 0;
        std::size_t collisionHitCount = 0;
        std::size_t clampedVelocityCount = 0;
        u32 substeps = 0;
        u32 solverIterations = 0;
        float simulatedSeconds = 0.0f;
        float maxConstraintError = 0.0f;
        AABB3 bounds = MakeEmptyAABB3();
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct SoftBodyComponent
    {
        SoftBodyInstance body{};
        SoftBodyConfig config{};
        bool enabled = true;
    };

    struct SoftBodySystemStats
    {
        std::size_t bodyCount = 0;
        std::size_t updatedBodyCount = 0;
        std::size_t particleCount = 0;
        std::size_t collisionHitCount = 0;
        std::size_t solvedConstraintCount = 0;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct SoftBodyProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        SoftBodyInstance cloth{};
        SoftBodyInstance rope{};
        SoftBodyStepStats clothStats{};
        SoftBodyStepStats ropeStats{};
        SoftBodySystemStats systemStats{};
    };

    const char* ToString(SoftBodyTopology topology);
    const char* ToString(SoftBodyConstraintKind kind);
    const char* ToString(SoftBodyCollisionMode mode);

    SoftBodyConfig SanitizeSoftBodyConfig(SoftBodyConfig config);
    SoftBodyClothDesc SanitizeSoftBodyClothDesc(SoftBodyClothDesc desc);
    SoftBodyRopeDesc SanitizeSoftBodyRopeDesc(SoftBodyRopeDesc desc);

    SoftBodyParticle MakeSoftBodyParticle(Vec3 position, float massKilograms, float radius, bool pinned = false);
    SoftBodyConstraint MakeSoftBodyDistanceConstraint(u32 particleA, u32 particleB, float restLength, float compliance, SoftBodyConstraintKind kind = SoftBodyConstraintKind::Distance);
    SoftBodyConstraint MakeSoftBodyPinConstraint(u32 particle, Vec3 target, float compliance = 0.0f);
    SoftBodyInstance MakeClothGridSoftBody(const SoftBodyClothDesc& desc);
    SoftBodyInstance MakeRopeSoftBody(const SoftBodyRopeDesc& desc);

    AABB3 ComputeSoftBodyBounds(const SoftBodyInstance& body);
    bool IsFinite(const SoftBodyParticle& particle);
    bool IsFinite(const SoftBodyInstance& body);

    SoftBodyStepStats StepSoftBody(SoftBodyInstance& body, const SoftBodyConfig& config, float deltaSeconds, const PhysicsScene* scene = nullptr);
    SoftBodySystemStats FixedUpdateSoftBodies(std::vector<SoftBodyComponent>& components, float deltaSeconds, const PhysicsScene* scene = nullptr);

    std::string ToDebugString(const SoftBodyConfig& config);
    std::string ToDebugString(const SoftBodyStepStats& stats);
    std::string ToDebugString(const SoftBodySystemStats& stats);
    std::string ToDebugString(const SoftBodyInstance& body);
    std::string BuildSoftBodyProbeSummary();
    SoftBodyProbeResult BuildSoftBodyProbe();
}
