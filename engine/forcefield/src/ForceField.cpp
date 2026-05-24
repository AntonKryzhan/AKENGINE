#include <AK/ForceField/ForceField.hpp>

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
        constexpr float MinimumDeltaSeconds = 1.0e-6f;
        constexpr float MinimumRadius = 1.0e-4f;
        constexpr float MaximumForce = 1.0e8f;

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

        float SmoothStep(float value)
        {
            const float t = Saturate(value);
            return t * t * (3.0f - 2.0f * t);
        }

        float EvaluateFalloff(ForceFieldFalloffKind kind, float distance, float innerRadius, float radius)
        {
            if (kind == ForceFieldFalloffKind::Constant || radius <= MinimumRadius)
            {
                return 1.0f;
            }

            if (distance <= innerRadius)
            {
                return 1.0f;
            }

            const float range = std::max(MinimumRadius, radius - innerRadius);
            const float normalized = std::clamp((radius - distance) / range, 0.0f, 1.0f);
            if (kind == ForceFieldFalloffKind::Linear)
            {
                return normalized;
            }
            if (kind == ForceFieldFalloffKind::SmoothStep)
            {
                return SmoothStep(normalized);
            }

            const float inverse = 1.0f / std::max(1.0f, distance * distance);
            const float inner = 1.0f / std::max(1.0f, innerRadius * innerRadius);
            return std::clamp(SafeDivide(inverse, inner, normalized), 0.0f, 1.0f);
        }

        bool AnyLayerMatches(u32 layerMask, u32 affectMask)
        {
            return (layerMask & affectMask) != 0u;
        }

        const PhysicsCollider* FindFirstColliderForBody(const PhysicsScene& scene, u32 bodyId, std::size_t* outIndex)
        {
            for (std::size_t i = 0; i < scene.colliders.size(); ++i)
            {
                const PhysicsCollider& collider = scene.colliders[i];
                if (collider.enabled && collider.bodyId == bodyId)
                {
                    if (outIndex)
                    {
                        *outIndex = i;
                    }
                    return &collider;
                }
            }
            if (outIndex)
            {
                *outIndex = std::numeric_limits<std::size_t>::max();
            }
            return nullptr;
        }

        Vec3 BodySamplePoint(const PhysicsScene& scene, const PhysicsBody& body, const PhysicsCollider* collider)
        {
            (void)scene;
            if (!collider)
            {
                return body.position;
            }

            const AABB3 bounds = ComputeColliderWorldBounds(body, *collider);
            if (IsValid(bounds))
            {
                return Center(bounds);
            }
            return body.position;
        }

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }
    }

    const char* ToString(ForceFieldKind kind)
    {
        switch (kind)
        {
        case ForceFieldKind::DirectionalAcceleration: return "DirectionalAcceleration";
        case ForceFieldKind::DirectionalForce: return "DirectionalForce";
        case ForceFieldKind::PointAttractor: return "PointAttractor";
        case ForceFieldKind::PointRepulsor: return "PointRepulsor";
        case ForceFieldKind::Vortex: return "Vortex";
        case ForceFieldKind::LinearDrag: return "LinearDrag";
        case ForceFieldKind::Wind: return "Wind";
        case ForceFieldKind::DamageOverTime: return "DamageOverTime";
        default: return "Unknown";
        }
    }

    const char* ToString(ForceFieldVolumeKind kind)
    {
        switch (kind)
        {
        case ForceFieldVolumeKind::Global: return "Global";
        case ForceFieldVolumeKind::Sphere: return "Sphere";
        case ForceFieldVolumeKind::Box: return "Box";
        case ForceFieldVolumeKind::HalfSpace: return "HalfSpace";
        default: return "Unknown";
        }
    }

    const char* ToString(ForceFieldFalloffKind kind)
    {
        switch (kind)
        {
        case ForceFieldFalloffKind::Constant: return "Constant";
        case ForceFieldFalloffKind::Linear: return "Linear";
        case ForceFieldFalloffKind::SmoothStep: return "SmoothStep";
        case ForceFieldFalloffKind::InverseSquare: return "InverseSquare";
        default: return "Unknown";
        }
    }

    ForceFieldDesc MakeGlobalAccelerationField(u32 id, Vec3 acceleration, std::string debugName)
    {
        ForceFieldDesc field{};
        field.id = id;
        field.kind = ForceFieldKind::DirectionalAcceleration;
        field.volumeKind = ForceFieldVolumeKind::Global;
        field.falloffKind = ForceFieldFalloffKind::Constant;
        field.accelerationMetersPerSecondSquared = Length(acceleration);
        field.direction = Normalize(acceleration, {0.0f, -1.0f, 0.0f});
        field.strength = field.accelerationMetersPerSecondSquared;
        field.debugName = std::move(debugName);
        return field;
    }

    ForceFieldDesc MakeSphericalAttractorField(u32 id, Vec3 center, float radiusMeters, float accelerationMetersPerSecondSquared, std::string debugName)
    {
        ForceFieldDesc field{};
        field.id = id;
        field.kind = ForceFieldKind::PointAttractor;
        field.volumeKind = ForceFieldVolumeKind::Sphere;
        field.falloffKind = ForceFieldFalloffKind::SmoothStep;
        field.center = center;
        field.radiusMeters = radiusMeters;
        field.accelerationMetersPerSecondSquared = accelerationMetersPerSecondSquared;
        field.strength = accelerationMetersPerSecondSquared;
        field.debugName = std::move(debugName);
        return field;
    }

    ForceFieldDesc MakeBoxWindField(u32 id, Vec3 center, Vec3 halfExtents, Vec3 windVelocityMetersPerSecond, float strength, std::string debugName)
    {
        ForceFieldDesc field{};
        field.id = id;
        field.kind = ForceFieldKind::Wind;
        field.volumeKind = ForceFieldVolumeKind::Box;
        field.falloffKind = ForceFieldFalloffKind::Constant;
        field.center = center;
        field.halfExtents = halfExtents;
        field.velocityMetersPerSecond = windVelocityMetersPerSecond;
        field.strength = strength;
        field.debugName = std::move(debugName);
        return field;
    }

    ForceFieldDesc MakeVortexField(u32 id, Vec3 center, float radiusMeters, Vec3 axis, float strength, std::string debugName)
    {
        ForceFieldDesc field{};
        field.id = id;
        field.kind = ForceFieldKind::Vortex;
        field.volumeKind = ForceFieldVolumeKind::Sphere;
        field.falloffKind = ForceFieldFalloffKind::Linear;
        field.center = center;
        field.radiusMeters = radiusMeters;
        field.axis = Normalize(axis, {0.0f, 1.0f, 0.0f});
        field.strength = strength;
        field.accelerationMetersPerSecondSquared = strength;
        field.debugName = std::move(debugName);
        return field;
    }

    ForceFieldDesc MakeDamageVolumeField(u32 id, Vec3 center, Vec3 halfExtents, float damagePerSecond, DamageKind kind, std::string debugName)
    {
        ForceFieldDesc field{};
        field.id = id;
        field.kind = ForceFieldKind::DamageOverTime;
        field.volumeKind = ForceFieldVolumeKind::Box;
        field.falloffKind = ForceFieldFalloffKind::Constant;
        field.center = center;
        field.halfExtents = halfExtents;
        field.damagePerSecond = damagePerSecond;
        field.damageKind = kind;
        field.debugName = std::move(debugName);
        return field;
    }

    ForceFieldDesc SanitizeForceField(ForceFieldDesc field)
    {
        field.direction = Normalize(field.direction, {0.0f, 1.0f, 0.0f});
        field.axis = Normalize(field.axis, {0.0f, 1.0f, 0.0f});
        field.radiusMeters = std::max(MinimumRadius, field.radiusMeters);
        field.innerRadiusMeters = std::clamp(field.innerRadiusMeters, 0.0f, field.radiusMeters);
        field.halfExtents = Max(field.halfExtents, {MinimumRadius, MinimumRadius, MinimumRadius});
        field.strength = std::max(0.0f, field.strength);
        field.accelerationMetersPerSecondSquared = std::max(0.0f, field.accelerationMetersPerSecondSquared);
        field.linearDrag = std::max(0.0f, field.linearDrag);
        field.damagePerSecond = std::max(0.0f, field.damagePerSecond);
        field.maxForceNewton = std::clamp(field.maxForceNewton, 0.0f, MaximumForce);
        return field;
    }

    bool IsFinite(const ForceFieldDesc& field)
    {
        return IsFinite(field.center)
            && IsFinite(field.halfExtents)
            && IsFinite(field.direction)
            && IsFinite(field.velocityMetersPerSecond)
            && IsFinite(field.axis)
            && IsFinite(field.radiusMeters)
            && IsFinite(field.strength)
            && IsFinite(field.accelerationMetersPerSecondSquared)
            && IsFinite(field.linearDrag)
            && IsFinite(field.damagePerSecond)
            && IsFinite(field.maxForceNewton);
    }

    bool IsFinite(const ForceFieldBodySample& sample)
    {
        return IsFinite(sample.point)
            && IsFinite(sample.forceNewton)
            && IsFinite(sample.accelerationMetersPerSecondSquared)
            && IsFinite(sample.fieldVelocity)
            && IsFinite(sample.weight)
            && IsFinite(sample.distanceMeters)
            && IsFinite(sample.damageAmount);
    }

    float EvaluateForceFieldWeight(const ForceFieldDesc& rawField, Vec3 point, float* outDistanceMeters)
    {
        const ForceFieldDesc field = SanitizeForceField(rawField);
        if (!field.enabled || !IsFinite(field))
        {
            return 0.0f;
        }

        float distance = 0.0f;
        float weight = 0.0f;
        if (field.volumeKind == ForceFieldVolumeKind::Global)
        {
            weight = 1.0f;
        }
        else if (field.volumeKind == ForceFieldVolumeKind::Sphere)
        {
            distance = Length(Subtract(point, field.center));
            if (distance <= field.radiusMeters)
            {
                weight = EvaluateFalloff(field.falloffKind, distance, field.innerRadiusMeters, field.radiusMeters);
            }
        }
        else if (field.volumeKind == ForceFieldVolumeKind::Box)
        {
            const AABB3 bounds = MakeAABB3FromCenterExtents(field.center, field.halfExtents);
            if (Contains(bounds, point))
            {
                const Vec3 delta = Subtract(point, field.center);
                const Vec3 normalizedDistance{
                    std::abs(delta.x) / std::max(FloatEpsilon, field.halfExtents.x),
                    std::abs(delta.y) / std::max(FloatEpsilon, field.halfExtents.y),
                    std::abs(delta.z) / std::max(FloatEpsilon, field.halfExtents.z)
                };
                distance = std::max(normalizedDistance.x, std::max(normalizedDistance.y, normalizedDistance.z));
                weight = field.falloffKind == ForceFieldFalloffKind::Constant ? 1.0f : EvaluateFalloff(field.falloffKind, distance, 0.0f, 1.0f);
            }
        }
        else if (field.volumeKind == ForceFieldVolumeKind::HalfSpace)
        {
            const float signedDistance = Dot(Subtract(point, field.center), field.direction);
            if (signedDistance >= 0.0f)
            {
                distance = signedDistance;
                weight = 1.0f;
            }
        }

        if (outDistanceMeters)
        {
            *outDistanceMeters = distance;
        }
        return std::clamp(weight, 0.0f, 1.0f);
    }

    bool ForceFieldCanAffectCollider(const ForceFieldDesc& rawField, const PhysicsScene&, const PhysicsBody& body, const PhysicsCollider* collider)
    {
        const ForceFieldDesc field = SanitizeForceField(rawField);
        if (!field.enabled || !body.enabled)
        {
            return false;
        }

        if (body.kind == PhysicsBodyKind::Static && !field.filter.affectStatic)
        {
            return false;
        }
        if (body.kind == PhysicsBodyKind::Kinematic && !field.filter.affectKinematic)
        {
            return false;
        }
        if (body.kind == PhysicsBodyKind::Dynamic && !field.filter.affectDynamic)
        {
            return false;
        }
        if (collider)
        {
            if (!collider->enabled)
            {
                return false;
            }
            if (collider->trigger && !field.filter.includeTriggers && field.kind != ForceFieldKind::DamageOverTime)
            {
                return false;
            }
            if (!AnyLayerMatches(collider->filter.layerMask, field.filter.affectsLayersMask))
            {
                return false;
            }
        }
        return true;
    }

    ForceFieldBodySample ComputeForceFieldSample(const ForceFieldDesc& rawField, const PhysicsScene& scene, const PhysicsBody& body, const PhysicsCollider* collider, std::size_t colliderIndex, float deltaSeconds)
    {
        const ForceFieldDesc field = SanitizeForceField(rawField);
        ForceFieldBodySample sample{};
        sample.fieldId = field.id;
        sample.bodyId = body.id;
        sample.colliderIndex = colliderIndex;
        sample.kind = field.kind;
        sample.volumeKind = field.volumeKind;
        sample.point = BodySamplePoint(scene, body, collider);
        sample.fieldVelocity = field.velocityMetersPerSecond;

        if (!ForceFieldCanAffectCollider(field, scene, body, collider))
        {
            sample.finite = IsFinite(sample);
            return sample;
        }

        float distance = 0.0f;
        const float weight = EvaluateForceFieldWeight(field, sample.point, &distance);
        sample.weight = weight;
        sample.distanceMeters = distance;
        if (weight <= 0.0f)
        {
            sample.finite = IsFinite(sample);
            return sample;
        }

        const float safeMass = std::max(0.001f, body.massKilograms);
        Vec3 force{};
        Vec3 acceleration{};

        if (field.kind == ForceFieldKind::DirectionalAcceleration)
        {
            acceleration = Multiply(field.direction, field.accelerationMetersPerSecondSquared > 0.0f ? field.accelerationMetersPerSecondSquared : field.strength);
            force = Multiply(acceleration, safeMass);
        }
        else if (field.kind == ForceFieldKind::DirectionalForce)
        {
            force = Multiply(field.direction, field.strength);
            acceleration = Divide(force, safeMass);
        }
        else if (field.kind == ForceFieldKind::PointAttractor || field.kind == ForceFieldKind::PointRepulsor)
        {
            Vec3 direction = Normalize(Subtract(field.center, sample.point), field.direction);
            if (field.kind == ForceFieldKind::PointRepulsor)
            {
                direction = Negate(direction);
            }
            acceleration = Multiply(direction, field.accelerationMetersPerSecondSquared > 0.0f ? field.accelerationMetersPerSecondSquared : field.strength);
            force = Multiply(acceleration, safeMass);
        }
        else if (field.kind == ForceFieldKind::Vortex)
        {
            const Vec3 radial = Subtract(sample.point, field.center);
            const Vec3 tangent = Normalize(Cross(field.axis, radial), field.direction);
            acceleration = Multiply(tangent, field.accelerationMetersPerSecondSquared > 0.0f ? field.accelerationMetersPerSecondSquared : field.strength);
            force = Multiply(acceleration, safeMass);
        }
        else if (field.kind == ForceFieldKind::LinearDrag)
        {
            force = Multiply(body.velocity, -field.linearDrag * safeMass);
            acceleration = Divide(force, safeMass);
        }
        else if (field.kind == ForceFieldKind::Wind)
        {
            const Vec3 relativeVelocity = Subtract(field.velocityMetersPerSecond, body.velocity);
            force = Multiply(relativeVelocity, field.strength * safeMass);
            acceleration = Divide(force, safeMass);
        }
        else if (field.kind == ForceFieldKind::DamageOverTime)
        {
            sample.damageAmount = field.damagePerSecond * std::max(MinimumDeltaSeconds, deltaSeconds) * weight;
        }

        sample.forceNewton = ClampLength(Multiply(force, weight), field.maxForceNewton);
        sample.accelerationMetersPerSecondSquared = Divide(sample.forceNewton, safeMass);
        if (field.kind == ForceFieldKind::DamageOverTime)
        {
            sample.accelerationMetersPerSecondSquared = {};
        }
        sample.active = LengthSquared(sample.forceNewton) > FloatEpsilon * FloatEpsilon || sample.damageAmount > 0.0f;
        sample.finite = IsFinite(sample);
        return sample;
    }

    ForceFieldSystemStats ApplyForceFieldsToPhysicsScene(PhysicsScene& scene, const std::vector<ForceFieldDesc>& fields, float deltaSeconds, DamageSystem* damageSystem, std::vector<ForceFieldBodySample>* outSamples)
    {
        ForceFieldSystemStats stats{};
        stats.fieldCount = fields.size();
        const float safeDelta = std::max(MinimumDeltaSeconds, deltaSeconds);
        if (outSamples)
        {
            outSamples->clear();
        }

        for (const ForceFieldDesc& rawField : fields)
        {
            const ForceFieldDesc field = SanitizeForceField(rawField);
            if (!field.enabled)
            {
                continue;
            }
            ++stats.enabledFieldCount;
            if (!IsFinite(field))
            {
                stats.finite = false;
                AddWarning(stats.warnings, "non-finite force field ignored");
                continue;
            }

            for (PhysicsBody& body : scene.bodies)
            {
                if (!body.enabled)
                {
                    continue;
                }
                ++stats.testedBodyCount;
                std::size_t colliderIndex = std::numeric_limits<std::size_t>::max();
                const PhysicsCollider* collider = FindFirstColliderForBody(scene, body.id, &colliderIndex);
                ++stats.testedColliderCount;
                ForceFieldBodySample sample = ComputeForceFieldSample(field, scene, body, collider, colliderIndex, safeDelta);
                if (!sample.finite)
                {
                    stats.finite = false;
                    AddWarning(stats.warnings, "non-finite force field sample");
                    continue;
                }
                if (!sample.active)
                {
                    continue;
                }

                ++stats.affectedBodyCount;
                stats.totalForceNewton += Length(sample.forceNewton);
                if (field.kind == ForceFieldKind::DirectionalAcceleration || field.kind == ForceFieldKind::PointAttractor || field.kind == ForceFieldKind::PointRepulsor || field.kind == ForceFieldKind::Vortex)
                {
                    ++stats.accelerationFieldCount;
                }
                if (field.kind == ForceFieldKind::LinearDrag)
                {
                    ++stats.dragFieldCount;
                }
                if (field.kind == ForceFieldKind::Wind)
                {
                    ++stats.windFieldCount;
                }

                if (LengthSquared(sample.forceNewton) > FloatEpsilon * FloatEpsilon && body.kind == PhysicsBodyKind::Dynamic)
                {
                    ApplyForce(body, sample.forceNewton);
                    body.sleeping = field.wakeBodies ? false : body.sleeping;
                    ++stats.appliedForceCount;
                }

                if (sample.damageAmount > 0.0f && damageSystem)
                {
                    DamageEvent event{};
                    event.kind = field.damageKind;
                    event.targetKind = DamageTargetKind::PhysicsBody;
                    event.sourceId = field.id;
                    event.targetId = body.id;
                    event.point = sample.point;
                    event.normal = field.direction;
                    event.direction = field.direction;
                    event.amount = sample.damageAmount;
                    event.damagePerSecond = field.damagePerSecond;
                    event.deltaSeconds = safeDelta;
                    event.valid = true;
                    if (QueueDamageEvent(*damageSystem, event))
                    {
                        ++stats.damageEventCount;
                        stats.totalDamage += sample.damageAmount;
                        sample.queuedDamage = true;
                    }
                }

                if (outSamples)
                {
                    outSamples->push_back(sample);
                }
            }
        }
        return stats;
    }

    std::string ToDebugString(const ForceFieldDesc& field)
    {
        std::ostringstream out;
        out << "forcefield id=" << field.id
            << " kind=" << ToString(field.kind)
            << " volume=" << ToString(field.volumeKind)
            << " falloff=" << ToString(field.falloffKind)
            << " center=" << ToDebugString(field.center, 2)
            << " radius=" << field.radiusMeters
            << " strength=" << field.strength
            << " accel=" << field.accelerationMetersPerSecondSquared
            << " damage_s=" << field.damagePerSecond
            << " enabled=" << (field.enabled ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const ForceFieldBodySample& sample)
    {
        std::ostringstream out;
        out << "forcefield_sample active=" << (sample.active ? "true" : "false")
            << " field=" << sample.fieldId
            << " body=" << sample.bodyId
            << " kind=" << ToString(sample.kind)
            << " weight=" << sample.weight
            << " force=" << ToDebugString(sample.forceNewton, 2)
            << " accel=" << ToDebugString(sample.accelerationMetersPerSecondSquared, 2)
            << " damage=" << sample.damageAmount
            << " queued=" << (sample.queuedDamage ? "true" : "false")
            << " finite=" << (sample.finite ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const ForceFieldSystemStats& stats)
    {
        std::ostringstream out;
        out << "forcefield_stats fields=" << stats.fieldCount
            << " enabled=" << stats.enabledFieldCount
            << " bodies=" << stats.testedBodyCount
            << " colliders=" << stats.testedColliderCount
            << " affected=" << stats.affectedBodyCount
            << " forces=" << stats.appliedForceCount
            << " accel=" << stats.accelerationFieldCount
            << " drag=" << stats.dragFieldCount
            << " wind=" << stats.windFieldCount
            << " damage_events=" << stats.damageEventCount
            << " total_force=" << stats.totalForceNewton
            << " total_damage=" << stats.totalDamage
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.warnings.size();
        return out.str();
    }

    ForceFieldProbeResult BuildForceFieldProbe()
    {
        ForceFieldProbeResult result{};
        result.scene.config.enableGravity = false;
        result.scene.config.fixedDeltaSeconds = 1.0f / 60.0f;
        result.scene.config.broadphaseGridCellSize = 1.0f;

        PhysicsBody bodyA = MakeDynamicBody(1, {-1.5f, 0.5f, 0.0f}, 8.0f);
        bodyA.velocity = {2.0f, 0.0f, 0.0f};
        PhysicsBody bodyB = MakeDynamicBody(2, {1.2f, 0.4f, 0.0f}, 4.0f);
        bodyB.velocity = {-1.0f, 0.0f, 0.2f};
        PhysicsBody bodyC = MakeStaticBody(3, {0.0f, -0.1f, 0.0f});
        result.scene.bodies.push_back(bodyA);
        result.scene.bodies.push_back(bodyB);
        result.scene.bodies.push_back(bodyC);

        PhysicsCollider colliderA = MakeSphereCollider(1, 0.35f);
        colliderA.filter.layerMask = PhysicsLayer_Dynamic;
        PhysicsCollider colliderB = MakeBoxCollider(2, {0.35f, 0.35f, 0.35f});
        colliderB.filter.layerMask = PhysicsLayer_Dynamic | PhysicsLayer_Destructible;
        PhysicsCollider ground = MakeBoxCollider(3, {4.0f, 0.1f, 4.0f});
        ground.filter.layerMask = PhysicsLayer_Static;
        result.scene.colliders.push_back(colliderA);
        result.scene.colliders.push_back(colliderB);
        result.scene.colliders.push_back(ground);

        result.fields.push_back(MakeGlobalAccelerationField(100, {0.0f, -2.0f, 0.0f}, "low_gravity_modifier"));
        result.fields.push_back(MakeSphericalAttractorField(101, {0.0f, 0.6f, 0.0f}, 3.0f, 5.0f, "local_gravity_well"));
        result.fields.push_back(MakeBoxWindField(102, {0.0f, 0.5f, 0.0f}, {3.0f, 1.5f, 2.0f}, {8.0f, 0.0f, 1.0f}, 0.45f, "wind_corridor"));
        ForceFieldDesc drag{};
        drag.id = 105;
        drag.kind = ForceFieldKind::LinearDrag;
        drag.volumeKind = ForceFieldVolumeKind::Box;
        drag.falloffKind = ForceFieldFalloffKind::Constant;
        drag.center = {0.0f, 0.5f, 0.0f};
        drag.halfExtents = {3.0f, 1.5f, 2.0f};
        drag.linearDrag = 0.35f;
        drag.debugName = "thick_air_drag";
        result.fields.push_back(drag);
        ForceFieldDesc vortex = MakeVortexField(103, {0.0f, 0.5f, 0.0f}, 2.8f, {0.0f, 1.0f, 0.0f}, 3.0f, "shock_afterflow");
        result.fields.push_back(vortex);
        result.fields.push_back(MakeDamageVolumeField(104, {1.2f, 0.4f, 0.0f}, {0.8f, 0.8f, 0.8f}, 12.0f, DamageKind::Heat, "heat_hazard"));

        result.damageSystem = MakeDamageSystem();
        result.damageables.push_back(MakeDamageableBody(1, 100.0f));
        result.damageables.push_back(MakeDamageableBody(2, 100.0f));

        result.stats = ApplyForceFieldsToPhysicsScene(result.scene, result.fields, 1.0f / 60.0f, &result.damageSystem, &result.samples);
        result.damageStats = ProcessDamageEvents(result.damageSystem, result.damageables, &result.scene, nullptr);
        result.physicsStats = StepPhysics(result.scene, 1.0f / 60.0f);

        result.ok = result.stats.finite
            && result.damageStats.finite
            && result.physicsStats.finite
            && result.stats.appliedForceCount >= 2
            && result.stats.damageEventCount >= 1
            && result.physicsStats.integratedBodyCount >= 2;

        std::ostringstream summary;
        summary << "[ " << (result.ok ? "ok" : "failed") << " ] force fields / volumes / area effects foundation"
            << " fields=" << result.stats.fieldCount
            << " affected=" << result.stats.affectedBodyCount
            << " forces=" << result.stats.appliedForceCount
            << " wind=" << result.stats.windFieldCount
            << " drag=" << result.stats.dragFieldCount
            << " damage_events=" << result.stats.damageEventCount
            << " damage_applied=" << result.damageStats.appliedEventCount
            << " total_force=" << std::fixed << std::setprecision(2) << result.stats.totalForceNewton
            << " samples=" << result.samples.size()
            << " physics_contacts=" << result.physicsStats.contactCount
            << " finite=" << (result.ok ? "true" : "false")
            << " warnings=" << (result.stats.warnings.size() + result.damageStats.warnings.size() + result.physicsStats.warnings.size());
        result.summary = summary.str();
        return result;
    }

    std::string BuildForceFieldProbeSummary()
    {
        return BuildForceFieldProbe().summary;
    }
}
