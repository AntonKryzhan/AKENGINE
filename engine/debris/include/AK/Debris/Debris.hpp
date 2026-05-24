#pragma once

#include <AK/Core/Types.hpp>
#include <AK/CSG/Boolean.hpp>
#include <AK/Fluid/Fluid.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>
#include <AK/Projectile/Projectile.hpp>
#include <AK/Surface/Surface.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class DebrisParticleState : u32
    {
        Active = 0,
        Sleeping = 1,
        Dead = 2
    };

    enum class DebrisSpawnMode : u32
    {
        ImpactCone = 0,
        Explosion = 1,
        VoxelCandidates = 2
    };

    enum class DebrisParticleKind : u32
    {
        Generic = 0,
        Wood = 1,
        Metal = 2,
        Stone = 3,
        Glass = 4,
        Dirt = 5,
        WaterSplash = 6,
        FireSpark = 7
    };

    struct DebrisMaterialDesc
    {
        DebrisParticleKind kind = DebrisParticleKind::Generic;
        float densityKgPerCubicMeter = 1200.0f;
        float restitution = 0.1f;
        float friction = 0.6f;
        float dragCoefficient = 0.65f;
        float buoyancyScale = 1.0f;
        float lifetimeScale = 1.0f;
        float sleepLinearVelocityThreshold = 0.08f;
        float heatDamagePerSecond = 0.0f;
        bool collides = true;
        bool receivesFluidForces = true;
        std::string debugName = "generic_debris";
    };

    struct DebrisParticle
    {
        u32 id = 0;
        DebrisParticleState state = DebrisParticleState::Active;
        DebrisParticleKind kind = DebrisParticleKind::Generic;
        Vec3 position{};
        Vec3 previousPosition{};
        Vec3 velocity{};
        Vec3 angularVelocity{};
        Vec3 halfExtents{0.04f, 0.04f, 0.04f};
        float radius = 0.04f;
        float massKilograms = 0.1f;
        float inverseMass = 10.0f;
        float ageSeconds = 0.0f;
        float lifetimeSeconds = 8.0f;
        float sleepTimerSeconds = 0.0f;
        float damage = 0.0f;
        u32 sourceBodyId = 0;
        bool fromVoxel = false;
        bool grounded = false;
    };

    struct DebrisEmitterDesc
    {
        DebrisSpawnMode mode = DebrisSpawnMode::ImpactCone;
        DebrisMaterialDesc material{};
        Vec3 origin{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 baseVelocity{};
        float impulseNewtonSeconds = 12.0f;
        float coneAngleDegrees = 50.0f;
        float minRadiusMeters = 0.015f;
        float maxRadiusMeters = 0.08f;
        float minLifetimeSeconds = 3.0f;
        float maxLifetimeSeconds = 12.0f;
        u32 count = 32;
        u32 maxSpawnPerFrame = 256;
        u32 randomSeed = 0xD3B815u;
        u32 sourceBodyId = 0;
    };

    struct DebrisSystemConfig
    {
        std::size_t maxParticles = 4096;
        Vec3 gravity{0.0f, -9.80665f, 0.0f};
        float fixedDeltaSeconds = 1.0f / 60.0f;
        float maxDeltaSeconds = 0.1f;
        float linearDamping = 0.02f;
        float angularDamping = 0.08f;
        float collisionSkinMeters = 0.005f;
        float sleepTimeSeconds = 0.5f;
        float killBelowY = -10000.0f;
        bool enableGravity = true;
        bool enableSceneCollision = true;
        bool enableFluidInteraction = true;
        bool enableSleeping = true;
        bool recycleOldestWhenFull = true;
    };

    struct DebrisSystem
    {
        DebrisSystemConfig config{};
        std::vector<DebrisParticle> particles;
        u32 nextParticleId = 1;
    };

    struct DebrisSpawnResult
    {
        u32 requested = 0;
        u32 spawned = 0;
        u32 recycled = 0;
        u32 clipped = 0;
        bool ok = false;
        std::vector<std::string> warnings;
    };

    struct DebrisStepStats
    {
        std::size_t particleCount = 0;
        std::size_t activeCount = 0;
        std::size_t sleepingCount = 0;
        std::size_t deadCount = 0;
        std::size_t integratedCount = 0;
        std::size_t collisionQueryCount = 0;
        std::size_t collisionHitCount = 0;
        std::size_t fluidSampleCount = 0;
        std::size_t fluidAffectedCount = 0;
        std::size_t killedByLifetime = 0;
        std::size_t recycledCount = 0;
        AABB3 activeBounds = MakeEmptyAABB3();
        float totalKineticEnergyJoules = 0.0f;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct DebrisPhysicsProxyResult
    {
        u32 createdBodies = 0;
        u32 createdColliders = 0;
        bool ok = false;
    };

    struct DebrisProbeResult
    {
        bool ok = false;
        std::string summary;
        DebrisSystem system{};
        DebrisSpawnResult spawn{};
        DebrisStepStats step{};
        DebrisPhysicsProxyResult proxy{};
        PhysicsScene scene{};
        std::vector<FluidVolume> fluids;
    };

    const char* ToString(DebrisParticleState state);
    const char* ToString(DebrisSpawnMode mode);
    const char* ToString(DebrisParticleKind kind);

    DebrisMaterialDesc MakeDebrisMaterial(DebrisParticleKind kind);
    DebrisMaterialDesc MakeDebrisMaterialFromSurface(const SurfaceMaterialDesc& surface);
    DebrisMaterialDesc MakeDebrisMaterialFromProjectileImpact(const ProjectileImpactMaterial& material);
    DebrisEmitterDesc MakeImpactDebrisEmitter(Vec3 origin, Vec3 normal, float impulseNewtonSeconds, DebrisMaterialDesc material = MakeDebrisMaterial(DebrisParticleKind::Generic));
    DebrisSystem MakeDebrisSystem(DebrisSystemConfig config = {});

    DebrisSpawnResult SpawnDebris(DebrisSystem& system, const DebrisEmitterDesc& emitter);
    DebrisSpawnResult SpawnDebrisFromVoxelCandidates(DebrisSystem& system, const std::vector<VoxelDebrisCandidate>& candidates, const DebrisEmitterDesc& emitter);
    DebrisSpawnResult SpawnDebrisFromVoxelDamage(DebrisSystem& system, const VoxelDamageStats& damage, const DebrisEmitterDesc& emitter);

    DebrisStepStats StepDebrisSystem(DebrisSystem& system, float deltaSeconds, const PhysicsScene* scene = nullptr, const std::vector<FluidVolume>* fluids = nullptr);
    DebrisPhysicsProxyResult CreateDebrisPhysicsProxies(const DebrisSystem& system, PhysicsScene& scene, float minRadiusMeters = 0.04f, u32 startBodyId = 60000u);
    void CompactDeadDebris(DebrisSystem& system);

    float EstimateDebrisMassKilograms(Vec3 halfExtents, const DebrisMaterialDesc& material);
    AABB3 ComputeDebrisParticleBounds(const DebrisParticle& particle);
    bool IsFinite(const DebrisParticle& particle);

    std::string ToDebugString(const DebrisMaterialDesc& material);
    std::string ToDebugString(const DebrisParticle& particle, int precision = 3);
    std::string ToDebugString(const DebrisSpawnResult& result);
    std::string ToDebugString(const DebrisStepStats& stats);
    std::string ToDebugString(const DebrisPhysicsProxyResult& result);
    DebrisProbeResult BuildDebrisProbe();
    std::string BuildDebrisProbeSummary();
}
