#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Debris/Debris.hpp>
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
    enum class DamageKind : u32
    {
        Generic = 0,
        ProjectileImpact = 1,
        Penetration = 2,
        Ricochet = 3,
        CollisionImpulse = 4,
        Explosion = 5,
        DebrisImpact = 6,
        FluidContact = 7,
        Fire = 8,
        Heat = 9,
        Crush = 10
    };

    enum class DamageTargetKind : u32
    {
        Unknown = 0,
        PhysicsBody = 1,
        DestructibleVoxel = 2,
        Character = 3,
        Vehicle = 4,
        Ragdoll = 5,
        SoftBody = 6,
        Debris = 7
    };

    enum class DamageResponseKind : u32
    {
        None = 0,
        Hit = 1,
        Fractured = 2,
        Destroyed = 3,
        Ignited = 4,
        Melted = 5
    };

    struct DamageResistanceProfile
    {
        float projectileScale = 1.0f;
        float penetrationScale = 1.0f;
        float collisionScale = 1.0f;
        float explosionScale = 1.0f;
        float debrisScale = 1.0f;
        float fluidScale = 1.0f;
        float heatScale = 1.0f;
        float fireScale = 1.0f;
        float armorJoules = 0.0f;
        float hardness = 1.0f;
        float ductility = 0.5f;
        float brittleness = 0.5f;
        bool invulnerable = false;
    };

    struct DamageableComponent
    {
        u32 targetId = 0;
        DamageTargetKind targetKind = DamageTargetKind::PhysicsBody;
        float maxHealth = 100.0f;
        float health = 100.0f;
        float structuralIntegrity = 100.0f;
        float fractureThreshold = 45.0f;
        float destroyThreshold = 0.0f;
        float accumulatedHeatKelvin = 0.0f;
        float ignitionThresholdKelvin = 700.0f;
        float meltThresholdKelvin = 1500.0f;
        DamageResistanceProfile resistance{};
        bool enabled = true;
        bool fractured = false;
        bool destroyed = false;
        bool burning = false;
        bool dirty = false;
    };

    struct DamageEvent
    {
        DamageKind kind = DamageKind::Generic;
        DamageTargetKind targetKind = DamageTargetKind::PhysicsBody;
        u32 sourceId = 0;
        u32 targetId = 0;
        Vec3 point{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 direction{0.0f, 0.0f, 1.0f};
        Vec3 velocity{};
        float amount = 0.0f;
        float kineticEnergyJoules = 0.0f;
        float impulseNewtonSeconds = 0.0f;
        float radiusMeters = 0.0f;
        float heatKelvin = 0.0f;
        float damagePerSecond = 0.0f;
        float deltaSeconds = 0.0f;
        float penetrationDepthMeters = 0.0f;
        float materialHardness = 1.0f;
        float materialDensityKgPerCubicMeter = 1000.0f;
        bool critical = false;
        bool valid = false;
    };

    struct DamageApplicationResult
    {
        bool applied = false;
        bool targetFound = false;
        DamageResponseKind response = DamageResponseKind::None;
        DamageKind kind = DamageKind::Generic;
        u32 targetId = 0;
        float rawDamage = 0.0f;
        float mitigatedDamage = 0.0f;
        float structuralDamage = 0.0f;
        float heatAddedKelvin = 0.0f;
        float healthBefore = 0.0f;
        float healthAfter = 0.0f;
        float structuralBefore = 0.0f;
        float structuralAfter = 0.0f;
        bool fractured = false;
        bool destroyed = false;
        bool ignited = false;
        bool melted = false;
        bool spawnDebris = false;
        bool dirtyBounds = false;
        bool wakePhysicsBody = false;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct DamageSystemConfig
    {
        std::size_t maxQueuedEvents = 4096;
        float minimumDamage = 0.001f;
        float impactEnergyToDamage = 0.02f;
        float impulseToDamage = 0.12f;
        float heatDamageScale = 0.05f;
        float structuralDamageScale = 0.55f;
        float fractureDebrisThreshold = 10.0f;
        bool clampHealth = true;
        bool wakeBodiesOnDamage = true;
        bool emitDebrisOnFracture = true;
        bool accumulateHeat = true;
    };

    struct DamageSystem
    {
        DamageSystemConfig config{};
        std::vector<DamageEvent> queue;
        std::vector<DamageApplicationResult> results;
        u64 processedSerial = 0;
    };

    struct DamageSystemStats
    {
        std::size_t queuedEventCount = 0;
        std::size_t processedEventCount = 0;
        std::size_t appliedEventCount = 0;
        std::size_t fracturedCount = 0;
        std::size_t destroyedCount = 0;
        std::size_t ignitedCount = 0;
        std::size_t meltedCount = 0;
        std::size_t debrisRequestCount = 0;
        std::size_t physicsWakeCount = 0;
        float totalRawDamage = 0.0f;
        float totalMitigatedDamage = 0.0f;
        float totalStructuralDamage = 0.0f;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct DamageProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        ProjectileBody projectile{};
        ProjectileImpactResult projectileImpact{};
        DebrisSystem debris{};
        FluidColliderInteraction fluidInteraction{};
        DamageSystem system{};
        std::vector<DamageableComponent> targets;
        std::vector<DamageApplicationResult> results;
        DamageSystemStats stats{};
    };

    const char* ToString(DamageKind kind);
    const char* ToString(DamageTargetKind kind);
    const char* ToString(DamageResponseKind kind);

    DamageResistanceProfile MakeDamageResistanceFromSurface(const SurfaceMaterialDesc& surface);
    DamageResistanceProfile MakeDamageResistanceFromProjectileMaterial(const ProjectileImpactMaterial& material);
    DamageableComponent MakeDamageableBody(u32 bodyId, float health = 100.0f, DamageResistanceProfile resistance = {});
    DamageSystem MakeDamageSystem(DamageSystemConfig config = {});

    bool IsFinite(const DamageResistanceProfile& resistance);
    bool IsFinite(const DamageableComponent& damageable);
    bool IsFinite(const DamageEvent& event);
    DamageEvent SanitizeDamageEvent(DamageEvent event);

    DamageEvent MakeProjectileDamageEvent(u32 targetId, const ProjectileBody& projectile, const ProjectileImpactResult& impact, DamageTargetKind targetKind = DamageTargetKind::PhysicsBody);
    DamageEvent MakePhysicsCollisionDamageEvent(const PhysicsScene& scene, const PhysicsEvent& event, float energyToDamageScale = 0.02f);
    DamageEvent MakeDebrisDamageEvent(u32 targetId, const DebrisParticle& particle, Vec3 contactPoint, Vec3 normal);
    DamageEvent MakeFluidDamageEvent(const FluidColliderInteraction& interaction, float deltaSeconds);
    DamageEvent MakeExplosionDamageEvent(u32 sourceId, u32 targetId, Vec3 point, float energyJoules, float radiusMeters, DamageTargetKind targetKind = DamageTargetKind::PhysicsBody);

    float EvaluateDamageResistanceScale(const DamageResistanceProfile& resistance, DamageKind kind);
    float ComputeRawDamageAmount(const DamageEvent& event, const DamageSystemConfig& config);
    DamageApplicationResult ApplyDamageEvent(DamageableComponent& target, const DamageEvent& event, const DamageSystemConfig& config = {});
    bool QueueDamageEvent(DamageSystem& system, DamageEvent event);
    DamageSystemStats ProcessDamageEvents(DamageSystem& system, std::vector<DamageableComponent>& targets, PhysicsScene* scene = nullptr, DebrisSystem* debris = nullptr);

    std::string ToDebugString(const DamageEvent& event);
    std::string ToDebugString(const DamageApplicationResult& result);
    std::string ToDebugString(const DamageSystemStats& stats);
    std::string BuildDamageProbeSummary();
    DamageProbeResult BuildDamageProbe();
}
