#include <AK/Damage/Damage.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr float MinimumHealth = 0.0f;
        constexpr float MaximumDamage = 1000000.0f;

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }

        DamageableComponent* FindDamageable(std::vector<DamageableComponent>& targets, u32 targetId, DamageTargetKind kind)
        {
            for (DamageableComponent& target : targets)
            {
                if (target.targetId == targetId && target.targetKind == kind)
                {
                    return &target;
                }
            }
            for (DamageableComponent& target : targets)
            {
                if (target.targetId == targetId)
                {
                    return &target;
                }
            }
            return nullptr;
        }

        float ClampDamage(float value)
        {
            if (!AK::IsFinite(value))
            {
                return 0.0f;
            }
            return std::clamp(value, 0.0f, MaximumDamage);
        }

        float SafeMass(const PhysicsBody* body)
        {
            if (!body)
            {
                return 1.0f;
            }
            return std::max(body->massKilograms, 0.001f);
        }

        Vec3 RelativeVelocity(const PhysicsBody* a, const PhysicsBody* b)
        {
            const Vec3 va = a ? a->velocity : Vec3{};
            const Vec3 vb = b ? b->velocity : Vec3{};
            return Subtract(va, vb);
        }
    }

    const char* ToString(DamageKind kind)
    {
        switch (kind)
        {
        case DamageKind::Generic: return "Generic";
        case DamageKind::ProjectileImpact: return "ProjectileImpact";
        case DamageKind::Penetration: return "Penetration";
        case DamageKind::Ricochet: return "Ricochet";
        case DamageKind::CollisionImpulse: return "CollisionImpulse";
        case DamageKind::Explosion: return "Explosion";
        case DamageKind::DebrisImpact: return "DebrisImpact";
        case DamageKind::FluidContact: return "FluidContact";
        case DamageKind::Fire: return "Fire";
        case DamageKind::Heat: return "Heat";
        case DamageKind::Crush: return "Crush";
        default: return "Unknown";
        }
    }

    const char* ToString(DamageTargetKind kind)
    {
        switch (kind)
        {
        case DamageTargetKind::Unknown: return "Unknown";
        case DamageTargetKind::PhysicsBody: return "PhysicsBody";
        case DamageTargetKind::DestructibleVoxel: return "DestructibleVoxel";
        case DamageTargetKind::Character: return "Character";
        case DamageTargetKind::Vehicle: return "Vehicle";
        case DamageTargetKind::Ragdoll: return "Ragdoll";
        case DamageTargetKind::SoftBody: return "SoftBody";
        case DamageTargetKind::Debris: return "Debris";
        default: return "Unknown";
        }
    }

    const char* ToString(DamageResponseKind kind)
    {
        switch (kind)
        {
        case DamageResponseKind::None: return "None";
        case DamageResponseKind::Hit: return "Hit";
        case DamageResponseKind::Fractured: return "Fractured";
        case DamageResponseKind::Destroyed: return "Destroyed";
        case DamageResponseKind::Ignited: return "Ignited";
        case DamageResponseKind::Melted: return "Melted";
        default: return "Unknown";
        }
    }

    DamageResistanceProfile MakeDamageResistanceFromSurface(const SurfaceMaterialDesc& surface)
    {
        DamageResistanceProfile resistance{};
        resistance.hardness = std::max(surface.physics.hardness, 0.05f);
        resistance.armorJoules = surface.physics.densityKgPerCubicMeter * surface.physics.destructionResistance * 0.015f;
        resistance.projectileScale = 1.0f / std::max(surface.physics.destructionResistance, 0.1f);
        resistance.penetrationScale = 1.15f / std::max(surface.physics.destructionResistance, 0.1f);
        resistance.collisionScale = 0.85f / std::max(surface.physics.hardness, 0.1f);
        resistance.explosionScale = 1.0f / std::max(surface.physics.destructionResistance, 0.1f);
        resistance.debrisScale = 0.65f / std::max(surface.physics.hardness, 0.1f);
        resistance.fluidScale = (surface.flags & SurfaceMaterialFlag_Fluid) != 0 ? 0.25f : 1.0f;
        const bool organic = surface.kind == SurfaceMaterialKind::Wood || surface.kind == SurfaceMaterialKind::Grass || surface.kind == SurfaceMaterialKind::TerrainSoil;
        resistance.heatScale = organic ? 1.35f : 0.65f;
        resistance.fireScale = organic ? 1.6f : 0.4f;
        resistance.ductility = surface.kind == SurfaceMaterialKind::Metal || surface.kind == SurfaceMaterialKind::Rubber ? 0.75f : 0.35f;
        resistance.brittleness = surface.kind == SurfaceMaterialKind::Glass || surface.kind == SurfaceMaterialKind::Rock || surface.kind == SurfaceMaterialKind::Concrete ? 0.8f : 0.35f;
        return resistance;
    }

    DamageResistanceProfile MakeDamageResistanceFromProjectileMaterial(const ProjectileImpactMaterial& material)
    {
        DamageResistanceProfile resistance{};
        resistance.hardness = std::max(material.yieldStrengthPascals / 250.0e6f, 0.05f);
        resistance.armorJoules = material.yieldStrengthPascals * std::max(material.thicknessOverrideMeters, 0.01f) * 0.000001f;
        resistance.projectileScale = 1.0f / std::max(material.destructionResistance, 0.1f);
        resistance.penetrationScale = 1.25f / std::max(material.destructionResistance, 0.1f);
        resistance.collisionScale = 0.8f / std::max(resistance.hardness, 0.1f);
        resistance.explosionScale = 1.0f / std::max(material.destructionResistance, 0.1f);
        resistance.debrisScale = 0.7f / std::max(resistance.hardness, 0.1f);
        resistance.heatScale = material.softNoRicochet ? 1.2f : 0.75f;
        resistance.fireScale = material.softNoRicochet ? 1.25f : 0.55f;
        resistance.ductility = material.softNoRicochet ? 0.85f : 0.35f;
        resistance.brittleness = material.softNoRicochet ? 0.15f : 0.65f;
        return resistance;
    }

    DamageableComponent MakeDamageableBody(u32 bodyId, float health, DamageResistanceProfile resistance)
    {
        DamageableComponent component{};
        component.targetId = bodyId;
        component.targetKind = DamageTargetKind::PhysicsBody;
        component.maxHealth = std::max(health, 0.001f);
        component.health = component.maxHealth;
        component.structuralIntegrity = 100.0f;
        component.fractureThreshold = std::max(component.maxHealth * 0.35f, 1.0f);
        component.resistance = resistance;
        return component;
    }

    DamageSystem MakeDamageSystem(DamageSystemConfig config)
    {
        DamageSystem system{};
        system.config = config;
        system.queue.reserve(std::min<std::size_t>(config.maxQueuedEvents, 1024));
        system.results.reserve(256);
        return system;
    }

    bool IsFinite(const DamageResistanceProfile& resistance)
    {
        return AK::IsFinite(resistance.projectileScale) && AK::IsFinite(resistance.penetrationScale) && AK::IsFinite(resistance.collisionScale) &&
               AK::IsFinite(resistance.explosionScale) && AK::IsFinite(resistance.debrisScale) && AK::IsFinite(resistance.fluidScale) &&
               AK::IsFinite(resistance.heatScale) && AK::IsFinite(resistance.fireScale) && AK::IsFinite(resistance.armorJoules) &&
               AK::IsFinite(resistance.hardness) && AK::IsFinite(resistance.ductility) && AK::IsFinite(resistance.brittleness);
    }

    bool IsFinite(const DamageableComponent& damageable)
    {
        return AK::IsFinite(damageable.maxHealth) && AK::IsFinite(damageable.health) && AK::IsFinite(damageable.structuralIntegrity) &&
               AK::IsFinite(damageable.fractureThreshold) && AK::IsFinite(damageable.accumulatedHeatKelvin) && IsFinite(damageable.resistance);
    }

    bool IsFinite(const DamageEvent& event)
    {
        return AK::IsFinite(event.point) && AK::IsFinite(event.normal) && AK::IsFinite(event.direction) && AK::IsFinite(event.velocity) &&
               AK::IsFinite(event.amount) && AK::IsFinite(event.kineticEnergyJoules) && AK::IsFinite(event.impulseNewtonSeconds) &&
               AK::IsFinite(event.radiusMeters) && AK::IsFinite(event.heatKelvin) && AK::IsFinite(event.damagePerSecond) &&
               AK::IsFinite(event.deltaSeconds) && AK::IsFinite(event.penetrationDepthMeters) && AK::IsFinite(event.materialHardness) &&
               AK::IsFinite(event.materialDensityKgPerCubicMeter);
    }

    DamageEvent SanitizeDamageEvent(DamageEvent event)
    {
        if (!IsFinite(event))
        {
            event.valid = false;
            return event;
        }
        event.normal = Normalize(event.normal, {0.0f, 1.0f, 0.0f});
        event.direction = Normalize(event.direction, {0.0f, 0.0f, 1.0f});
        event.amount = ClampDamage(event.amount);
        event.kineticEnergyJoules = ClampDamage(event.kineticEnergyJoules);
        event.impulseNewtonSeconds = std::max(0.0f, event.impulseNewtonSeconds);
        event.radiusMeters = std::max(0.0f, event.radiusMeters);
        event.heatKelvin = std::max(0.0f, event.heatKelvin);
        event.damagePerSecond = std::max(0.0f, event.damagePerSecond);
        event.deltaSeconds = std::max(0.0f, event.deltaSeconds);
        event.penetrationDepthMeters = std::max(0.0f, event.penetrationDepthMeters);
        event.materialHardness = std::max(0.05f, event.materialHardness);
        event.materialDensityKgPerCubicMeter = std::max(0.0f, event.materialDensityKgPerCubicMeter);
        event.valid = event.targetId != 0 || event.targetKind != DamageTargetKind::Unknown;
        return event;
    }

    DamageEvent MakeProjectileDamageEvent(u32 targetId, const ProjectileBody& projectile, const ProjectileImpactResult& impact, DamageTargetKind targetKind)
    {
        DamageEvent event{};
        event.kind = impact.outcome == ProjectileImpactOutcome::Penetration ? DamageKind::Penetration :
                     impact.outcome == ProjectileImpactOutcome::Ricochet ? DamageKind::Ricochet : DamageKind::ProjectileImpact;
        event.targetKind = targetKind;
        event.targetId = targetId;
        event.point = impact.outPosition;
        event.normal = Normalize(Multiply(impact.outVelocity, -1.0f), {0.0f, 1.0f, 0.0f});
        event.direction = Normalize(projectile.forward, {0.0f, 0.0f, 1.0f});
        event.velocity = projectile.velocity;
        event.kineticEnergyJoules = std::max(impact.kineticEnergyJoules, ComputeProjectileKineticEnergyJoules(projectile));
        event.impulseNewtonSeconds = projectile.massKilograms * Length(projectile.velocity);
        event.amount = std::max(impact.damagePercent, event.kineticEnergyJoules * 0.02f);
        event.penetrationDepthMeters = std::max(impact.embedDepthMeters, impact.penetrationEnergyJoules > 0.0f ? 0.05f : 0.0f);
        event.heatKelvin = std::max(0.0f, projectile.temperatureKelvin - projectile.ambientReferenceTemperatureKelvin);
        event.materialHardness = 1.0f;
        event.critical = impact.outcome == ProjectileImpactOutcome::Penetration || impact.damagePercent >= 50.0f;
        event.valid = true;
        return SanitizeDamageEvent(event);
    }

    DamageEvent MakePhysicsCollisionDamageEvent(const PhysicsScene& scene, const PhysicsEvent& physicsEvent, float energyToDamageScale)
    {
        const PhysicsBody* bodyA = FindBody(scene, physicsEvent.bodyA);
        const PhysicsBody* bodyB = FindBody(scene, physicsEvent.bodyB);
        const Vec3 relativeVelocity = RelativeVelocity(bodyA, bodyB);
        const float speed = Length(relativeVelocity);
        const float reducedMass = (SafeMass(bodyA) * SafeMass(bodyB)) / std::max(SafeMass(bodyA) + SafeMass(bodyB), 0.001f);
        DamageEvent event{};
        event.kind = DamageKind::CollisionImpulse;
        event.targetKind = DamageTargetKind::PhysicsBody;
        event.sourceId = physicsEvent.bodyA;
        event.targetId = physicsEvent.bodyB;
        event.point = physicsEvent.point;
        event.normal = physicsEvent.normal;
        event.direction = Normalize(relativeVelocity, physicsEvent.normal);
        event.velocity = relativeVelocity;
        event.kineticEnergyJoules = 0.5f * reducedMass * speed * speed;
        event.impulseNewtonSeconds = reducedMass * speed;
        event.amount = event.kineticEnergyJoules * std::max(energyToDamageScale, 0.0f);
        event.valid = physicsEvent.bodyB != 0 && event.amount > 0.0f;
        return SanitizeDamageEvent(event);
    }

    DamageEvent MakeDebrisDamageEvent(u32 targetId, const DebrisParticle& particle, Vec3 contactPoint, Vec3 normal)
    {
        DamageEvent event{};
        event.kind = DamageKind::DebrisImpact;
        event.targetKind = DamageTargetKind::PhysicsBody;
        event.sourceId = particle.id;
        event.targetId = targetId;
        event.point = contactPoint;
        event.normal = normal;
        event.direction = Normalize(particle.velocity, {0.0f, -1.0f, 0.0f});
        event.velocity = particle.velocity;
        const float speed = Length(particle.velocity);
        event.kineticEnergyJoules = 0.5f * particle.massKilograms * speed * speed;
        event.impulseNewtonSeconds = particle.massKilograms * speed;
        event.amount = event.kineticEnergyJoules * 0.05f;
        event.radiusMeters = particle.radius;
        event.materialDensityKgPerCubicMeter = SafeDivide(particle.massKilograms, std::max(4.0f / 3.0f * Pi32 * particle.radius * particle.radius * particle.radius, 0.000001f), 1000.0f);
        event.valid = particle.state == DebrisParticleState::Active && targetId != 0;
        return SanitizeDamageEvent(event);
    }

    DamageEvent MakeFluidDamageEvent(const FluidColliderInteraction& interaction, float deltaSeconds)
    {
        DamageEvent event{};
        event.kind = DamageKind::FluidContact;
        event.targetKind = DamageTargetKind::PhysicsBody;
        event.targetId = interaction.bodyId;
        event.point = interaction.applicationPoint;
        event.normal = Normalize(Multiply(interaction.totalForce, -1.0f), {0.0f, 1.0f, 0.0f});
        event.direction = Normalize(interaction.flowVelocity, {0.0f, 0.0f, 1.0f});
        event.velocity = interaction.flowVelocity;
        event.amount = interaction.damagePerSecond * std::max(deltaSeconds, 0.0f) * std::max(interaction.submergedFraction, 0.0f);
        event.damagePerSecond = interaction.damagePerSecond;
        event.deltaSeconds = deltaSeconds;
        event.heatKelvin = interaction.damagePerSecond > 0.0f ? 550.0f * std::max(interaction.submergedFraction, 0.0f) : 0.0f;
        event.valid = interaction.active && interaction.bodyId != 0 && event.amount > 0.0f;
        return SanitizeDamageEvent(event);
    }

    DamageEvent MakeExplosionDamageEvent(u32 sourceId, u32 targetId, Vec3 point, float energyJoules, float radiusMeters, DamageTargetKind targetKind)
    {
        DamageEvent event{};
        event.kind = DamageKind::Explosion;
        event.targetKind = targetKind;
        event.sourceId = sourceId;
        event.targetId = targetId;
        event.point = point;
        event.normal = {0.0f, 1.0f, 0.0f};
        event.direction = {0.0f, 1.0f, 0.0f};
        event.kineticEnergyJoules = std::max(0.0f, energyJoules);
        event.radiusMeters = std::max(0.0f, radiusMeters);
        event.amount = std::sqrt(std::max(0.0f, energyJoules)) * 0.5f;
        event.critical = energyJoules > 5000.0f;
        event.valid = targetId != 0 && energyJoules > 0.0f;
        return SanitizeDamageEvent(event);
    }

    float EvaluateDamageResistanceScale(const DamageResistanceProfile& resistance, DamageKind kind)
    {
        if (resistance.invulnerable)
        {
            return 0.0f;
        }

        float scale = 1.0f;
        switch (kind)
        {
        case DamageKind::ProjectileImpact: scale = resistance.projectileScale; break;
        case DamageKind::Penetration: scale = resistance.penetrationScale; break;
        case DamageKind::Ricochet: scale = resistance.projectileScale * 0.35f; break;
        case DamageKind::CollisionImpulse: scale = resistance.collisionScale; break;
        case DamageKind::Explosion: scale = resistance.explosionScale; break;
        case DamageKind::DebrisImpact: scale = resistance.debrisScale; break;
        case DamageKind::FluidContact: scale = resistance.fluidScale; break;
        case DamageKind::Fire: scale = resistance.fireScale; break;
        case DamageKind::Heat: scale = resistance.heatScale; break;
        case DamageKind::Crush: scale = resistance.collisionScale * 1.3f; break;
        case DamageKind::Generic: default: scale = 1.0f; break;
        }

        const float hardnessMitigation = 1.0f / std::max(0.25f, std::sqrt(std::max(resistance.hardness, 0.05f)));
        return std::max(0.0f, scale * hardnessMitigation);
    }

    float ComputeRawDamageAmount(const DamageEvent& event, const DamageSystemConfig& config)
    {
        if (!event.valid)
        {
            return 0.0f;
        }

        float raw = event.amount;
        if (raw <= 0.0f)
        {
            raw += event.kineticEnergyJoules * config.impactEnergyToDamage;
            raw += event.impulseNewtonSeconds * config.impulseToDamage;
            raw += event.damagePerSecond * event.deltaSeconds;
        }

        if (event.kind == DamageKind::Heat || event.kind == DamageKind::Fire || event.kind == DamageKind::FluidContact)
        {
            raw += event.heatKelvin * config.heatDamageScale;
        }

        if (event.critical)
        {
            raw *= 1.25f;
        }
        return ClampDamage(raw);
    }

    DamageApplicationResult ApplyDamageEvent(DamageableComponent& target, const DamageEvent& rawEvent, const DamageSystemConfig& config)
    {
        DamageApplicationResult result{};
        DamageEvent event = SanitizeDamageEvent(rawEvent);
        result.kind = event.kind;
        result.targetId = target.targetId;
        result.targetFound = true;
        result.finite = IsFinite(event) && IsFinite(target);
        if (!result.finite)
        {
            AddWarning(result.warnings, "non-finite damage input");
            return result;
        }
        if (!target.enabled || target.destroyed || target.resistance.invulnerable || !event.valid)
        {
            return result;
        }

        result.rawDamage = ComputeRawDamageAmount(event, config);
        const float resistanceScale = EvaluateDamageResistanceScale(target.resistance, event.kind);
        const float armorAbsorption = std::min(result.rawDamage, std::max(0.0f, target.resistance.armorJoules * 0.01f));
        result.mitigatedDamage = std::max(0.0f, result.rawDamage * resistanceScale - armorAbsorption);
        if (result.mitigatedDamage < config.minimumDamage)
        {
            result.mitigatedDamage = 0.0f;
        }

        const float brittlenessBoost = Lerp(0.75f, 1.45f, std::clamp(target.resistance.brittleness, 0.0f, 1.0f));
        const float ductilityAbsorb = Lerp(1.0f, 0.65f, std::clamp(target.resistance.ductility, 0.0f, 1.0f));
        result.structuralDamage = result.mitigatedDamage * config.structuralDamageScale * brittlenessBoost * ductilityAbsorb;
        if (event.kind == DamageKind::Penetration || event.kind == DamageKind::Explosion || event.kind == DamageKind::Crush)
        {
            result.structuralDamage *= 1.35f;
        }

        result.healthBefore = target.health;
        result.structuralBefore = target.structuralIntegrity;
        target.health -= result.mitigatedDamage;
        target.structuralIntegrity -= result.structuralDamage;
        if (config.accumulateHeat)
        {
            result.heatAddedKelvin = event.heatKelvin;
            target.accumulatedHeatKelvin += event.heatKelvin;
        }
        if (config.clampHealth)
        {
            target.health = std::clamp(target.health, MinimumHealth, target.maxHealth);
            target.structuralIntegrity = std::clamp(target.structuralIntegrity, 0.0f, 100.0f);
        }

        result.applied = result.mitigatedDamage > 0.0f || result.heatAddedKelvin > 0.0f;
        result.healthAfter = target.health;
        result.structuralAfter = target.structuralIntegrity;
        result.fractured = !target.fractured && result.structuralDamage >= target.fractureThreshold;
        if (!result.fractured && !target.fractured)
        {
            result.fractured = target.structuralIntegrity <= (100.0f - target.fractureThreshold);
        }
        result.destroyed = target.health <= target.destroyThreshold || target.structuralIntegrity <= 0.0f;
        result.ignited = !target.burning && target.accumulatedHeatKelvin >= target.ignitionThresholdKelvin;
        result.melted = target.accumulatedHeatKelvin >= target.meltThresholdKelvin;
        result.spawnDebris = config.emitDebrisOnFracture && (result.fractured || result.destroyed) && result.structuralDamage >= config.fractureDebrisThreshold;
        result.dirtyBounds = result.fractured || result.destroyed || result.mitigatedDamage > 0.0f;
        result.wakePhysicsBody = config.wakeBodiesOnDamage && result.applied;

        if (result.destroyed)
        {
            target.destroyed = true;
            target.enabled = false;
            result.response = DamageResponseKind::Destroyed;
        }
        else if (result.melted)
        {
            result.response = DamageResponseKind::Melted;
        }
        else if (result.ignited)
        {
            target.burning = true;
            result.response = DamageResponseKind::Ignited;
        }
        else if (result.fractured)
        {
            target.fractured = true;
            result.response = DamageResponseKind::Fractured;
        }
        else if (result.applied)
        {
            result.response = DamageResponseKind::Hit;
        }
        target.dirty = target.dirty || result.dirtyBounds;
        result.finite = IsFinite(target);
        return result;
    }

    bool QueueDamageEvent(DamageSystem& system, DamageEvent event)
    {
        event = SanitizeDamageEvent(event);
        if (!event.valid)
        {
            return false;
        }
        if (system.queue.size() >= system.config.maxQueuedEvents)
        {
            return false;
        }
        system.queue.push_back(event);
        return true;
    }

    DamageSystemStats ProcessDamageEvents(DamageSystem& system, std::vector<DamageableComponent>& targets, PhysicsScene* scene, DebrisSystem* debris)
    {
        DamageSystemStats stats{};
        stats.queuedEventCount = system.queue.size();
        system.results.clear();
        for (const DamageEvent& event : system.queue)
        {
            ++stats.processedEventCount;
            DamageableComponent* target = FindDamageable(targets, event.targetId, event.targetKind);
            if (!target)
            {
                AddWarning(stats.warnings, "damage target not found");
                continue;
            }
            DamageApplicationResult result = ApplyDamageEvent(*target, event, system.config);
            system.results.push_back(result);
            stats.finite = stats.finite && result.finite;
            stats.totalRawDamage += result.rawDamage;
            stats.totalMitigatedDamage += result.mitigatedDamage;
            stats.totalStructuralDamage += result.structuralDamage;
            if (result.applied) { ++stats.appliedEventCount; }
            if (result.fractured) { ++stats.fracturedCount; }
            if (result.destroyed) { ++stats.destroyedCount; }
            if (result.ignited) { ++stats.ignitedCount; }
            if (result.melted) { ++stats.meltedCount; }
            if (result.spawnDebris)
            {
                ++stats.debrisRequestCount;
                if (debris)
                {
                    DebrisEmitterDesc emitter = MakeImpactDebrisEmitter(event.point, event.normal, std::max(2.0f, result.structuralDamage * 0.2f));
                    emitter.count = static_cast<u32>(std::clamp(result.structuralDamage * 0.5f, 4.0f, 32.0f));
                    SpawnDebris(*debris, emitter);
                }
            }
            if (result.wakePhysicsBody)
            {
                ++stats.physicsWakeCount;
                if (scene)
                {
                    if (PhysicsBody* body = FindBody(*scene, event.targetId))
                    {
                        body->sleeping = false;
                        body->sleepTimerSeconds = 0.0f;
                    }
                }
            }
        }
        system.queue.clear();
        system.processedSerial += stats.processedEventCount;
        stats.finite = stats.finite && AK::IsFinite(stats.totalRawDamage) && AK::IsFinite(stats.totalMitigatedDamage) && AK::IsFinite(stats.totalStructuralDamage);
        system.results.shrink_to_fit();
        return stats;
    }

    std::string ToDebugString(const DamageEvent& event)
    {
        std::ostringstream out;
        out << "damage_event kind=" << ToString(event.kind)
            << " target=" << event.targetId
            << " raw=" << event.amount
            << " energy=" << event.kineticEnergyJoules
            << " impulse=" << event.impulseNewtonSeconds
            << " heat=" << event.heatKelvin
            << " valid=" << (event.valid ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const DamageApplicationResult& result)
    {
        std::ostringstream out;
        out << "damage_result target=" << result.targetId
            << " kind=" << ToString(result.kind)
            << " response=" << ToString(result.response)
            << " applied=" << (result.applied ? "true" : "false")
            << " raw=" << result.rawDamage
            << " mitigated=" << result.mitigatedDamage
            << " structural=" << result.structuralDamage
            << " hp=" << result.healthBefore << "->" << result.healthAfter
            << " integrity=" << result.structuralBefore << "->" << result.structuralAfter
            << " debris=" << (result.spawnDebris ? "true" : "false")
            << " wake=" << (result.wakePhysicsBody ? "true" : "false")
            << " finite=" << (result.finite ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const DamageSystemStats& stats)
    {
        std::ostringstream out;
        out << "damage_stats queued=" << stats.queuedEventCount
            << " processed=" << stats.processedEventCount
            << " applied=" << stats.appliedEventCount
            << " fractured=" << stats.fracturedCount
            << " destroyed=" << stats.destroyedCount
            << " ignited=" << stats.ignitedCount
            << " melted=" << stats.meltedCount
            << " debris=" << stats.debrisRequestCount
            << " wakes=" << stats.physicsWakeCount
            << " raw=" << stats.totalRawDamage
            << " mitigated=" << stats.totalMitigatedDamage
            << " structural=" << stats.totalStructuralDamage
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.warnings.size();
        return out.str();
    }

    DamageProbeResult BuildDamageProbe()
    {
        DamageProbeResult probe{};
        probe.scene.config.enableSpatialBroadphase = true;
        probe.scene.bodies.push_back(MakeStaticBody(1, {0.0f, -0.5f, 0.0f}));
        probe.scene.colliders.push_back(MakeBoxCollider(1, {4.0f, 0.5f, 4.0f}));
        probe.scene.bodies.push_back(MakeDynamicBody(2, {0.0f, 0.55f, 0.0f}, 35.0f));
        probe.scene.colliders.push_back(MakeBoxCollider(2, {0.45f, 0.45f, 0.45f}));
        probe.scene.colliders.back().material = MakePhysicsMaterial(0.8f, 0.6f, 0.08f, 2400.0f);
        probe.scene.bodies.back().velocity = {0.0f, -9.0f, 0.0f};
        PhysicsStepStats physicsStats = StepPhysics(probe.scene, 1.0f / 60.0f);

        SurfaceMaterialRegistry registry = BuildDefaultSurfaceMaterialRegistry();
        const SurfaceMaterialDesc* concrete = registry.FindByName("concrete");
        DamageResistanceProfile resistance = concrete ? MakeDamageResistanceFromSurface(*concrete) : DamageResistanceProfile{};
        resistance.armorJoules = 25.0f;
        DamageableComponent destructible = MakeDamageableBody(2, 150.0f, resistance);
        destructible.targetKind = DamageTargetKind::DestructibleVoxel;
        destructible.fractureThreshold = 22.0f;
        destructible.ignitionThresholdKelvin = 500.0f;
        destructible.meltThresholdKelvin = 950.0f;
        probe.targets.push_back(destructible);

        ProjectilePreset preset = MakeProjectilePreset(ProjectileDamageType::Physical, ProjectileWeaponType::SniperRifle);
        probe.projectile = MakeProjectileBody(preset, {-2.0f, 0.4f, 0.0f}, {1.0f, 0.0f, 0.0f});
        ProjectileImpactMaterial material = concrete ? MakeProjectileImpactMaterial(*concrete) : ProjectileImpactMaterial{};
        material.thicknessOverrideMeters = 0.25f;
        ProjectileImpactInput impactInput{};
        impactInput.projectile = probe.projectile;
        impactInput.material = material;
        impactInput.materialDamage = MakeProjectileMaterialDamageState(material);
        impactInput.hitPoint = {0.0f, 0.4f, 0.0f};
        impactInput.surfaceNormal = {-1.0f, 0.0f, 0.0f};
        impactInput.thicknessMeters = 0.25f;
        probe.projectileImpact = ResolveProjectileImpact(impactInput);

        probe.system = MakeDamageSystem();
        QueueDamageEvent(probe.system, MakeProjectileDamageEvent(2, probe.projectile, probe.projectileImpact, DamageTargetKind::DestructibleVoxel));
        if (!probe.scene.events.empty())
        {
            DamageEvent collisionDamage = MakePhysicsCollisionDamageEvent(probe.scene, probe.scene.events.front());
            collisionDamage.targetKind = DamageTargetKind::DestructibleVoxel;
            collisionDamage.targetId = 2;
            QueueDamageEvent(probe.system, collisionDamage);
        }

        probe.fluidInteraction.active = true;
        probe.fluidInteraction.bodyId = 2;
        probe.fluidInteraction.submergedFraction = 0.7f;
        probe.fluidInteraction.damagePerSecond = 35.0f;
        probe.fluidInteraction.applicationPoint = {0.0f, 0.0f, 0.0f};
        probe.fluidInteraction.totalForce = {0.0f, 1200.0f, 0.0f};
        QueueDamageEvent(probe.system, MakeFluidDamageEvent(probe.fluidInteraction, 0.25f));

        probe.debris = MakeDebrisSystem();
        probe.stats = ProcessDamageEvents(probe.system, probe.targets, &probe.scene, &probe.debris);
        probe.results = probe.system.results;
        probe.ok = probe.stats.finite && probe.stats.processedEventCount >= 2 && probe.stats.appliedEventCount >= 1 && probe.stats.fracturedCount >= 1 && !probe.targets.empty() && probe.targets.front().dirty && physicsStats.finite;

        std::ostringstream out;
        out << "[ ok ] damage/health/breakage foundation"
            << " events=" << probe.stats.processedEventCount
            << " applied=" << probe.stats.appliedEventCount
            << " fractured=" << probe.stats.fracturedCount
            << " destroyed=" << probe.stats.destroyedCount
            << " debris_requests=" << probe.stats.debrisRequestCount
            << " debris_particles=" << probe.debris.particles.size()
            << " wakes=" << probe.stats.physicsWakeCount
            << " health=" << (probe.targets.empty() ? 0.0f : probe.targets.front().health)
            << " integrity=" << (probe.targets.empty() ? 0.0f : probe.targets.front().structuralIntegrity)
            << " projectile=" << ToString(probe.projectileImpact.outcome)
            << " finite=" << (probe.stats.finite ? "true" : "false")
            << " warnings=" << probe.stats.warnings.size();
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

    std::string BuildDamageProbeSummary()
    {
        return BuildDamageProbe().summary;
    }
}
