#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Damage/Damage.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class ForceFieldKind : u32
    {
        DirectionalAcceleration = 0,
        DirectionalForce = 1,
        PointAttractor = 2,
        PointRepulsor = 3,
        Vortex = 4,
        LinearDrag = 5,
        Wind = 6,
        DamageOverTime = 7
    };

    enum class ForceFieldVolumeKind : u32
    {
        Global = 0,
        Sphere = 1,
        Box = 2,
        HalfSpace = 3
    };

    enum class ForceFieldFalloffKind : u32
    {
        Constant = 0,
        Linear = 1,
        SmoothStep = 2,
        InverseSquare = 3
    };

    struct ForceFieldFilter
    {
        u32 affectsLayersMask = PhysicsLayer_All;
        bool affectStatic = false;
        bool affectKinematic = false;
        bool affectDynamic = true;
        bool includeTriggers = false;
        bool queueDamageForTriggers = true;
    };

    struct ForceFieldDesc
    {
        u32 id = 0;
        ForceFieldKind kind = ForceFieldKind::DirectionalAcceleration;
        ForceFieldVolumeKind volumeKind = ForceFieldVolumeKind::Global;
        ForceFieldFalloffKind falloffKind = ForceFieldFalloffKind::SmoothStep;
        ForceFieldFilter filter{};
        Vec3 center{};
        Vec3 halfExtents{1.0f, 1.0f, 1.0f};
        float radiusMeters = 1.0f;
        Vec3 direction{0.0f, 1.0f, 0.0f};
        Vec3 velocityMetersPerSecond{};
        Vec3 axis{0.0f, 1.0f, 0.0f};
        float strength = 1.0f;
        float accelerationMetersPerSecondSquared = 0.0f;
        float linearDrag = 0.0f;
        float damagePerSecond = 0.0f;
        float maxForceNewton = 1.0e6f;
        float innerRadiusMeters = 0.0f;
        DamageKind damageKind = DamageKind::Generic;
        bool enabled = true;
        bool wakeBodies = true;
        std::string debugName;
    };

    struct ForceFieldBodySample
    {
        bool active = false;
        u32 fieldId = 0;
        u32 bodyId = 0;
        std::size_t colliderIndex = 0;
        ForceFieldKind kind = ForceFieldKind::DirectionalAcceleration;
        ForceFieldVolumeKind volumeKind = ForceFieldVolumeKind::Global;
        Vec3 point{};
        Vec3 forceNewton{};
        Vec3 accelerationMetersPerSecondSquared{};
        Vec3 fieldVelocity{};
        float weight = 0.0f;
        float distanceMeters = 0.0f;
        float damageAmount = 0.0f;
        bool queuedDamage = false;
        bool finite = true;
    };

    struct ForceFieldSystemStats
    {
        std::size_t fieldCount = 0;
        std::size_t enabledFieldCount = 0;
        std::size_t testedBodyCount = 0;
        std::size_t testedColliderCount = 0;
        std::size_t affectedBodyCount = 0;
        std::size_t appliedForceCount = 0;
        std::size_t accelerationFieldCount = 0;
        std::size_t dragFieldCount = 0;
        std::size_t windFieldCount = 0;
        std::size_t damageEventCount = 0;
        float totalForceNewton = 0.0f;
        float totalDamage = 0.0f;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct ForceFieldProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        std::vector<ForceFieldDesc> fields;
        std::vector<ForceFieldBodySample> samples;
        ForceFieldSystemStats stats{};
        DamageSystem damageSystem{};
        std::vector<DamageableComponent> damageables;
        DamageSystemStats damageStats{};
        PhysicsStepStats physicsStats{};
    };

    const char* ToString(ForceFieldKind kind);
    const char* ToString(ForceFieldVolumeKind kind);
    const char* ToString(ForceFieldFalloffKind kind);

    ForceFieldDesc MakeGlobalAccelerationField(u32 id, Vec3 acceleration, std::string debugName = {});
    ForceFieldDesc MakeSphericalAttractorField(u32 id, Vec3 center, float radiusMeters, float accelerationMetersPerSecondSquared, std::string debugName = {});
    ForceFieldDesc MakeBoxWindField(u32 id, Vec3 center, Vec3 halfExtents, Vec3 windVelocityMetersPerSecond, float strength = 1.0f, std::string debugName = {});
    ForceFieldDesc MakeVortexField(u32 id, Vec3 center, float radiusMeters, Vec3 axis, float strength, std::string debugName = {});
    ForceFieldDesc MakeDamageVolumeField(u32 id, Vec3 center, Vec3 halfExtents, float damagePerSecond, DamageKind kind = DamageKind::Generic, std::string debugName = {});

    ForceFieldDesc SanitizeForceField(ForceFieldDesc field);
    bool IsFinite(const ForceFieldDesc& field);
    bool IsFinite(const ForceFieldBodySample& sample);
    float EvaluateForceFieldWeight(const ForceFieldDesc& field, Vec3 point, float* outDistanceMeters = nullptr);
    bool ForceFieldCanAffectCollider(const ForceFieldDesc& field, const PhysicsScene& scene, const PhysicsBody& body, const PhysicsCollider* collider);
    ForceFieldBodySample ComputeForceFieldSample(const ForceFieldDesc& field, const PhysicsScene& scene, const PhysicsBody& body, const PhysicsCollider* collider, std::size_t colliderIndex, float deltaSeconds);
    ForceFieldSystemStats ApplyForceFieldsToPhysicsScene(PhysicsScene& scene, const std::vector<ForceFieldDesc>& fields, float deltaSeconds, DamageSystem* damageSystem = nullptr, std::vector<ForceFieldBodySample>* outSamples = nullptr);

    std::string ToDebugString(const ForceFieldDesc& field);
    std::string ToDebugString(const ForceFieldBodySample& sample);
    std::string ToDebugString(const ForceFieldSystemStats& stats);
    std::string BuildForceFieldProbeSummary();
    ForceFieldProbeResult BuildForceFieldProbe();
}
