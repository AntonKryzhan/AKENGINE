#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Damage/Damage.hpp>
#include <AK/Debris/Debris.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class ExplosionFalloffKind : u32
    {
        Linear = 0,
        Smoothstep = 1,
        InverseSquare = 2
    };

    enum class ExplosionOcclusionPolicy : u32
    {
        None = 0,
        RaycastFirstHit = 1
    };

    struct ExplosionFragmentationConfig
    {
        bool enabled = true;
        u32 debrisCount = 48;
        float debrisImpulseNewtonSeconds = 28.0f;
        float coneAngleDegrees = 180.0f;
        float minRadiusMeters = 0.015f;
        float maxRadiusMeters = 0.09f;
        DebrisMaterialDesc material = MakeDebrisMaterial(DebrisParticleKind::Stone);
    };

    struct ExplosionDesc
    {
        u32 sourceId = 0;
        Vec3 center{};
        float radiusMeters = 6.0f;
        float energyJoules = 12000.0f;
        float impulseScale = 1.0f;
        float damageScale = 1.0f;
        float heatKelvin = 0.0f;
        float upwardBias = 0.08f;
        float minimumAttenuation = 0.0f;
        ExplosionFalloffKind falloff = ExplosionFalloffKind::Smoothstep;
        ExplosionOcclusionPolicy occlusion = ExplosionOcclusionPolicy::RaycastFirstHit;
        float occludedTransmission = 0.25f;
        bool applyImpulse = true;
        bool queueDamage = true;
        bool spawnDebris = true;
        bool wakeBodies = true;
        ExplosionFragmentationConfig fragmentation{};
    };

    struct ExplosionTarget
    {
        u32 bodyId = 0;
        DamageTargetKind targetKind = DamageTargetKind::PhysicsBody;
        Vec3 position{};
        AABB3 bounds = MakeAABB3FromCenterExtents({}, {0.1f, 0.1f, 0.1f});
        float massKilograms = 1.0f;
        bool dynamicBody = false;
        bool valid = false;
    };

    struct ExplosionHit
    {
        u32 bodyId = 0;
        DamageTargetKind targetKind = DamageTargetKind::PhysicsBody;
        Vec3 point{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 direction{0.0f, 1.0f, 0.0f};
        float distanceMeters = 0.0f;
        float attenuation = 0.0f;
        float visibility = 1.0f;
        float impulseNewtonSeconds = 0.0f;
        float damageAmount = 0.0f;
        bool occluded = false;
        bool affected = false;
        DamageEvent damageEvent{};
    };

    struct ExplosionResult
    {
        std::size_t testedTargetCount = 0;
        std::size_t affectedTargetCount = 0;
        std::size_t occludedTargetCount = 0;
        std::size_t impulseAppliedCount = 0;
        std::size_t damageQueuedCount = 0;
        std::size_t debrisSpawnedCount = 0;
        float totalImpulseNewtonSeconds = 0.0f;
        float totalDamage = 0.0f;
        float maxDistanceMeters = 0.0f;
        bool finite = true;
        std::vector<ExplosionHit> hits;
        std::vector<std::string> warnings;
    };

    struct ExplosionProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        DamageSystem damage{};
        DebrisSystem debris{};
        std::vector<DamageableComponent> targets;
        ExplosionDesc explosion{};
        ExplosionResult explosionResult{};
        DamageSystemStats damageStats{};
        PhysicsStepStats physicsStats{};
    };

    const char* ToString(ExplosionFalloffKind kind);
    const char* ToString(ExplosionOcclusionPolicy policy);

    ExplosionDesc MakeExplosionDesc(Vec3 center, float radiusMeters, float energyJoules, u32 sourceId = 0);
    ExplosionTarget MakeExplosionTargetFromBody(const PhysicsBody& body, const PhysicsScene& scene, DamageTargetKind targetKind = DamageTargetKind::PhysicsBody);
    std::vector<ExplosionTarget> GatherExplosionTargetsFromPhysicsScene(const PhysicsScene& scene, DamageTargetKind targetKind = DamageTargetKind::PhysicsBody);

    bool IsFinite(const ExplosionDesc& explosion);
    bool IsFinite(const ExplosionTarget& target);
    bool IsFinite(const ExplosionHit& hit);
    ExplosionDesc SanitizeExplosionDesc(ExplosionDesc explosion);
    float EvaluateExplosionFalloff(float normalizedDistance, ExplosionFalloffKind kind);
    float EvaluateExplosionVisibility(const PhysicsScene& scene, const ExplosionDesc& explosion, const ExplosionTarget& target, bool* outOccluded = nullptr);
    ExplosionHit ComputeExplosionHit(const PhysicsScene& scene, const ExplosionDesc& explosion, const ExplosionTarget& target);
    DamageEvent MakeExplosionDamageEvent(const ExplosionDesc& explosion, const ExplosionHit& hit);
    DebrisEmitterDesc MakeExplosionDebrisEmitter(const ExplosionDesc& explosion, const ExplosionHit* strongestHit = nullptr);

    ExplosionResult ApplyExplosion(PhysicsScene& scene, const ExplosionDesc& explosion, const std::vector<ExplosionTarget>& targets, DamageSystem* damage = nullptr, DebrisSystem* debris = nullptr);

    std::string ToDebugString(const ExplosionDesc& explosion);
    std::string ToDebugString(const ExplosionHit& hit);
    std::string ToDebugString(const ExplosionResult& result);
    ExplosionProbeResult BuildExplosionProbe();
    std::string BuildExplosionProbeSummary();
}
