#include <AK/Debris/Debris.hpp>

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
        constexpr float MinimumDebrisRadius = 0.002f;
        constexpr float MinimumDebrisMass = 1.0e-5f;
        constexpr float MaximumDebrisSpeed = 180.0f;
        constexpr float MinimumDeltaSeconds = 1.0e-6f;

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }

        u32 HashU32(u32 value)
        {
            value ^= value >> 16u;
            value *= 0x7feb352du;
            value ^= value >> 15u;
            value *= 0x846ca68bu;
            value ^= value >> 16u;
            return value;
        }

        float Random01(u32& state)
        {
            state = HashU32(state + 0x9e3779b9u);
            return static_cast<float>(state & 0x00ffffffu) / static_cast<float>(0x01000000u);
        }

        float RandomRange(u32& state, float minValue, float maxValue)
        {
            return minValue + (maxValue - minValue) * Random01(state);
        }

        Vec3 Negate(Vec3 value)
        {
            return {-value.x, -value.y, -value.z};
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


        Vec3 RandomDirectionInCone(u32& state, Vec3 normal, float coneAngleDegrees)
        {
            const Vec3 n = Normalize(normal, {0.0f, 1.0f, 0.0f});
            Vec3 tangent = Normalize(Cross(n, {0.0f, 0.0f, 1.0f}), {1.0f, 0.0f, 0.0f});
            if (LengthSquared(tangent) <= FloatEpsilon)
            {
                tangent = {1.0f, 0.0f, 0.0f};
            }
            const Vec3 bitangent = Normalize(Cross(tangent, n), {0.0f, 0.0f, 1.0f});
            const float angle = std::clamp(coneAngleDegrees, 0.0f, 180.0f) * DegToRad32;
            const float cosMin = std::cos(angle);
            const float cosTheta = RandomRange(state, cosMin, 1.0f);
            const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
            const float phi = RandomRange(state, 0.0f, TwoPi32);
            Vec3 dir = Add(Multiply(n, cosTheta), Add(Multiply(tangent, std::cos(phi) * sinTheta), Multiply(bitangent, std::sin(phi) * sinTheta)));
            return Normalize(dir, n);
        }

        std::size_t FindReusableSlot(const DebrisSystem& system)
        {
            for (std::size_t i = 0; i < system.particles.size(); ++i)
            {
                if (system.particles[i].state == DebrisParticleState::Dead)
                {
                    return i;
                }
            }

            std::size_t oldest = 0;
            float oldestAge = -1.0f;
            for (std::size_t i = 0; i < system.particles.size(); ++i)
            {
                const DebrisParticle& particle = system.particles[i];
                if (particle.state == DebrisParticleState::Sleeping && particle.ageSeconds > oldestAge)
                {
                    oldestAge = particle.ageSeconds;
                    oldest = i;
                }
            }
            if (oldestAge >= 0.0f)
            {
                return oldest;
            }

            for (std::size_t i = 0; i < system.particles.size(); ++i)
            {
                const DebrisParticle& particle = system.particles[i];
                if (particle.ageSeconds > oldestAge)
                {
                    oldestAge = particle.ageSeconds;
                    oldest = i;
                }
            }
            return oldest;
        }

        DebrisParticle MakeParticleFromEmitter(DebrisSystem& system, const DebrisEmitterDesc& emitter, u32& rngState, Vec3 origin, Vec3 baseSize, Vec3 initialImpulse, bool fromVoxel)
        {
            DebrisParticle particle{};
            particle.id = system.nextParticleId++;
            particle.state = DebrisParticleState::Active;
            particle.kind = emitter.material.kind;
            particle.position = origin;
            particle.previousPosition = origin;

            const float radius = std::clamp(RandomRange(rngState, emitter.minRadiusMeters, emitter.maxRadiusMeters), MinimumDebrisRadius, 2.0f);
            if (LengthSquared(baseSize) > FloatEpsilon)
            {
                particle.halfExtents = Multiply(baseSize, 0.5f);
                particle.radius = std::max(radius, 0.5f * std::max({baseSize.x, baseSize.y, baseSize.z}));
            }
            else
            {
                const float anisotropy = RandomRange(rngState, 0.65f, 1.45f);
                particle.halfExtents = {radius * anisotropy, radius * RandomRange(rngState, 0.55f, 1.2f), radius * RandomRange(rngState, 0.55f, 1.3f)};
                particle.radius = radius;
            }

            particle.massKilograms = EstimateDebrisMassKilograms(particle.halfExtents, emitter.material);
            particle.inverseMass = particle.massKilograms > MinimumDebrisMass ? 1.0f / particle.massKilograms : 0.0f;
            particle.lifetimeSeconds = RandomRange(rngState, emitter.minLifetimeSeconds, emitter.maxLifetimeSeconds) * std::max(0.05f, emitter.material.lifetimeScale);
            particle.sourceBodyId = emitter.sourceBodyId;
            particle.fromVoxel = fromVoxel;

            const Vec3 randomDir = RandomDirectionInCone(rngState, emitter.normal, emitter.coneAngleDegrees);
            const float impulseJitter = RandomRange(rngState, 0.35f, 1.15f);
            const Vec3 coneImpulse = Multiply(randomDir, emitter.impulseNewtonSeconds * impulseJitter);
            const Vec3 totalImpulse = Add(coneImpulse, initialImpulse);
            particle.velocity = Add(emitter.baseVelocity, Multiply(totalImpulse, particle.inverseMass));
            particle.velocity = ClampLength(particle.velocity, MaximumDebrisSpeed);
            particle.angularVelocity = Multiply(RandomDirectionInCone(rngState, randomDir, 180.0f), RandomRange(rngState, 1.0f, 32.0f));
            return particle;
        }

        void StoreParticle(DebrisSystem& system, DebrisParticle particle, DebrisSpawnResult& result)
        {
            if (system.particles.size() < system.config.maxParticles)
            {
                system.particles.push_back(particle);
                ++result.spawned;
                return;
            }

            if (!system.config.recycleOldestWhenFull || system.particles.empty())
            {
                ++result.clipped;
                return;
            }

            const std::size_t slot = FindReusableSlot(system);
            system.particles[slot] = particle;
            ++result.spawned;
            ++result.recycled;
        }

        float ParticleProjectedArea(const DebrisParticle& particle)
        {
            return Pi32 * particle.radius * particle.radius;
        }

        void ApplyFluidToParticle(DebrisParticle& particle, const std::vector<FluidVolume>& fluids, float deltaSeconds, DebrisStepStats& stats, const DebrisSystemConfig& config)
        {
            const FluidSample sample = SampleFluidAtPoint(fluids, particle.position);
            ++stats.fluidSampleCount;
            if (!sample.inside)
            {
                return;
            }

            ++stats.fluidAffectedCount;
            const float volume = std::max(0.0f, 8.0f * particle.halfExtents.x * particle.halfExtents.y * particle.halfExtents.z);
            const Vec3 up = Normalize(Negate(config.gravity), {0.0f, 1.0f, 0.0f});
            const float g = std::max(0.0f, Length(config.gravity));
            const float buoyancy = sample.medium.densityKgPerCubicMeter * volume * g * sample.medium.buoyancyScale;
            const Vec3 buoyancyAcceleration = Multiply(up, buoyancy * particle.inverseMass);

            const Vec3 relativeVelocity = Subtract(particle.velocity, sample.flowVelocity);
            const float speed = Length(relativeVelocity);
            const Vec3 dragDirection = speed > FloatEpsilon ? Multiply(relativeVelocity, -1.0f / speed) : Vec3{};
            const float area = ParticleProjectedArea(particle);
            const float drag = 0.5f * sample.medium.densityKgPerCubicMeter * speed * speed * area * std::max(0.0f, sample.medium.linearDragCoefficient);
            const Vec3 dragAcceleration = Multiply(dragDirection, drag * particle.inverseMass);

            particle.velocity = Add(particle.velocity, Multiply(Add(buoyancyAcceleration, dragAcceleration), deltaSeconds));
            particle.damage += sample.medium.damageOnContact ? sample.medium.damagePerSecond * deltaSeconds : 0.0f;
        }

        void CollideParticleWithScene(DebrisParticle& particle, const PhysicsScene& scene, const DebrisSystemConfig& config, float deltaSeconds, DebrisStepStats& stats)
        {
            const Vec3 from = particle.previousPosition;
            const Vec3 to = particle.position;
            ++stats.collisionQueryCount;
            const PhysicsSweepHit hit = SweepSpherePhysicsScene(scene, from, to, particle.radius + config.collisionSkinMeters, PhysicsQueryFlags::Default);
            if (!hit.hit)
            {
                particle.grounded = false;
                return;
            }

            ++stats.collisionHitCount;
            particle.position = Add(hit.point, Multiply(hit.normal, particle.radius + config.collisionSkinMeters));
            const float normalSpeed = Dot(particle.velocity, hit.normal);
            if (normalSpeed < 0.0f)
            {
                Vec3 normalVelocity = Multiply(hit.normal, normalSpeed);
                Vec3 tangentVelocity = Subtract(particle.velocity, normalVelocity);
                float restitution = 0.15f;
                float friction = 0.55f;
                if (hit.collider < scene.colliders.size())
                {
                    restitution = scene.colliders[hit.collider].material.restitution;
                    friction = scene.colliders[hit.collider].material.dynamicFriction;
                }
                normalVelocity = Multiply(normalVelocity, -std::clamp(restitution, 0.0f, 1.0f));
                tangentVelocity = Multiply(tangentVelocity, std::clamp(1.0f - friction * 0.35f, 0.0f, 1.0f));
                particle.velocity = Add(normalVelocity, tangentVelocity);
            }
            particle.grounded = Dot(hit.normal, Normalize(Negate(config.gravity), {0.0f, 1.0f, 0.0f})) > 0.55f;

            const float speed = Length(particle.velocity);
            if (speed < 0.15f && particle.grounded)
            {
                particle.velocity = Multiply(particle.velocity, std::max(0.0f, 1.0f - deltaSeconds * 8.0f));
            }
        }
    }

    const char* ToString(DebrisParticleState state)
    {
        switch (state)
        {
        case DebrisParticleState::Active:
            return "Active";
        case DebrisParticleState::Sleeping:
            return "Sleeping";
        case DebrisParticleState::Dead:
            return "Dead";
        default:
            return "Unknown";
        }
    }

    const char* ToString(DebrisSpawnMode mode)
    {
        switch (mode)
        {
        case DebrisSpawnMode::ImpactCone:
            return "ImpactCone";
        case DebrisSpawnMode::Explosion:
            return "Explosion";
        case DebrisSpawnMode::VoxelCandidates:
            return "VoxelCandidates";
        default:
            return "Unknown";
        }
    }

    const char* ToString(DebrisParticleKind kind)
    {
        switch (kind)
        {
        case DebrisParticleKind::Generic:
            return "Generic";
        case DebrisParticleKind::Wood:
            return "Wood";
        case DebrisParticleKind::Metal:
            return "Metal";
        case DebrisParticleKind::Stone:
            return "Stone";
        case DebrisParticleKind::Glass:
            return "Glass";
        case DebrisParticleKind::Dirt:
            return "Dirt";
        case DebrisParticleKind::WaterSplash:
            return "WaterSplash";
        case DebrisParticleKind::FireSpark:
            return "FireSpark";
        default:
            return "Unknown";
        }
    }

    DebrisMaterialDesc MakeDebrisMaterial(DebrisParticleKind kind)
    {
        DebrisMaterialDesc material{};
        material.kind = kind;
        switch (kind)
        {
        case DebrisParticleKind::Wood:
            material.densityKgPerCubicMeter = 650.0f;
            material.restitution = 0.18f;
            material.friction = 0.72f;
            material.debugName = "wood_debris";
            break;
        case DebrisParticleKind::Metal:
            material.densityKgPerCubicMeter = 7800.0f;
            material.restitution = 0.12f;
            material.friction = 0.45f;
            material.lifetimeScale = 1.4f;
            material.debugName = "metal_debris";
            break;
        case DebrisParticleKind::Stone:
            material.densityKgPerCubicMeter = 2400.0f;
            material.restitution = 0.08f;
            material.friction = 0.80f;
            material.debugName = "stone_debris";
            break;
        case DebrisParticleKind::Glass:
            material.densityKgPerCubicMeter = 2500.0f;
            material.restitution = 0.05f;
            material.friction = 0.35f;
            material.lifetimeScale = 0.75f;
            material.debugName = "glass_debris";
            break;
        case DebrisParticleKind::Dirt:
            material.densityKgPerCubicMeter = 1500.0f;
            material.restitution = 0.02f;
            material.friction = 0.95f;
            material.debugName = "dirt_debris";
            break;
        case DebrisParticleKind::WaterSplash:
            material.densityKgPerCubicMeter = 997.0f;
            material.restitution = 0.0f;
            material.friction = 0.1f;
            material.lifetimeScale = 0.25f;
            material.collides = false;
            material.debugName = "water_splash";
            break;
        case DebrisParticleKind::FireSpark:
            material.densityKgPerCubicMeter = 80.0f;
            material.restitution = 0.0f;
            material.friction = 0.05f;
            material.lifetimeScale = 0.18f;
            material.collides = false;
            material.receivesFluidForces = false;
            material.debugName = "fire_spark";
            break;
        case DebrisParticleKind::Generic:
        default:
            break;
        }
        return material;
    }

    DebrisMaterialDesc MakeDebrisMaterialFromSurface(const SurfaceMaterialDesc& surface)
    {
        DebrisParticleKind kind = DebrisParticleKind::Generic;
        switch (surface.kind)
        {
        case SurfaceMaterialKind::Wood:
            kind = DebrisParticleKind::Wood;
            break;
        case SurfaceMaterialKind::Metal:
            kind = DebrisParticleKind::Metal;
            break;
        case SurfaceMaterialKind::Glass:
            kind = DebrisParticleKind::Glass;
            break;
        case SurfaceMaterialKind::Concrete:
        case SurfaceMaterialKind::Rock:
            kind = DebrisParticleKind::Stone;
            break;
        case SurfaceMaterialKind::TerrainSoil:
        case SurfaceMaterialKind::Sand:
        case SurfaceMaterialKind::Grass:
            kind = DebrisParticleKind::Dirt;
            break;
        case SurfaceMaterialKind::Water:
            kind = DebrisParticleKind::WaterSplash;
            break;
        default:
            kind = DebrisParticleKind::Generic;
            break;
        }

        DebrisMaterialDesc material = MakeDebrisMaterial(kind);
        material.densityKgPerCubicMeter = std::max(1.0f, surface.physics.densityKgPerCubicMeter);
        material.restitution = std::clamp(surface.physics.restitution, 0.0f, 1.0f);
        material.friction = std::clamp(surface.physics.dynamicFriction, 0.0f, 4.0f);
        return material;
    }

    DebrisMaterialDesc MakeDebrisMaterialFromProjectileImpact(const ProjectileImpactMaterial& material)
    {
        DebrisMaterialDesc debris = MakeDebrisMaterial(DebrisParticleKind::Generic);
        debris.densityKgPerCubicMeter = std::max(1.0f, material.densityKgPerCubicMeter);
        debris.restitution = std::clamp(1.0f - material.ricochetEnergyLoss, 0.0f, 1.0f) * 0.25f;
        debris.friction = std::clamp(0.35f + material.energyDissipation, 0.0f, 2.0f);
        debris.debugName = material.name + "_debris";
        return debris;
    }

    DebrisEmitterDesc MakeImpactDebrisEmitter(Vec3 origin, Vec3 normal, float impulseNewtonSeconds, DebrisMaterialDesc material)
    {
        DebrisEmitterDesc emitter{};
        emitter.mode = DebrisSpawnMode::ImpactCone;
        emitter.material = material;
        emitter.origin = origin;
        emitter.normal = Normalize(normal, {0.0f, 1.0f, 0.0f});
        emitter.impulseNewtonSeconds = std::max(0.0f, impulseNewtonSeconds);
        return emitter;
    }

    DebrisSystem MakeDebrisSystem(DebrisSystemConfig config)
    {
        DebrisSystem system{};
        config.maxParticles = std::max<std::size_t>(1, config.maxParticles);
        config.fixedDeltaSeconds = std::clamp(config.fixedDeltaSeconds, 1.0e-5f, 0.25f);
        config.maxDeltaSeconds = std::max(config.fixedDeltaSeconds, config.maxDeltaSeconds);
        config.collisionSkinMeters = std::max(0.0f, config.collisionSkinMeters);
        system.config = config;
        system.particles.reserve(std::min<std::size_t>(config.maxParticles, 8192));
        return system;
    }

    float EstimateDebrisMassKilograms(Vec3 halfExtents, const DebrisMaterialDesc& material)
    {
        const float volume = std::max(1.0e-8f, 8.0f * std::abs(halfExtents.x) * std::abs(halfExtents.y) * std::abs(halfExtents.z));
        return std::max(MinimumDebrisMass, volume * std::max(1.0f, material.densityKgPerCubicMeter));
    }

    DebrisSpawnResult SpawnDebris(DebrisSystem& system, const DebrisEmitterDesc& emitter)
    {
        DebrisSpawnResult result{};
        result.requested = emitter.count;
        if (system.config.maxParticles == 0)
        {
            AddWarning(result.warnings, "debris system capacity is zero");
            return result;
        }

        u32 rngState = emitter.randomSeed ^ static_cast<u32>(system.nextParticleId * 2654435761u);
        const u32 toSpawn = std::min(emitter.count, emitter.maxSpawnPerFrame);
        for (u32 i = 0; i < toSpawn; ++i)
        {
            Vec3 offset = Multiply(RandomDirectionInCone(rngState, emitter.normal, 180.0f), RandomRange(rngState, 0.0f, emitter.maxRadiusMeters));
            DebrisParticle particle = MakeParticleFromEmitter(system, emitter, rngState, Add(emitter.origin, offset), {}, {}, false);
            StoreParticle(system, particle, result);
        }
        result.clipped += emitter.count > toSpawn ? emitter.count - toSpawn : 0;
        result.ok = result.spawned > 0 && result.warnings.empty();
        return result;
    }

    DebrisSpawnResult SpawnDebrisFromVoxelCandidates(DebrisSystem& system, const std::vector<VoxelDebrisCandidate>& candidates, const DebrisEmitterDesc& emitter)
    {
        DebrisSpawnResult result{};
        result.requested = static_cast<u32>(candidates.size());
        if (candidates.empty())
        {
            result.ok = true;
            return result;
        }

        u32 rngState = emitter.randomSeed ^ 0xA5A5F00Du;
        const u32 toSpawn = std::min<u32>(static_cast<u32>(candidates.size()), emitter.maxSpawnPerFrame);
        for (u32 i = 0; i < toSpawn; ++i)
        {
            const VoxelDebrisCandidate& candidate = candidates[i];
            DebrisParticle particle = MakeParticleFromEmitter(system, emitter, rngState, candidate.center, candidate.size, candidate.initialImpulse, true);
            if (candidate.massKilograms > MinimumDebrisMass)
            {
                particle.massKilograms = candidate.massKilograms;
                particle.inverseMass = 1.0f / particle.massKilograms;
            }
            StoreParticle(system, particle, result);
        }
        result.clipped += candidates.size() > toSpawn ? static_cast<u32>(candidates.size() - toSpawn) : 0u;
        result.ok = result.spawned > 0 && result.warnings.empty();
        return result;
    }

    DebrisSpawnResult SpawnDebrisFromVoxelDamage(DebrisSystem& system, const VoxelDamageStats& damage, const DebrisEmitterDesc& emitter)
    {
        return SpawnDebrisFromVoxelCandidates(system, damage.debrisCandidates, emitter);
    }

    AABB3 ComputeDebrisParticleBounds(const DebrisParticle& particle)
    {
        return MakeAABB3FromCenterExtents(particle.position, Max(particle.halfExtents, {particle.radius, particle.radius, particle.radius}));
    }

    bool IsFinite(const DebrisParticle& particle)
    {
        return AK::IsFinite(particle.position)
            && AK::IsFinite(particle.previousPosition)
            && AK::IsFinite(particle.velocity)
            && AK::IsFinite(particle.angularVelocity)
            && AK::IsFinite(particle.radius)
            && AK::IsFinite(particle.massKilograms)
            && particle.radius >= 0.0f
            && particle.massKilograms >= 0.0f;
    }

    DebrisStepStats StepDebrisSystem(DebrisSystem& system, float deltaSeconds, const PhysicsScene* scene, const std::vector<FluidVolume>* fluids)
    {
        DebrisStepStats stats{};
        const float dt = std::clamp(deltaSeconds, MinimumDeltaSeconds, system.config.maxDeltaSeconds);
        stats.particleCount = system.particles.size();

        for (DebrisParticle& particle : system.particles)
        {
            if (particle.state == DebrisParticleState::Dead)
            {
                ++stats.deadCount;
                continue;
            }
            if (!IsFinite(particle))
            {
                particle.state = DebrisParticleState::Dead;
                stats.finite = false;
                AddWarning(stats.warnings, "killed non-finite debris particle");
                ++stats.deadCount;
                continue;
            }

            particle.ageSeconds += dt;
            if (particle.ageSeconds >= particle.lifetimeSeconds || particle.position.y < system.config.killBelowY)
            {
                particle.state = DebrisParticleState::Dead;
                ++stats.killedByLifetime;
                ++stats.deadCount;
                continue;
            }

            if (particle.state == DebrisParticleState::Sleeping)
            {
                ++stats.sleepingCount;
                stats.activeBounds = IsValid(stats.activeBounds) ? Union(stats.activeBounds, ComputeDebrisParticleBounds(particle)) : ComputeDebrisParticleBounds(particle);
                continue;
            }

            ++stats.activeCount;
            ++stats.integratedCount;
            particle.previousPosition = particle.position;

            if (system.config.enableGravity)
            {
                particle.velocity = Add(particle.velocity, Multiply(system.config.gravity, dt));
            }

            if (system.config.enableFluidInteraction && fluids)
            {
                ApplyFluidToParticle(particle, *fluids, dt, stats, system.config);
            }

            const float damping = std::clamp(1.0f - system.config.linearDamping * dt, 0.0f, 1.0f);
            particle.velocity = Multiply(ClampLength(particle.velocity, MaximumDebrisSpeed), damping);
            particle.angularVelocity = Multiply(particle.angularVelocity, std::clamp(1.0f - system.config.angularDamping * dt, 0.0f, 1.0f));
            particle.position = Add(particle.position, Multiply(particle.velocity, dt));

            if (system.config.enableSceneCollision && scene)
            {
                CollideParticleWithScene(particle, *scene, system.config, dt, stats);
            }

            const float speed = Length(particle.velocity);
            if (system.config.enableSleeping && speed < 0.08f && particle.grounded)
            {
                particle.sleepTimerSeconds += dt;
                if (particle.sleepTimerSeconds >= system.config.sleepTimeSeconds)
                {
                    particle.state = DebrisParticleState::Sleeping;
                    particle.velocity = {};
                    particle.angularVelocity = {};
                    ++stats.sleepingCount;
                }
            }
            else
            {
                particle.sleepTimerSeconds = 0.0f;
            }

            const float kinetic = 0.5f * particle.massKilograms * LengthSquared(particle.velocity);
            stats.totalKineticEnergyJoules += kinetic;
            stats.activeBounds = IsValid(stats.activeBounds) ? Union(stats.activeBounds, ComputeDebrisParticleBounds(particle)) : ComputeDebrisParticleBounds(particle);
        }

        return stats;
    }

    DebrisPhysicsProxyResult CreateDebrisPhysicsProxies(const DebrisSystem& system, PhysicsScene& scene, float minRadiusMeters, u32 startBodyId)
    {
        DebrisPhysicsProxyResult result{};
        u32 bodyId = startBodyId;
        for (const DebrisParticle& particle : system.particles)
        {
            if (particle.state == DebrisParticleState::Dead || particle.radius < minRadiusMeters)
            {
                continue;
            }
            PhysicsBody body = MakeDynamicBody(bodyId, particle.position, particle.massKilograms);
            body.velocity = particle.velocity;
            body.linearDamping = system.config.linearDamping;
            PhysicsCollider collider = MakeSphereCollider(bodyId, particle.radius);
            collider.filter.layerMask = PhysicsLayer_Destructible;
            collider.filter.collidesWithMask = PhysicsLayer_Static | PhysicsLayer_Dynamic | PhysicsLayer_Character | PhysicsLayer_Destructible;
            collider.material = MakePhysicsMaterial(0.7f, 0.55f, 0.08f, std::max(1.0f, particle.massKilograms));
            scene.bodies.push_back(body);
            scene.colliders.push_back(collider);
            ++bodyId;
            ++result.createdBodies;
            ++result.createdColliders;
        }
        result.ok = true;
        return result;
    }

    void CompactDeadDebris(DebrisSystem& system)
    {
        system.particles.erase(std::remove_if(system.particles.begin(), system.particles.end(), [](const DebrisParticle& particle) {
            return particle.state == DebrisParticleState::Dead;
        }), system.particles.end());
    }

    std::string ToDebugString(const DebrisMaterialDesc& material)
    {
        std::ostringstream stream;
        stream << "debris_material kind=" << ToString(material.kind)
               << " density=" << material.densityKgPerCubicMeter
               << " restitution=" << material.restitution
               << " friction=" << material.friction
               << " drag=" << material.dragCoefficient
               << " fluid=" << (material.receivesFluidForces ? "true" : "false")
               << " collides=" << (material.collides ? "true" : "false");
        return stream.str();
    }

    std::string ToDebugString(const DebrisParticle& particle, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision)
               << "debris id=" << particle.id
               << " state=" << ToString(particle.state)
               << " kind=" << ToString(particle.kind)
               << " pos=" << ToDebugString(particle.position, precision)
               << " vel=" << ToDebugString(particle.velocity, precision)
               << " radius=" << particle.radius
               << " mass=" << particle.massKilograms
               << " age=" << particle.ageSeconds << "/" << particle.lifetimeSeconds
               << " voxel=" << (particle.fromVoxel ? "true" : "false")
               << " grounded=" << (particle.grounded ? "true" : "false");
        return stream.str();
    }

    std::string ToDebugString(const DebrisSpawnResult& result)
    {
        std::ostringstream stream;
        stream << "debris_spawn requested=" << result.requested
               << " spawned=" << result.spawned
               << " recycled=" << result.recycled
               << " clipped=" << result.clipped
               << " ok=" << (result.ok ? "true" : "false")
               << " warnings=" << result.warnings.size();
        return stream.str();
    }

    std::string ToDebugString(const DebrisStepStats& stats)
    {
        std::ostringstream stream;
        stream << "debris_step particles=" << stats.particleCount
               << " active=" << stats.activeCount
               << " sleeping=" << stats.sleepingCount
               << " dead=" << stats.deadCount
               << " integrated=" << stats.integratedCount
               << " collisions=" << stats.collisionHitCount << "/" << stats.collisionQueryCount
               << " fluid=" << stats.fluidAffectedCount << "/" << stats.fluidSampleCount
               << " killed=" << stats.killedByLifetime
               << " kinetic=" << stats.totalKineticEnergyJoules
               << " finite=" << (stats.finite ? "true" : "false")
               << " warnings=" << stats.warnings.size();
        return stream.str();
    }

    std::string ToDebugString(const DebrisPhysicsProxyResult& result)
    {
        std::ostringstream stream;
        stream << "debris_physics_proxy bodies=" << result.createdBodies
               << " colliders=" << result.createdColliders
               << " ok=" << (result.ok ? "true" : "false");
        return stream.str();
    }

    DebrisProbeResult BuildDebrisProbe()
    {
        DebrisProbeResult probe{};
        DebrisSystemConfig config{};
        config.maxParticles = 256;
        config.fixedDeltaSeconds = 1.0f / 120.0f;
        probe.system = MakeDebrisSystem(config);

        probe.scene.config.broadphaseGridCellSize = 1.0f;
        probe.scene.bodies.push_back(MakeStaticBody(1, {0.0f, 0.0f, 0.0f}));
        PhysicsCollider floor = MakeBoxCollider(1, {8.0f, 0.25f, 8.0f}, {0.0f, -0.25f, 0.0f});
        floor.material = MakePhysicsMaterial(0.85f, 0.75f, 0.08f, 2400.0f);
        probe.scene.colliders.push_back(floor);

        probe.fluids.push_back(MakeBoxFluidVolume(1, {0.25f, 0.75f, 0.15f}, {1.8f, 1.1f, 1.8f}, MakeFluidMedium(FluidMediumKind::FreshWater)));

        CsgVoxelGrid grid = MakeCsgVoxelGrid(MakeAABB3FromCenterExtents({0.0f, 0.8f, 0.0f}, {0.8f, 0.8f, 0.8f}), 12, 12, 12);
        for (u32 z = 0; z < grid.resolutionZ; ++z)
        {
            for (u32 y = 0; y < grid.resolutionY; ++y)
            {
                for (u32 x = 0; x < grid.resolutionX; ++x)
                {
                    CsgSetSolid(grid, x, y, z, true);
                }
            }
        }
        VoxelTunnelSettings tunnel{};
        tunnel.radiusMeters = 0.19f;
        tunnel.maxDepthMeters = 1.8f;
        VoxelDamageStats damage = RemoveVoxelTunnelDda(grid, {-0.8f, 0.8f, 0.0f}, {1.0f, -0.1f, 0.15f}, tunnel);

        DebrisEmitterDesc emitter = MakeImpactDebrisEmitter({-0.35f, 0.95f, 0.0f}, Normalize(Vec3{1.0f, 0.35f, 0.1f}), 3.5f, MakeDebrisMaterial(DebrisParticleKind::Stone));
        emitter.count = 48;
        emitter.maxSpawnPerFrame = 64;
        emitter.minRadiusMeters = 0.018f;
        emitter.maxRadiusMeters = 0.055f;
        emitter.baseVelocity = {1.5f, 1.0f, 0.25f};
        emitter.randomSeed = 0x41574B55u;
        probe.spawn = SpawnDebrisFromVoxelDamage(probe.system, damage, emitter);
        if (probe.spawn.spawned == 0)
        {
            probe.spawn = SpawnDebris(probe.system, emitter);
        }

        for (int i = 0; i < 80; ++i)
        {
            probe.step = StepDebrisSystem(probe.system, 1.0f / 120.0f, &probe.scene, &probe.fluids);
        }
        probe.proxy = CreateDebrisPhysicsProxies(probe.system, probe.scene, 0.03f, 61000u);
        probe.ok = probe.spawn.spawned > 0 && probe.step.finite && probe.proxy.ok;

        std::ostringstream stream;
        stream << "[ ok ] debris/fracture runtime foundation spawned=" << probe.spawn.spawned
               << " active=" << probe.step.activeCount
               << " sleeping=" << probe.step.sleepingCount
               << " collisions=" << probe.step.collisionHitCount
               << " fluid=" << probe.step.fluidAffectedCount
               << " proxies=" << probe.proxy.createdBodies
               << " kinetic=" << std::fixed << std::setprecision(2) << probe.step.totalKineticEnergyJoules
               << " finite=" << (probe.step.finite ? "true" : "false")
               << " warnings=" << probe.step.warnings.size();
        probe.summary = stream.str();
        if (!probe.ok)
        {
            probe.summary = "[fail] " + probe.summary;
        }
        return probe;
    }

    std::string BuildDebrisProbeSummary()
    {
        return BuildDebrisProbe().summary;
    }
}
