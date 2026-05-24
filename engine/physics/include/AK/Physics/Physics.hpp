#pragma once

#include <AK/Core/Types.hpp>
#include <AK/CSG/Boolean.hpp>
#include <AK/Math/Geometry.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class PhysicsBodyKind : u32
    {
        Static = 0,
        Kinematic = 1,
        Dynamic = 2
    };

    enum class PhysicsColliderKind : u32
    {
        Sphere = 0,
        Box = 1,
        ProxyAABB = 2,
        Capsule = 3,
        Heightfield = 4
    };

    enum class PhysicsQueryFlags : u32
    {
        None = 0,
        IncludeTriggers = 1u << 0,
        IncludeStatic = 1u << 1,
        IncludeKinematic = 1u << 2,
        IncludeDynamic = 1u << 3,
        Default = IncludeStatic | IncludeKinematic | IncludeDynamic
    };

    enum class PhysicsConstraintKind : u32
    {
        Distance = 0
    };

    enum PhysicsCollisionLayer : u32
    {
        PhysicsLayer_Default = 1u << 0u,
        PhysicsLayer_Static = 1u << 1u,
        PhysicsLayer_Dynamic = 1u << 2u,
        PhysicsLayer_Character = 1u << 3u,
        PhysicsLayer_Projectile = 1u << 4u,
        PhysicsLayer_Destructible = 1u << 5u,
        PhysicsLayer_Trigger = 1u << 6u,
        PhysicsLayer_QueryOnly = 1u << 7u,
        PhysicsLayer_All = 0xffffffffu
    };

    enum class PhysicsMaterialCombineMode : u32
    {
        Average = 0,
        Minimum = 1,
        Maximum = 2,
        Multiply = 3
    };

    enum class PhysicsEventKind : u32
    {
        ContactStarted = 0,
        ContactStayed = 1,
        ContactEnded = 2,
        TriggerEntered = 3,
        TriggerStayed = 4,
        TriggerExited = 5,
        BodySlept = 6,
        BodyWoke = 7
    };

    enum class PhysicsContactFeature : u32
    {
        None = 0,
        SphereSphere = 1,
        SphereBox = 2,
        BoxBox = 3,
        CapsuleSphere = 4,
        CapsuleCapsule = 5,
        CapsuleBox = 6,
        BoundsFallback = 7
    };

    PhysicsQueryFlags operator|(PhysicsQueryFlags a, PhysicsQueryFlags b);
    bool HasFlag(PhysicsQueryFlags flags, PhysicsQueryFlags flag);

    struct PhysicsMaterialDesc
    {
        float staticFriction = 0.7f;
        float dynamicFriction = 0.55f;
        float restitution = 0.05f;
        float rollingResistance = 0.02f;
        float densityKgPerCubicMeter = 1000.0f;
        float hardness = 1.0f;
        PhysicsMaterialCombineMode frictionCombine = PhysicsMaterialCombineMode::Average;
        PhysicsMaterialCombineMode restitutionCombine = PhysicsMaterialCombineMode::Maximum;
        bool valid = true;
    };

    struct PhysicsCollisionFilter
    {
        u32 layerMask = PhysicsLayer_Default;
        u32 collidesWithMask = PhysicsLayer_All;
        u32 eventMask = PhysicsLayer_All;
        bool queryOnly = false;
    };

    struct PhysicsContactMaterial
    {
        float staticFriction = 0.7f;
        float dynamicFriction = 0.55f;
        float restitution = 0.05f;
        float rollingResistance = 0.02f;
        float hardness = 1.0f;
        bool valid = true;
    };

    struct PhysicsBody
    {
        u32 id = 0;
        PhysicsBodyKind kind = PhysicsBodyKind::Static;
        Vec3 position{};
        Vec3 previousPosition{};
        Quat orientation{};
        Vec3 velocity{};
        Vec3 accumulatedForce{};
        float massKilograms = 1.0f;
        float inverseMass = 0.0f;
        float linearDamping = 0.02f;
        float gravityScale = 1.0f;
        float restitution = 0.0f;
        float friction = 0.5f;
        float sleepTimerSeconds = 0.0f;
        bool enabled = true;
        bool sleeping = false;
        bool canSleep = true;
        bool continuousCollision = false;
    };

    struct PhysicsCollider
    {
        u32 bodyId = 0;
        PhysicsColliderKind kind = PhysicsColliderKind::Box;
        Vec3 localCenter{};
        Quat localRotation{};
        Vec3 halfExtents{0.5f, 0.5f, 0.5f};
        float radius = 0.5f;
        float halfHeight = 0.5f;
        float contactOffset = 0.02f;
        float restOffset = 0.0f;
        AABB3 localBounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        bool enabled = true;
        bool trigger = false;
        PhysicsCollisionFilter filter{};
        PhysicsMaterialDesc material{};
        std::string debugName;
    };

    struct RigidBodyComponent
    {
        u32 bodyId = 0;
        PhysicsBodyKind kind = PhysicsBodyKind::Static;
        float massKilograms = 1.0f;
        float linearDamping = 0.02f;
        float gravityScale = 1.0f;
        bool enabled = true;
        bool canSleep = true;
        bool continuousCollision = false;
    };

    struct ColliderComponent
    {
        u32 bodyId = 0;
        PhysicsColliderKind kind = PhysicsColliderKind::Box;
        Vec3 localCenter{};
        Quat localRotation{};
        Vec3 halfExtents{0.5f, 0.5f, 0.5f};
        float radius = 0.5f;
        float halfHeight = 0.5f;
        float contactOffset = 0.02f;
        float restOffset = 0.0f;
        bool trigger = false;
        bool enabled = true;
        PhysicsCollisionFilter filter{};
        PhysicsMaterialDesc material{};
    };

    struct PhysicsObbShape
    {
        Vec3 center{};
        std::array<Vec3, 3> axes{Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}};
        Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    };

    struct PhysicsCapsuleShape
    {
        Vec3 a{};
        Vec3 b{};
        float radius = 0.5f;
    };

    struct PhysicsBroadphasePair
    {
        std::size_t colliderA = 0;
        std::size_t colliderB = 0;
        AABB3 boundsA{};
        AABB3 boundsB{};
    };

    struct PhysicsDistanceConstraint
    {
        u32 bodyA = 0;
        u32 bodyB = 0;
        Vec3 localAnchorA{};
        Vec3 localAnchorB{};
        float restDistance = 1.0f;
        float minDistance = 0.0f;
        float maxDistance = 1.0f;
        float stiffness = 1.0f;
        float damping = 0.0f;
        bool enabled = true;
    };

    struct PhysicsConstraint
    {
        PhysicsConstraintKind kind = PhysicsConstraintKind::Distance;
        PhysicsDistanceConstraint distance{};
    };

    struct PhysicsContactCacheEntry
    {
        u64 pairKey = 0;
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 point{};
        float penetration = 0.0f;
        u32 age = 0;
        bool touching = false;
    };

    struct PhysicsContact
    {
        std::size_t colliderA = 0;
        std::size_t colliderB = 0;
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 point{};
        float penetration = 0.0f;
        bool trigger = false;
        bool valid = false;
        bool speculative = false;
        u64 cacheKey = 0;
        u32 cacheAge = 0;
        PhysicsContactFeature feature = PhysicsContactFeature::None;
        PhysicsContactMaterial material{};
        float normalImpulse = 0.0f;
        float tangentImpulse = 0.0f;
    };

    struct PhysicsContactManifold
    {
        std::size_t colliderA = 0;
        std::size_t colliderB = 0;
        Vec3 normal{0.0f, 1.0f, 0.0f};
        std::array<Vec3, 4> points{};
        std::array<float, 4> penetrations{};
        u32 pointCount = 0;
        bool trigger = false;
        bool valid = false;
    };

    struct PhysicsRaycastHit
    {
        bool hit = false;
        std::size_t collider = 0;
        u32 bodyId = 0;
        Vec3 point{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float distance = 0.0f;
    };

    struct PhysicsSweepHit
    {
        bool hit = false;
        std::size_t collider = 0;
        u32 bodyId = 0;
        Vec3 point{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float distance = 0.0f;
        float timeOfImpact = 0.0f;
    };

    struct PhysicsEvent
    {
        PhysicsEventKind kind = PhysicsEventKind::ContactStarted;
        u32 bodyA = 0;
        u32 bodyB = 0;
        std::size_t colliderA = 0;
        std::size_t colliderB = 0;
        Vec3 point{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float penetration = 0.0f;
        u32 age = 0;
    };

    struct PhysicsBroadphaseGridStats
    {
        std::size_t colliderCount = 0;
        std::size_t occupiedCellCount = 0;
        std::size_t candidatePairCount = 0;
        std::size_t acceptedPairCount = 0;
        std::size_t duplicatePairCount = 0;
        std::size_t skippedPairCount = 0;
        std::size_t filteredPairCount = 0;
        float cellSize = 1.0f;
        bool usedFallback = false;
    };

    struct PhysicsIsland
    {
        std::vector<u32> bodyIds;
        AABB3 bounds = MakeEmptyAABB3();
        bool sleeping = true;
    };

    struct PhysicsWorldConfig
    {
        Vec3 gravity{0.0f, -9.80665f, 0.0f};
        float fixedDeltaSeconds = 1.0f / 60.0f;
        float maxDeltaSeconds = 0.1f;
        float broadphaseFatMargin = 0.02f;
        float broadphaseGridCellSize = 2.0f;
        float contactSlop = 0.001f;
        float defaultContactOffset = 0.02f;
        float defaultRestOffset = 0.0f;
        float speculativeVelocityMargin = 1.0f;
        float staticLeakageGuardSkin = 0.004f;
        float maxDepenetrationVelocity = 60.0f;
        float maxLinearVelocity = 250.0f;
        float sleepLinearVelocityThreshold = 0.03f;
        float sleepTimeThreshold = 0.5f;
        float ccdMinVelocity = 10.0f;
        float ccdRadiusScale = 1.0f;
        u32 maxSubsteps = 8;
        u32 solverIterations = 4;
        u32 constraintIterations = 4;
        u32 leakageGuardIterations = 2;
        bool enableGravity = true;
        bool enableContactResolution = true;
        bool enableSpatialBroadphase = true;
        bool enableSleeping = true;
        bool enableContinuousCollision = true;
        bool enableConstraints = true;
        bool enableSpeculativeContacts = true;
        bool enableStaticLeakageGuard = true;
    };

    struct PhysicsScene
    {
        PhysicsWorldConfig config{};
        std::vector<PhysicsBody> bodies;
        std::vector<PhysicsCollider> colliders;
        std::vector<PhysicsConstraint> constraints;
        std::vector<PhysicsContactCacheEntry> contactCache;
        std::vector<PhysicsEvent> events;
    };

    struct PhysicsStepStats
    {
        std::size_t bodyCount = 0;
        std::size_t colliderCount = 0;
        std::size_t dynamicBodyCount = 0;
        std::size_t integratedBodyCount = 0;
        std::size_t broadphasePairCount = 0;
        std::size_t contactCount = 0;
        std::size_t manifoldCount = 0;
        std::size_t resolvedContactCount = 0;
        std::size_t persistentContactCount = 0;
        std::size_t proxyColliderCount = 0;
        std::size_t islandCount = 0;
        std::size_t sleepingBodyCount = 0;
        std::size_t wokenBodyCount = 0;
        std::size_t ccdSweepCount = 0;
        std::size_t ccdHitCount = 0;
        std::size_t constraintCount = 0;
        std::size_t solvedConstraintCount = 0;
        std::size_t obbContactCount = 0;
        std::size_t capsuleContactCount = 0;
        std::size_t fallbackContactCount = 0;
        std::size_t manifoldPointCount = 0;
        std::size_t clippedManifoldCount = 0;
        std::size_t triggerContactCount = 0;
        std::size_t contactStartedEventCount = 0;
        std::size_t contactStayedEventCount = 0;
        std::size_t contactEndedEventCount = 0;
        std::size_t triggerEventCount = 0;
        std::size_t materialPairCount = 0;
        std::size_t filteredPairCount = 0;
        std::size_t speculativeContactCount = 0;
        std::size_t leakageGuardSweepCount = 0;
        std::size_t leakageGuardCorrectionCount = 0;
        float maxPenetrationBeforeSolve = 0.0f;
        float maxPenetrationAfterGuard = 0.0f;
        std::size_t raycastQueryCount = 0;
        std::size_t sweepQueryCount = 0;
        PhysicsBroadphaseGridStats broadphaseGrid{};
        float simulatedSeconds = 0.0f;
        u32 substeps = 0;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct PhysicsSystemState
    {
        double accumulatorSeconds = 0.0;
        u64 fixedTick = 0;
        PhysicsStepStats lastStats{};
    };

    struct PhysicsProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        PhysicsStepStats stats{};
        std::vector<PhysicsBroadphasePair> pairs;
        std::vector<PhysicsContact> contacts;
        std::vector<PhysicsContactManifold> manifolds;
        std::vector<PhysicsIsland> islands;
        std::vector<PhysicsEvent> events;
        PhysicsRaycastHit raycast{};
        PhysicsSweepHit sweep{};
    };

    const char* ToString(PhysicsBodyKind kind);
    const char* ToString(PhysicsColliderKind kind);
    const char* ToString(PhysicsConstraintKind kind);
    const char* ToString(PhysicsContactFeature feature);
    const char* ToString(PhysicsMaterialCombineMode mode);
    const char* ToString(PhysicsEventKind kind);

    PhysicsBody MakeStaticBody(u32 id, Vec3 position = {});
    PhysicsBody MakeKinematicBody(u32 id, Vec3 position = {});
    PhysicsBody MakeDynamicBody(u32 id, Vec3 position = {}, float massKilograms = 1.0f);
    PhysicsBody MakePhysicsBody(const RigidBodyComponent& component, Vec3 position = {});

    PhysicsCollider MakeSphereCollider(u32 bodyId, float radius, Vec3 localCenter = {});
    PhysicsCollider MakeBoxCollider(u32 bodyId, Vec3 halfExtents, Vec3 localCenter = {});
    PhysicsCollider MakeCapsuleCollider(u32 bodyId, float radius, float halfHeight, Vec3 localCenter = {});
    PhysicsCollider MakeHeightfieldCollider(u32 bodyId, AABB3 localBounds, Vec3 localCenter = {});
    PhysicsCollider MakeProxyAabbCollider(u32 bodyId, AABB3 localBounds, std::string debugName = {});
    PhysicsCollider MakePhysicsCollider(const ColliderComponent& component);
    PhysicsConstraint MakeDistanceConstraint(u32 bodyA, u32 bodyB, float restDistance, Vec3 localAnchorA = {}, Vec3 localAnchorB = {});
    PhysicsMaterialDesc MakePhysicsMaterial(float staticFriction, float dynamicFriction, float restitution, float densityKgPerCubicMeter = 1000.0f);
    PhysicsMaterialDesc SanitizePhysicsMaterial(PhysicsMaterialDesc material);
    PhysicsContactMaterial CombinePhysicsMaterials(const PhysicsMaterialDesc& a, const PhysicsMaterialDesc& b);
    bool PhysicsFiltersCanCollide(const PhysicsCollisionFilter& a, const PhysicsCollisionFilter& b);
    std::vector<PhysicsCollider> MakeProxyCollidersFromCsg(u32 bodyId, const std::vector<CsgCollisionProxy>& proxies, std::size_t maxProxyCount = 256);

    PhysicsBody* FindBody(PhysicsScene& scene, u32 bodyId);
    const PhysicsBody* FindBody(const PhysicsScene& scene, u32 bodyId);
    bool IsDynamic(const PhysicsBody& body);
    bool IsFinite(const PhysicsBody& body);
    bool IsFinite(const PhysicsCollider& collider);

    AABB3 ComputeColliderLocalBounds(const PhysicsCollider& collider);
    PhysicsObbShape ComputeBoxWorldShape(const PhysicsBody& body, const PhysicsCollider& collider);
    PhysicsCapsuleShape ComputeCapsuleWorldShape(const PhysicsBody& body, const PhysicsCollider& collider);
    AABB3 ComputeColliderWorldBounds(const PhysicsBody& body, const PhysicsCollider& collider);
    AABB3 InflateAABB(AABB3 bounds, float margin);

    std::vector<PhysicsBroadphasePair> BuildBroadphasePairs(const PhysicsScene& scene);
    std::vector<PhysicsBroadphasePair> BuildBroadphasePairsSpatialHash(const PhysicsScene& scene, PhysicsBroadphaseGridStats* outStats = nullptr);
    PhysicsContact BuildContact(const PhysicsScene& scene, const PhysicsBroadphasePair& pair);
    std::vector<PhysicsContact> BuildContacts(const PhysicsScene& scene, const std::vector<PhysicsBroadphasePair>& pairs);
    PhysicsContactManifold BuildContactManifold(const PhysicsContact& contact);
    PhysicsContactManifold BuildContactManifold(const PhysicsScene& scene, const PhysicsContact& contact);
    std::vector<PhysicsContactManifold> BuildContactManifolds(const std::vector<PhysicsContact>& contacts);
    std::vector<PhysicsContactManifold> BuildContactManifolds(const PhysicsScene& scene, const std::vector<PhysicsContact>& contacts);
    std::vector<PhysicsIsland> BuildPhysicsIslands(const PhysicsScene& scene, const std::vector<PhysicsBroadphasePair>& pairs);

    PhysicsRaycastHit RaycastPhysicsScene(const PhysicsScene& scene, Ray3 ray, float maxDistance, PhysicsQueryFlags flags = PhysicsQueryFlags::Default);
    PhysicsSweepHit SweepSpherePhysicsScene(const PhysicsScene& scene, Vec3 start, Vec3 end, float radius, PhysicsQueryFlags flags = PhysicsQueryFlags::Default);
    PhysicsSweepHit SweepSpherePhysicsSceneExcludingBody(const PhysicsScene& scene, Vec3 start, Vec3 end, float radius, u32 excludedBodyId, PhysicsQueryFlags flags = PhysicsQueryFlags::Default);

    void ApplyForce(PhysicsBody& body, Vec3 forceNewton);
    void IntegrateBodySemiImplicitEuler(PhysicsBody& body, const PhysicsWorldConfig& config, float deltaSeconds);
    void ResolveContact(PhysicsScene& scene, const PhysicsContact& contact);
    bool SolveDistanceConstraint(PhysicsScene& scene, const PhysicsDistanceConstraint& constraint, float deltaSeconds);
    std::size_t SolvePhysicsConstraints(PhysicsScene& scene, float deltaSeconds, u32 iterations);
    PhysicsStepStats StepPhysics(PhysicsScene& scene, float deltaSeconds);
    PhysicsStepStats FixedUpdatePhysicsSystem(PhysicsScene& scene, PhysicsSystemState& state, double frameDeltaSeconds);

    std::string ToDebugString(const PhysicsBody& body, int precision = 3);
    std::string ToDebugString(const PhysicsCollider& collider, int precision = 3);
    std::string ToDebugString(const PhysicsStepStats& stats);
    std::string ToDebugString(const PhysicsContact& contact, int precision = 3);
    std::string ToDebugString(const PhysicsBroadphaseGridStats& stats);
    std::string ToDebugString(const PhysicsRaycastHit& hit, int precision = 3);
    std::string ToDebugString(const PhysicsSweepHit& hit, int precision = 3);
    std::string ToDebugString(const PhysicsEvent& event, int precision = 3);
    std::string BuildPhysicsProbeSummary();
    PhysicsProbeResult BuildPhysicsProbe();
}
