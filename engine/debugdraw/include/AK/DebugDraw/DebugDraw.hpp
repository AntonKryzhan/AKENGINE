#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Debris/Debris.hpp>
#include <AK/Explosion/Explosion.hpp>
#include <AK/Fluid/Fluid.hpp>
#include <AK/ForceField/ForceField.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>
#include <AK/Projectile/Projectile.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class DebugDrawPrimitiveKind : u32
    {
        Line = 0,
        Arrow = 1,
        Point = 2,
        Aabb = 3,
        Obb = 4,
        Sphere = 5,
        Capsule = 6,
        Text = 7
    };

    enum class DebugDrawCategory : u32
    {
        None = 0,
        PhysicsBody = 1u << 0u,
        Collider = 1u << 1u,
        Broadphase = 1u << 2u,
        Contact = 1u << 3u,
        Query = 1u << 4u,
        Projectile = 1u << 5u,
        Fluid = 1u << 6u,
        Debris = 1u << 7u,
        Damage = 1u << 8u,
        Explosion = 1u << 9u,
        ForceField = 1u << 10u,
        All = 0xffffffffu
    };

    enum class DebugDrawSpace : u32
    {
        World = 0,
        CameraRelative = 1,
        PhysicsIslandLocal = 2
    };

    struct DebugDrawColor
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    struct DebugDrawCommand
    {
        DebugDrawPrimitiveKind kind = DebugDrawPrimitiveKind::Line;
        DebugDrawCategory category = DebugDrawCategory::None;
        DebugDrawSpace space = DebugDrawSpace::World;
        DebugDrawColor color{};
        Vec3 a{};
        Vec3 b{};
        Vec3 c{};
        Vec3 halfExtents{};
        Quat rotation{};
        float radius = 0.0f;
        float halfHeight = 0.0f;
        float thickness = 1.0f;
        float lifetimeSeconds = 0.0f;
        u32 id = 0;
        std::string label;
    };

    struct DebugDrawStats
    {
        std::size_t commandCount = 0;
        std::size_t lineCount = 0;
        std::size_t arrowCount = 0;
        std::size_t pointCount = 0;
        std::size_t aabbCount = 0;
        std::size_t obbCount = 0;
        std::size_t sphereCount = 0;
        std::size_t capsuleCount = 0;
        std::size_t textCount = 0;
        std::size_t physicsCommandCount = 0;
        std::size_t broadphaseCommandCount = 0;
        std::size_t contactCommandCount = 0;
        std::size_t queryCommandCount = 0;
        std::size_t projectileCommandCount = 0;
        std::size_t fluidCommandCount = 0;
        std::size_t debrisCommandCount = 0;
        std::size_t damageCommandCount = 0;
        std::size_t explosionCommandCount = 0;
        std::size_t forceFieldCommandCount = 0;
        std::size_t clippedCommandCount = 0;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct DebugDrawList
    {
        std::vector<DebugDrawCommand> commands;
        DebugDrawStats stats{};
        std::size_t maxCommands = 8192;
        bool clipped = false;
    };

    struct DebugDrawConfig
    {
        DebugDrawCategory enabledCategories = DebugDrawCategory::All;
        std::size_t maxCommands = 8192;
        float defaultLifetimeSeconds = 0.0f;
        bool drawPhysicsBodies = true;
        bool drawColliders = true;
        bool drawBroadphasePairs = true;
        bool drawContacts = true;
        bool drawManifolds = true;
        bool drawQueries = true;
        bool drawProjectileTrajectory = true;
        bool drawFluids = true;
        bool drawDebris = true;
        bool drawExplosions = true;
        bool drawForceFields = true;
        bool drawLabels = true;
    };

    struct DebugDrawProbeResult
    {
        bool ok = false;
        std::string summary;
        DebugDrawList list{};
        PhysicsProbeResult physics{};
        ProjectileProbeResult projectile{};
        FluidProbeResult fluid{};
        DebrisProbeResult debris{};
        ExplosionProbeResult explosion{};
        ForceFieldProbeResult forceField{};
    };

    DebugDrawCategory operator|(DebugDrawCategory a, DebugDrawCategory b);
    DebugDrawCategory operator&(DebugDrawCategory a, DebugDrawCategory b);
    bool HasDebugDrawCategory(DebugDrawCategory mask, DebugDrawCategory category);

    DebugDrawColor MakeDebugDrawColor(float r, float g, float b, float a = 1.0f);
    DebugDrawColor DebugColorPhysicsBody();
    DebugDrawColor DebugColorCollider();
    DebugDrawColor DebugColorTrigger();
    DebugDrawColor DebugColorBroadphase();
    DebugDrawColor DebugColorContact();
    DebugDrawColor DebugColorQuery();
    DebugDrawColor DebugColorProjectile();
    DebugDrawColor DebugColorFluid();
    DebugDrawColor DebugColorDebris();
    DebugDrawColor DebugColorDamage();
    DebugDrawColor DebugColorExplosion();
    DebugDrawColor DebugColorForceField();

    const char* ToString(DebugDrawPrimitiveKind kind);
    const char* ToString(DebugDrawCategory category);
    const char* ToString(DebugDrawSpace space);

    bool IsFinite(const DebugDrawCommand& command);
    void ResetDebugDrawList(DebugDrawList& list, std::size_t maxCommands = 8192);
    bool PushDebugDrawCommand(DebugDrawList& list, DebugDrawCommand command);

    bool AddDebugLine(DebugDrawList& list, Vec3 a, Vec3 b, DebugDrawColor color, DebugDrawCategory category, std::string label = {});
    bool AddDebugArrow(DebugDrawList& list, Vec3 a, Vec3 b, DebugDrawColor color, DebugDrawCategory category, std::string label = {});
    bool AddDebugPoint(DebugDrawList& list, Vec3 position, float radius, DebugDrawColor color, DebugDrawCategory category, std::string label = {});
    bool AddDebugAabb(DebugDrawList& list, AABB3 bounds, DebugDrawColor color, DebugDrawCategory category, std::string label = {});
    bool AddDebugObb(DebugDrawList& list, Vec3 center, Vec3 halfExtents, Quat rotation, DebugDrawColor color, DebugDrawCategory category, std::string label = {});
    bool AddDebugSphere(DebugDrawList& list, Sphere3 sphere, DebugDrawColor color, DebugDrawCategory category, std::string label = {});
    bool AddDebugCapsule(DebugDrawList& list, Vec3 a, Vec3 b, float radius, DebugDrawColor color, DebugDrawCategory category, std::string label = {});
    bool AddDebugText(DebugDrawList& list, Vec3 position, std::string text, DebugDrawColor color, DebugDrawCategory category);

    void CollectPhysicsDebugDraw(DebugDrawList& list, const PhysicsScene& scene, const std::vector<PhysicsBroadphasePair>& pairs, const std::vector<PhysicsContact>& contacts, const std::vector<PhysicsContactManifold>& manifolds, const DebugDrawConfig& config = {});
    void CollectPhysicsQueryDebugDraw(DebugDrawList& list, const PhysicsRaycastHit& raycast, Ray3 ray, float rayLength, const PhysicsSweepHit& sweep, Vec3 sweepStart, Vec3 sweepEnd, float sweepRadius, const DebugDrawConfig& config = {});
    void CollectProjectileDebugDraw(DebugDrawList& list, const ProjectileProbeResult& projectile, const DebugDrawConfig& config = {});
    void CollectFluidDebugDraw(DebugDrawList& list, const std::vector<FluidVolume>& volumes, const std::vector<FluidColliderInteraction>& interactions, const DebugDrawConfig& config = {});
    void CollectDebrisDebugDraw(DebugDrawList& list, const DebrisSystem& debris, const DebugDrawConfig& config = {});
    void CollectExplosionDebugDraw(DebugDrawList& list, const ExplosionDesc& explosion, const ExplosionResult& result, const DebugDrawConfig& config = {});
    void CollectForceFieldDebugDraw(DebugDrawList& list, const std::vector<ForceFieldDesc>& fields, const std::vector<ForceFieldBodySample>& samples, const DebugDrawConfig& config = {});

    DebugDrawStats RecomputeDebugDrawStats(const DebugDrawList& list);
    std::string ToDebugString(const DebugDrawColor& color);
    std::string ToDebugString(const DebugDrawCommand& command, int precision = 2);
    std::string ToDebugString(const DebugDrawStats& stats);
    std::string BuildDebugDrawProbeSummary();
    DebugDrawProbeResult BuildDebugDrawProbe();
}
