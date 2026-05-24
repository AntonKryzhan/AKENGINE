#include <AK/Explosion/Explosion.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr float MinimumRadiusMeters = 0.001f;
        constexpr float MaximumEnergyJoules = 1.0e9f;

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }

        float SafeMass(float mass)
        {
            return std::max(mass, 0.001f);
        }

        Vec3 DirectionFromExplosion(Vec3 center, Vec3 point, float upwardBias)
        {
            Vec3 direction = Subtract(point, center);
            direction.y += std::max(0.0f, upwardBias) * std::max(Length(direction), 1.0f);
            return Normalize(direction, {0.0f, 1.0f, 0.0f});
        }

        AABB3 ComputeBodyBounds(const PhysicsScene& scene, const PhysicsBody& body)
        {
            AABB3 bounds = MakeEmptyAABB3();
            bool any = false;
            for (const PhysicsCollider& collider : scene.colliders)
            {
                if (!collider.enabled || collider.bodyId != body.id)
                {
                    continue;
                }
                const AABB3 colliderBounds = ComputeColliderWorldBounds(body, collider);
                bounds = any ? Union(bounds, colliderBounds) : colliderBounds;
                any = true;
            }
            if (!any)
            {
                bounds = MakeAABB3FromCenterExtents(body.position, {0.1f, 0.1f, 0.1f});
            }
            return bounds;
        }

        Vec3 ClosestPointOnAABB(AABB3 bounds, Vec3 point)
        {
            return {
                std::clamp(point.x, bounds.min.x, bounds.max.x),
                std::clamp(point.y, bounds.min.y, bounds.max.y),
                std::clamp(point.z, bounds.min.z, bounds.max.z)
            };
        }

        float ApproximateTargetArea(AABB3 bounds)
        {
            const Vec3 size = Size(bounds);
            const float xy = std::max(0.0f, size.x * size.y);
            const float xz = std::max(0.0f, size.x * size.z);
            const float yz = std::max(0.0f, size.y * size.z);
            return std::max({xy, xz, yz, 0.01f});
        }

        float EstimatePeakOverpressureDamage(float energyJoules, float radiusMeters, float distanceMeters, float attenuation, float area)
        {
            const float safeDistance = std::max(distanceMeters, radiusMeters * 0.08f + 0.05f);
            const float pressureLike = energyJoules / (4.0f * Pi32 * safeDistance * safeDistance * std::max(radiusMeters, 0.05f));
            return pressureLike * area * 0.0025f * attenuation;
        }

        u32 StableSeedFromExplosion(const ExplosionDesc& explosion)
        {
            const u32 x = static_cast<u32>(std::abs(explosion.center.x) * 73856093.0f);
            const u32 y = static_cast<u32>(std::abs(explosion.center.y) * 19349663.0f);
            const u32 z = static_cast<u32>(std::abs(explosion.center.z) * 83492791.0f);
            return 0xE970510u ^ explosion.sourceId ^ x ^ y ^ z;
        }
    }

    const char* ToString(ExplosionFalloffKind kind)
    {
        switch (kind)
        {
        case ExplosionFalloffKind::Linear: return "Linear";
        case ExplosionFalloffKind::Smoothstep: return "Smoothstep";
        case ExplosionFalloffKind::InverseSquare: return "InverseSquare";
        default: return "Unknown";
        }
    }

    const char* ToString(ExplosionOcclusionPolicy policy)
    {
        switch (policy)
        {
        case ExplosionOcclusionPolicy::None: return "None";
        case ExplosionOcclusionPolicy::RaycastFirstHit: return "RaycastFirstHit";
        default: return "Unknown";
        }
    }

    ExplosionDesc MakeExplosionDesc(Vec3 center, float radiusMeters, float energyJoules, u32 sourceId)
    {
        ExplosionDesc explosion{};
        explosion.sourceId = sourceId;
        explosion.center = center;
        explosion.radiusMeters = radiusMeters;
        explosion.energyJoules = energyJoules;
        explosion.heatKelvin = std::sqrt(std::max(0.0f, energyJoules)) * 0.12f;
        explosion.fragmentation.debrisImpulseNewtonSeconds = std::sqrt(std::max(0.0f, energyJoules)) * 0.25f;
        explosion.fragmentation.debrisCount = static_cast<u32>(std::clamp(std::sqrt(std::max(0.0f, energyJoules)) * 0.45f, 12.0f, 160.0f));
        return SanitizeExplosionDesc(explosion);
    }

    ExplosionTarget MakeExplosionTargetFromBody(const PhysicsBody& body, const PhysicsScene& scene, DamageTargetKind targetKind)
    {
        ExplosionTarget target{};
        target.bodyId = body.id;
        target.targetKind = targetKind;
        target.bounds = ComputeBodyBounds(scene, body);
        target.position = Center(target.bounds);
        target.massKilograms = SafeMass(body.massKilograms);
        target.dynamicBody = IsDynamic(body);
        target.valid = body.enabled && body.id != 0 && IsValid(target.bounds) && IsFinite(target.position);
        return target;
    }

    std::vector<ExplosionTarget> GatherExplosionTargetsFromPhysicsScene(const PhysicsScene& scene, DamageTargetKind targetKind)
    {
        std::vector<ExplosionTarget> targets;
        targets.reserve(scene.bodies.size());
        for (const PhysicsBody& body : scene.bodies)
        {
            ExplosionTarget target = MakeExplosionTargetFromBody(body, scene, targetKind);
            if (target.valid)
            {
                targets.push_back(target);
            }
        }
        return targets;
    }

    bool IsFinite(const ExplosionDesc& explosion)
    {
        return AK::IsFinite(explosion.center) && AK::IsFinite(explosion.radiusMeters) && AK::IsFinite(explosion.energyJoules) &&
               AK::IsFinite(explosion.impulseScale) && AK::IsFinite(explosion.damageScale) && AK::IsFinite(explosion.heatKelvin) &&
               AK::IsFinite(explosion.upwardBias) && AK::IsFinite(explosion.minimumAttenuation) && AK::IsFinite(explosion.occludedTransmission);
    }

    bool IsFinite(const ExplosionTarget& target)
    {
        return AK::IsFinite(target.position) && IsValid(target.bounds) && AK::IsFinite(target.massKilograms);
    }

    bool IsFinite(const ExplosionHit& hit)
    {
        return AK::IsFinite(hit.point) && AK::IsFinite(hit.normal) && AK::IsFinite(hit.direction) && AK::IsFinite(hit.distanceMeters) &&
               AK::IsFinite(hit.attenuation) && AK::IsFinite(hit.visibility) && AK::IsFinite(hit.impulseNewtonSeconds) && AK::IsFinite(hit.damageAmount);
    }

    ExplosionDesc SanitizeExplosionDesc(ExplosionDesc explosion)
    {
        if (!IsFinite(explosion))
        {
            explosion = {};
        }
        explosion.radiusMeters = std::clamp(explosion.radiusMeters, MinimumRadiusMeters, 1000000.0f);
        explosion.energyJoules = std::clamp(explosion.energyJoules, 0.0f, MaximumEnergyJoules);
        explosion.impulseScale = std::max(0.0f, explosion.impulseScale);
        explosion.damageScale = std::max(0.0f, explosion.damageScale);
        explosion.heatKelvin = std::max(0.0f, explosion.heatKelvin);
        explosion.upwardBias = std::max(0.0f, explosion.upwardBias);
        explosion.minimumAttenuation = std::clamp(explosion.minimumAttenuation, 0.0f, 1.0f);
        explosion.occludedTransmission = std::clamp(explosion.occludedTransmission, 0.0f, 1.0f);
        explosion.fragmentation.debrisCount = std::min(explosion.fragmentation.debrisCount, 4096u);
        explosion.fragmentation.debrisImpulseNewtonSeconds = std::max(0.0f, explosion.fragmentation.debrisImpulseNewtonSeconds);
        explosion.fragmentation.minRadiusMeters = std::max(0.001f, explosion.fragmentation.minRadiusMeters);
        explosion.fragmentation.maxRadiusMeters = std::max(explosion.fragmentation.minRadiusMeters, explosion.fragmentation.maxRadiusMeters);
        return explosion;
    }

    float EvaluateExplosionFalloff(float normalizedDistance, ExplosionFalloffKind kind)
    {
        const float t = std::clamp(normalizedDistance, 0.0f, 1.0f);
        switch (kind)
        {
        case ExplosionFalloffKind::Linear:
            return 1.0f - t;
        case ExplosionFalloffKind::InverseSquare:
        {
            const float d = std::max(0.08f, t);
            const float value = 1.0f / (1.0f + 12.0f * d * d);
            const float edge = 1.0f / 13.0f;
            return std::clamp((value - edge) / (1.0f - edge), 0.0f, 1.0f);
        }
        case ExplosionFalloffKind::Smoothstep:
        default:
        {
            const float smooth = t * t * (3.0f - 2.0f * t);
            return 1.0f - smooth;
        }
        }
    }

    float EvaluateExplosionVisibility(const PhysicsScene& scene, const ExplosionDesc& rawExplosion, const ExplosionTarget& target, bool* outOccluded)
    {
        ExplosionDesc explosion = SanitizeExplosionDesc(rawExplosion);
        if (outOccluded)
        {
            *outOccluded = false;
        }
        if (explosion.occlusion == ExplosionOcclusionPolicy::None)
        {
            return 1.0f;
        }

        const Vec3 targetPoint = ClosestPointOnAABB(target.bounds, explosion.center);
        const Vec3 delta = Subtract(targetPoint, explosion.center);
        const float distance = Length(delta);
        if (distance <= 0.0001f)
        {
            return 1.0f;
        }
        Ray3 ray{};
        ray.origin = explosion.center;
        ray.direction = Normalize(delta, {0.0f, 1.0f, 0.0f});
        const PhysicsRaycastHit hit = RaycastPhysicsScene(scene, ray, distance + 0.02f, PhysicsQueryFlags::Default);
        if (!hit.hit)
        {
            return 1.0f;
        }
        if (hit.bodyId == target.bodyId || hit.distance >= distance - 0.03f)
        {
            return 1.0f;
        }
        if (outOccluded)
        {
            *outOccluded = true;
        }
        return explosion.occludedTransmission;
    }

    ExplosionHit ComputeExplosionHit(const PhysicsScene& scene, const ExplosionDesc& rawExplosion, const ExplosionTarget& target)
    {
        ExplosionDesc explosion = SanitizeExplosionDesc(rawExplosion);
        ExplosionHit hit{};
        hit.bodyId = target.bodyId;
        hit.targetKind = target.targetKind;
        if (!target.valid || !IsFinite(target))
        {
            return hit;
        }

        hit.point = ClosestPointOnAABB(target.bounds, explosion.center);
        Vec3 delta = Subtract(hit.point, explosion.center);
        hit.distanceMeters = Length(delta);
        if (hit.distanceMeters <= 0.0001f)
        {
            hit.point = target.position;
            delta = Subtract(hit.point, explosion.center);
            hit.distanceMeters = Length(delta);
        }
        hit.direction = DirectionFromExplosion(explosion.center, hit.point, explosion.upwardBias);
        hit.normal = hit.direction;

        if (hit.distanceMeters > explosion.radiusMeters)
        {
            return hit;
        }

        bool occluded = false;
        hit.visibility = EvaluateExplosionVisibility(scene, explosion, target, &occluded);
        hit.occluded = occluded;
        const float normalizedDistance = hit.distanceMeters / std::max(explosion.radiusMeters, MinimumRadiusMeters);
        hit.attenuation = std::max(explosion.minimumAttenuation, EvaluateExplosionFalloff(normalizedDistance, explosion.falloff) * hit.visibility);
        if (hit.attenuation <= 0.0f)
        {
            return hit;
        }

        const float area = ApproximateTargetArea(target.bounds);
        hit.impulseNewtonSeconds = std::sqrt(std::max(0.0f, explosion.energyJoules)) * 0.08f * area * hit.attenuation * explosion.impulseScale;
        hit.damageAmount = EstimatePeakOverpressureDamage(explosion.energyJoules, explosion.radiusMeters, hit.distanceMeters, hit.attenuation, area) * explosion.damageScale;
        hit.affected = hit.impulseNewtonSeconds > 0.0f || hit.damageAmount > 0.0f;
        hit.damageEvent = MakeExplosionDamageEvent(explosion, hit);
        return hit;
    }

    DamageEvent MakeExplosionDamageEvent(const ExplosionDesc& explosion, const ExplosionHit& hit)
    {
        DamageEvent event = AK::MakeExplosionDamageEvent(explosion.sourceId, hit.bodyId, hit.point, explosion.energyJoules * hit.attenuation, explosion.radiusMeters, hit.targetKind);
        event.normal = hit.normal;
        event.direction = hit.direction;
        event.impulseNewtonSeconds = hit.impulseNewtonSeconds;
        event.amount = hit.damageAmount;
        event.heatKelvin = explosion.heatKelvin * hit.attenuation;
        event.critical = hit.attenuation > 0.65f && !hit.occluded;
        event.valid = hit.affected && hit.bodyId != 0;
        return SanitizeDamageEvent(event);
    }

    DebrisEmitterDesc MakeExplosionDebrisEmitter(const ExplosionDesc& rawExplosion, const ExplosionHit* strongestHit)
    {
        ExplosionDesc explosion = SanitizeExplosionDesc(rawExplosion);
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 origin = explosion.center;
        if (strongestHit)
        {
            normal = strongestHit->direction;
            origin = strongestHit->point;
        }
        DebrisEmitterDesc emitter = MakeImpactDebrisEmitter(origin, normal, explosion.fragmentation.debrisImpulseNewtonSeconds, explosion.fragmentation.material);
        emitter.mode = DebrisSpawnMode::Explosion;
        emitter.count = explosion.fragmentation.debrisCount;
        emitter.coneAngleDegrees = explosion.fragmentation.coneAngleDegrees;
        emitter.minRadiusMeters = explosion.fragmentation.minRadiusMeters;
        emitter.maxRadiusMeters = explosion.fragmentation.maxRadiusMeters;
        emitter.baseVelocity = Multiply(normal, std::sqrt(std::max(0.0f, explosion.energyJoules)) * 0.015f);
        emitter.randomSeed = StableSeedFromExplosion(explosion);
        emitter.sourceBodyId = explosion.sourceId;
        return emitter;
    }

    ExplosionResult ApplyExplosion(PhysicsScene& scene, const ExplosionDesc& rawExplosion, const std::vector<ExplosionTarget>& targets, DamageSystem* damage, DebrisSystem* debris)
    {
        ExplosionDesc explosion = SanitizeExplosionDesc(rawExplosion);
        ExplosionResult result{};
        result.testedTargetCount = targets.size();
        result.finite = IsFinite(explosion);
        if (!result.finite || explosion.energyJoules <= 0.0f || explosion.radiusMeters <= 0.0f)
        {
            AddWarning(result.warnings, "invalid explosion input");
            return result;
        }

        const ExplosionHit* strongestHit = nullptr;
        float strongestAttenuation = -1.0f;
        result.hits.reserve(targets.size());
        for (const ExplosionTarget& target : targets)
        {
            ExplosionHit hit = ComputeExplosionHit(scene, explosion, target);
            result.finite = result.finite && IsFinite(hit);
            result.maxDistanceMeters = std::max(result.maxDistanceMeters, hit.distanceMeters);
            if (!hit.affected)
            {
                continue;
            }

            result.hits.push_back(hit);
            ExplosionHit& stored = result.hits.back();
            ++result.affectedTargetCount;
            if (stored.occluded)
            {
                ++result.occludedTargetCount;
            }
            result.totalImpulseNewtonSeconds += stored.impulseNewtonSeconds;
            result.totalDamage += stored.damageAmount;

            if (stored.attenuation > strongestAttenuation)
            {
                strongestAttenuation = stored.attenuation;
                strongestHit = &stored;
            }

            if (explosion.applyImpulse && stored.impulseNewtonSeconds > 0.0f)
            {
                if (PhysicsBody* body = FindBody(scene, stored.bodyId))
                {
                    if (IsDynamic(*body))
                    {
                        const Vec3 deltaVelocity = Multiply(stored.direction, SafeDivide(stored.impulseNewtonSeconds, SafeMass(body->massKilograms)));
                        body->velocity = Add(body->velocity, deltaVelocity);
                        if (explosion.wakeBodies)
                        {
                            body->sleeping = false;
                            body->sleepTimerSeconds = 0.0f;
                        }
                        ++result.impulseAppliedCount;
                    }
                }
            }

            if (explosion.queueDamage && damage)
            {
                if (QueueDamageEvent(*damage, stored.damageEvent))
                {
                    ++result.damageQueuedCount;
                }
            }
        }

        if (explosion.spawnDebris && explosion.fragmentation.enabled && debris && result.affectedTargetCount > 0)
        {
            const DebrisEmitterDesc emitter = MakeExplosionDebrisEmitter(explosion, strongestHit);
            const DebrisSpawnResult spawn = SpawnDebris(*debris, emitter);
            result.debrisSpawnedCount = spawn.spawned;
            result.finite = result.finite && spawn.ok;
            if (!spawn.ok)
            {
                AddWarning(result.warnings, "debris spawn clipped or failed");
            }
        }

        result.finite = result.finite && AK::IsFinite(result.totalImpulseNewtonSeconds) && AK::IsFinite(result.totalDamage);
        return result;
    }

    std::string ToDebugString(const ExplosionDesc& explosion)
    {
        std::ostringstream out;
        out << "explosion center=" << ToDebugString(explosion.center, 2)
            << " radius=" << explosion.radiusMeters
            << " energy=" << explosion.energyJoules
            << " falloff=" << ToString(explosion.falloff)
            << " occlusion=" << ToString(explosion.occlusion)
            << " impulse=" << (explosion.applyImpulse ? "true" : "false")
            << " damage=" << (explosion.queueDamage ? "true" : "false")
            << " debris=" << (explosion.spawnDebris ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const ExplosionHit& hit)
    {
        std::ostringstream out;
        out << "explosion_hit body=" << hit.bodyId
            << " affected=" << (hit.affected ? "true" : "false")
            << " distance=" << hit.distanceMeters
            << " attenuation=" << hit.attenuation
            << " visibility=" << hit.visibility
            << " occluded=" << (hit.occluded ? "true" : "false")
            << " impulse=" << hit.impulseNewtonSeconds
            << " damage=" << hit.damageAmount
            << " dir=" << ToDebugString(hit.direction, 2);
        return out.str();
    }

    std::string ToDebugString(const ExplosionResult& result)
    {
        std::ostringstream out;
        out << "explosion_result tested=" << result.testedTargetCount
            << " affected=" << result.affectedTargetCount
            << " occluded=" << result.occludedTargetCount
            << " impulses=" << result.impulseAppliedCount
            << " damage_events=" << result.damageQueuedCount
            << " debris=" << result.debrisSpawnedCount
            << " total_impulse=" << result.totalImpulseNewtonSeconds
            << " total_damage=" << result.totalDamage
            << " hits=" << result.hits.size()
            << " finite=" << (result.finite ? "true" : "false")
            << " warnings=" << result.warnings.size();
        return out.str();
    }

    ExplosionProbeResult BuildExplosionProbe()
    {
        ExplosionProbeResult probe{};
        probe.scene.config.enableSpatialBroadphase = true;
        probe.scene.config.enableContactResolution = true;
        probe.scene.bodies.push_back(MakeStaticBody(1, {0.0f, -0.55f, 0.0f}));
        probe.scene.colliders.push_back(MakeBoxCollider(1, {6.0f, 0.5f, 6.0f}));
        probe.scene.colliders.back().filter.layerMask = PhysicsLayer_Static;

        probe.scene.bodies.push_back(MakeDynamicBody(2, {1.25f, 0.25f, 0.0f}, 35.0f));
        probe.scene.colliders.push_back(MakeBoxCollider(2, {0.35f, 0.35f, 0.35f}));
        probe.scene.colliders.back().filter.layerMask = PhysicsLayer_Dynamic;

        probe.scene.bodies.push_back(MakeDynamicBody(3, {3.25f, 0.35f, 0.0f}, 90.0f));
        probe.scene.colliders.push_back(MakeBoxCollider(3, {0.45f, 0.45f, 0.45f}));
        probe.scene.colliders.back().filter.layerMask = PhysicsLayer_Destructible;

        probe.scene.bodies.push_back(MakeStaticBody(4, {2.15f, 0.35f, 0.0f}));
        probe.scene.colliders.push_back(MakeBoxCollider(4, {0.08f, 0.8f, 1.5f}));
        probe.scene.colliders.back().filter.layerMask = PhysicsLayer_Static;

        probe.scene.bodies.push_back(MakeDynamicBody(5, {-1.7f, 0.3f, 0.35f}, 18.0f));
        probe.scene.colliders.push_back(MakeSphereCollider(5, 0.3f));
        probe.scene.colliders.back().filter.layerMask = PhysicsLayer_Dynamic;

        DamageResistanceProfile concrete{};
        concrete.explosionScale = 0.85f;
        concrete.hardness = 1.4f;
        concrete.brittleness = 0.75f;
        concrete.armorJoules = 10.0f;
        DamageableComponent nearBody = MakeDamageableBody(2, 120.0f, concrete);
        nearBody.fractureThreshold = 18.0f;
        DamageableComponent occludedBody = MakeDamageableBody(3, 160.0f, concrete);
        occludedBody.fractureThreshold = 24.0f;
        DamageableComponent sphereBody = MakeDamageableBody(5, 80.0f, concrete);
        sphereBody.fractureThreshold = 14.0f;
        probe.targets.push_back(nearBody);
        probe.targets.push_back(occludedBody);
        probe.targets.push_back(sphereBody);

        probe.damage = MakeDamageSystem();
        probe.debris = MakeDebrisSystem();
        probe.debris.config.maxParticles = 512;

        probe.explosion = MakeExplosionDesc({0.0f, 0.2f, 0.0f}, 4.0f, 38000.0f, 9001u);
        probe.explosion.fragmentation.debrisCount = 36;
        probe.explosion.occludedTransmission = 0.2f;
        std::vector<ExplosionTarget> targets = GatherExplosionTargetsFromPhysicsScene(probe.scene);
        targets.erase(std::remove_if(targets.begin(), targets.end(), [](const ExplosionTarget& target)
        {
            return target.bodyId == 1u || target.bodyId == 4u;
        }), targets.end());

        probe.explosionResult = ApplyExplosion(probe.scene, probe.explosion, targets, &probe.damage, &probe.debris);
        probe.damageStats = ProcessDamageEvents(probe.damage, probe.targets, &probe.scene, &probe.debris);
        probe.physicsStats = StepPhysics(probe.scene, 1.0f / 60.0f);

        probe.ok = probe.explosionResult.finite && probe.damageStats.finite && probe.physicsStats.finite &&
                   probe.explosionResult.affectedTargetCount >= 2 && probe.explosionResult.damageQueuedCount >= 2 &&
                   probe.explosionResult.impulseAppliedCount >= 2 && probe.damageStats.appliedEventCount >= 2 &&
                   probe.debris.particles.size() >= probe.explosion.fragmentation.debrisCount;

        std::ostringstream out;
        out << "[ ok ] explosion/shockwave/blast foundation"
            << " affected=" << probe.explosionResult.affectedTargetCount
            << " occluded=" << probe.explosionResult.occludedTargetCount
            << " impulses=" << probe.explosionResult.impulseAppliedCount
            << " damage_events=" << probe.explosionResult.damageQueuedCount
            << " applied=" << probe.damageStats.appliedEventCount
            << " fractured=" << probe.damageStats.fracturedCount
            << " debris=" << probe.debris.particles.size()
            << " total_impulse=" << probe.explosionResult.totalImpulseNewtonSeconds
            << " total_damage=" << probe.explosionResult.totalDamage
            << " physics_contacts=" << probe.physicsStats.contactCount
            << " finite=" << (probe.explosionResult.finite && probe.damageStats.finite && probe.physicsStats.finite ? "true" : "false")
            << " warnings=" << (probe.explosionResult.warnings.size() + probe.damageStats.warnings.size() + probe.physicsStats.warnings.size());
        probe.summary = out.str();
        if (!probe.ok)
        {
            const std::size_t pos = probe.summary.find("[ ok ]");
            if (pos != std::string::npos)
            {
                probe.summary.replace(pos, 6, "[fail]");
            }
        }
        return probe;
    }

    std::string BuildExplosionProbeSummary()
    {
        return BuildExplosionProbe().summary;
    }
}
