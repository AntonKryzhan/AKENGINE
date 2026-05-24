#include <AK/SoftBody/SoftBody.hpp>

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
        constexpr float MinimumMass = 1.0e-6f;
        constexpr float MinimumRadius = 0.001f;
        constexpr float MaximumVelocity = 1000.0f;
        constexpr u32 MaximumSubsteps = 32;
        constexpr u32 MaximumSolverIterations = 64;

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }

        float SafeInverseMass(float mass, bool pinned)
        {
            if (pinned || mass <= MinimumMass)
            {
                return 0.0f;
            }
            return 1.0f / mass;
        }

        float ParticleMass(const SoftBodyParticle& particle)
        {
            return particle.inverseMass > 0.0f ? 1.0f / particle.inverseMass : 0.0f;
        }

        Vec3 Negate(Vec3 value)
        {
            return {-value.x, -value.y, -value.z};
        }

        Vec3 ClampLength(Vec3 value, float maxLength, bool* clamped = nullptr)
        {
            const float length = Length(value);
            if (length > maxLength && length > FloatEpsilon)
            {
                if (clamped)
                {
                    *clamped = true;
                }
                return Multiply(value, maxLength / length);
            }

            if (clamped)
            {
                *clamped = false;
            }
            return value;
        }

        float Distance(Vec3 a, Vec3 b)
        {
            return Length(Subtract(a, b));
        }

        void CountConstraint(const SoftBodyConstraint& constraint, SoftBodyStepStats& stats)
        {
            switch (constraint.kind)
            {
            case SoftBodyConstraintKind::Distance:
                ++stats.distanceConstraintCount;
                break;
            case SoftBodyConstraintKind::Pin:
                ++stats.pinConstraintCount;
                break;
            case SoftBodyConstraintKind::Bending:
                ++stats.bendingConstraintCount;
                break;
            default:
                break;
            }
        }

        float CurrentConstraintError(const SoftBodyInstance& body, const SoftBodyConstraint& constraint)
        {
            if (constraint.kind == SoftBodyConstraintKind::Pin)
            {
                if (constraint.pin.particle >= body.particles.size() || !constraint.pin.enabled)
                {
                    return 0.0f;
                }
                return Distance(body.particles[constraint.pin.particle].position, constraint.pin.target);
            }

            const SoftBodyDistanceConstraint& distance = constraint.distance;
            if (distance.particleA >= body.particles.size() || distance.particleB >= body.particles.size() || !distance.enabled)
            {
                return 0.0f;
            }
            return std::abs(Distance(body.particles[distance.particleA].position, body.particles[distance.particleB].position) - distance.restLength);
        }

        bool SolveDistanceXPBD(SoftBodyInstance& body, const SoftBodyDistanceConstraint& constraint, float stepSeconds, float slop)
        {
            if (!constraint.enabled || constraint.particleA >= body.particles.size() || constraint.particleB >= body.particles.size())
            {
                return false;
            }

            SoftBodyParticle& a = body.particles[constraint.particleA];
            SoftBodyParticle& b = body.particles[constraint.particleB];
            if (!a.enabled || !b.enabled)
            {
                return false;
            }

            const float wA = a.inverseMass;
            const float wB = b.inverseMass;
            const float wSum = wA + wB;
            if (wSum <= FloatEpsilon)
            {
                return false;
            }

            const Vec3 delta = Subtract(b.position, a.position);
            const float length = Length(delta);
            if (length <= FloatEpsilon)
            {
                return false;
            }

            const float error = length - constraint.restLength;
            if (std::abs(error) <= slop)
            {
                return true;
            }

            const Vec3 normal = Divide(delta, length);
            const float alpha = std::max(0.0f, constraint.compliance) / std::max(MinimumDeltaSeconds, stepSeconds * stepSeconds);
            const float lambda = -error / (wSum + alpha);
            const Vec3 correction = Multiply(normal, lambda);

            if (wA > 0.0f)
            {
                a.position = Subtract(a.position, Multiply(correction, wA));
            }
            if (wB > 0.0f)
            {
                b.position = Add(b.position, Multiply(correction, wB));
            }
            return true;
        }

        bool SolvePinXPBD(SoftBodyInstance& body, const SoftBodyPinConstraint& constraint, float stepSeconds)
        {
            if (!constraint.enabled || constraint.particle >= body.particles.size())
            {
                return false;
            }

            SoftBodyParticle& particle = body.particles[constraint.particle];
            if (!particle.enabled)
            {
                return false;
            }

            const float w = std::max(particle.inverseMass, particle.pinned ? 1.0f : particle.inverseMass);
            if (w <= FloatEpsilon)
            {
                particle.position = constraint.target;
                particle.previousPosition = constraint.target;
                particle.velocity = {};
                return true;
            }

            const Vec3 error = Subtract(particle.position, constraint.target);
            const float alpha = std::max(0.0f, constraint.compliance) / std::max(MinimumDeltaSeconds, stepSeconds * stepSeconds);
            const Vec3 correction = Multiply(error, 1.0f / (1.0f + alpha));
            particle.position = Subtract(particle.position, correction);
            if (particle.pinned || constraint.compliance <= FloatEpsilon)
            {
                particle.previousPosition = particle.position;
                particle.velocity = {};
            }
            return true;
        }

        void ProjectParticleAgainstScene(SoftBodyParticle& particle, const SoftBodyConfig& config, const PhysicsScene& scene, SoftBodyStepStats& stats)
        {
            if (particle.inverseMass <= 0.0f || !particle.enabled)
            {
                return;
            }

            ++stats.collisionQueryCount;
            const PhysicsSweepHit hit = SweepSpherePhysicsScene(scene, particle.previousPosition, particle.position, particle.radius, PhysicsQueryFlags::Default);
            if (!hit.hit)
            {
                return;
            }

            ++stats.collisionHitCount;
            const Vec3 target = Add(hit.point, Multiply(hit.normal, particle.radius + config.constraintSlop));
            particle.position = target;

            const float normalVelocity = Dot(particle.velocity, hit.normal);
            if (normalVelocity < 0.0f)
            {
                const Vec3 normalPart = Multiply(hit.normal, normalVelocity);
                const Vec3 tangentPart = Subtract(particle.velocity, normalPart);
                particle.velocity = Add(Multiply(tangentPart, std::max(0.0f, 1.0f - config.collisionFriction)), Multiply(Negate(normalPart), config.collisionRestitution));
            }
        }

        void AddDistanceConstraint(SoftBodyInstance& body, u32 a, u32 b, float compliance, SoftBodyConstraintKind kind)
        {
            if (a >= body.particles.size() || b >= body.particles.size() || a == b)
            {
                return;
            }
            body.constraints.push_back(MakeSoftBodyDistanceConstraint(a, b, Distance(body.particles[a].position, body.particles[b].position), compliance, kind));
        }

        Vec3 GridPosition(const SoftBodyClothDesc& desc, u32 x, u32 y)
        {
            return Add(desc.origin, Add(Multiply(desc.right, static_cast<float>(x) * desc.spacing), Multiply(desc.down, static_cast<float>(y) * desc.spacing)));
        }
    }

    const char* ToString(SoftBodyTopology topology)
    {
        switch (topology)
        {
        case SoftBodyTopology::ClothGrid:
            return "ClothGrid";
        case SoftBodyTopology::Rope:
            return "Rope";
        case SoftBodyTopology::ParticleCloud:
            return "ParticleCloud";
        default:
            return "Unknown";
        }
    }

    const char* ToString(SoftBodyConstraintKind kind)
    {
        switch (kind)
        {
        case SoftBodyConstraintKind::Distance:
            return "Distance";
        case SoftBodyConstraintKind::Pin:
            return "Pin";
        case SoftBodyConstraintKind::Bending:
            return "Bending";
        default:
            return "Unknown";
        }
    }

    const char* ToString(SoftBodyCollisionMode mode)
    {
        switch (mode)
        {
        case SoftBodyCollisionMode::None:
            return "None";
        case SoftBodyCollisionMode::PhysicsScene:
            return "PhysicsScene";
        default:
            return "Unknown";
        }
    }

    SoftBodyConfig SanitizeSoftBodyConfig(SoftBodyConfig config)
    {
        if (!IsFinite(config.gravity))
        {
            config.gravity = {0.0f, -9.80665f, 0.0f};
        }
        config.fixedDeltaSeconds = std::clamp(config.fixedDeltaSeconds, MinimumDeltaSeconds, 0.25f);
        config.damping = std::clamp(config.damping, 0.0f, 10.0f);
        config.velocityLimit = std::clamp(config.velocityLimit, 0.1f, MaximumVelocity);
        config.collisionFriction = std::clamp(config.collisionFriction, 0.0f, 1.0f);
        config.collisionRestitution = std::clamp(config.collisionRestitution, 0.0f, 1.0f);
        config.constraintSlop = std::clamp(config.constraintSlop, 0.0f, 0.1f);
        config.solverIterations = std::clamp(config.solverIterations, 1u, MaximumSolverIterations);
        config.substeps = std::clamp(config.substeps, 1u, MaximumSubsteps);
        return config;
    }

    SoftBodyClothDesc SanitizeSoftBodyClothDesc(SoftBodyClothDesc desc)
    {
        desc.columns = std::clamp(desc.columns, 2u, 512u);
        desc.rows = std::clamp(desc.rows, 2u, 512u);
        desc.spacing = std::clamp(desc.spacing, 0.001f, 100.0f);
        desc.particleMassKilograms = std::max(MinimumMass, desc.particleMassKilograms);
        desc.particleRadius = std::max(MinimumRadius, desc.particleRadius);
        desc.stretchCompliance = std::max(0.0f, desc.stretchCompliance);
        desc.bendCompliance = std::max(0.0f, desc.bendCompliance);
        desc.right = Normalize(desc.right, {1.0f, 0.0f, 0.0f});
        desc.down = Normalize(desc.down, {0.0f, -1.0f, 0.0f});
        return desc;
    }

    SoftBodyRopeDesc SanitizeSoftBodyRopeDesc(SoftBodyRopeDesc desc)
    {
        desc.particleCount = std::clamp(desc.particleCount, 2u, 4096u);
        desc.spacing = std::clamp(desc.spacing, 0.001f, 100.0f);
        desc.particleMassKilograms = std::max(MinimumMass, desc.particleMassKilograms);
        desc.particleRadius = std::max(MinimumRadius, desc.particleRadius);
        desc.stretchCompliance = std::max(0.0f, desc.stretchCompliance);
        desc.direction = Normalize(desc.direction, {1.0f, 0.0f, 0.0f});
        return desc;
    }

    SoftBodyParticle MakeSoftBodyParticle(Vec3 position, float massKilograms, float radius, bool pinned)
    {
        SoftBodyParticle particle{};
        particle.position = position;
        particle.previousPosition = position;
        particle.velocity = {};
        particle.accumulatedForce = {};
        particle.inverseMass = SafeInverseMass(massKilograms, pinned);
        particle.radius = std::max(MinimumRadius, radius);
        particle.pinned = pinned;
        return particle;
    }

    SoftBodyConstraint MakeSoftBodyDistanceConstraint(u32 particleA, u32 particleB, float restLength, float compliance, SoftBodyConstraintKind kind)
    {
        SoftBodyConstraint constraint{};
        constraint.kind = kind;
        constraint.distance.particleA = particleA;
        constraint.distance.particleB = particleB;
        constraint.distance.restLength = std::max(0.0f, restLength);
        constraint.distance.compliance = std::max(0.0f, compliance);
        return constraint;
    }

    SoftBodyConstraint MakeSoftBodyPinConstraint(u32 particle, Vec3 target, float compliance)
    {
        SoftBodyConstraint constraint{};
        constraint.kind = SoftBodyConstraintKind::Pin;
        constraint.pin.particle = particle;
        constraint.pin.target = target;
        constraint.pin.compliance = std::max(0.0f, compliance);
        return constraint;
    }

    SoftBodyInstance MakeClothGridSoftBody(const SoftBodyClothDesc& inputDesc)
    {
        const SoftBodyClothDesc desc = SanitizeSoftBodyClothDesc(inputDesc);
        SoftBodyInstance body{};
        body.topology = SoftBodyTopology::ClothGrid;
        body.particles.reserve(static_cast<std::size_t>(desc.columns) * static_cast<std::size_t>(desc.rows));

        for (u32 y = 0; y < desc.rows; ++y)
        {
            for (u32 x = 0; x < desc.columns; ++x)
            {
                const bool pinned = desc.pinTopRow && y == 0u;
                const Vec3 position = GridPosition(desc, x, y);
                body.particles.push_back(MakeSoftBodyParticle(position, desc.particleMassKilograms, desc.particleRadius, pinned));
                if (pinned)
                {
                    body.constraints.push_back(MakeSoftBodyPinConstraint(static_cast<u32>(body.particles.size() - 1u), position));
                }
            }
        }

        const auto index = [columns = desc.columns](u32 x, u32 y) -> u32
        {
            return y * columns + x;
        };

        for (u32 y = 0; y < desc.rows; ++y)
        {
            for (u32 x = 0; x < desc.columns; ++x)
            {
                if (x + 1u < desc.columns)
                {
                    AddDistanceConstraint(body, index(x, y), index(x + 1u, y), desc.stretchCompliance, SoftBodyConstraintKind::Distance);
                }
                if (y + 1u < desc.rows)
                {
                    AddDistanceConstraint(body, index(x, y), index(x, y + 1u), desc.stretchCompliance, SoftBodyConstraintKind::Distance);
                }
                if (x + 1u < desc.columns && y + 1u < desc.rows)
                {
                    AddDistanceConstraint(body, index(x, y), index(x + 1u, y + 1u), desc.stretchCompliance * 2.0f, SoftBodyConstraintKind::Distance);
                    AddDistanceConstraint(body, index(x + 1u, y), index(x, y + 1u), desc.stretchCompliance * 2.0f, SoftBodyConstraintKind::Distance);
                }
                if (desc.addBending && x + 2u < desc.columns)
                {
                    AddDistanceConstraint(body, index(x, y), index(x + 2u, y), desc.bendCompliance, SoftBodyConstraintKind::Bending);
                }
                if (desc.addBending && y + 2u < desc.rows)
                {
                    AddDistanceConstraint(body, index(x, y), index(x, y + 2u), desc.bendCompliance, SoftBodyConstraintKind::Bending);
                }
            }
        }

        body.bounds = ComputeSoftBodyBounds(body);
        return body;
    }

    SoftBodyInstance MakeRopeSoftBody(const SoftBodyRopeDesc& inputDesc)
    {
        const SoftBodyRopeDesc desc = SanitizeSoftBodyRopeDesc(inputDesc);
        SoftBodyInstance body{};
        body.topology = SoftBodyTopology::Rope;
        body.particles.reserve(desc.particleCount);

        for (u32 i = 0; i < desc.particleCount; ++i)
        {
            const bool pinned = (desc.pinFirst && i == 0u) || (desc.pinLast && i + 1u == desc.particleCount);
            const Vec3 position = Add(desc.origin, Multiply(desc.direction, static_cast<float>(i) * desc.spacing));
            body.particles.push_back(MakeSoftBodyParticle(position, desc.particleMassKilograms, desc.particleRadius, pinned));
            if (pinned)
            {
                body.constraints.push_back(MakeSoftBodyPinConstraint(i, position));
            }
        }

        for (u32 i = 0; i + 1u < desc.particleCount; ++i)
        {
            AddDistanceConstraint(body, i, i + 1u, desc.stretchCompliance, SoftBodyConstraintKind::Distance);
        }
        for (u32 i = 0; i + 2u < desc.particleCount; ++i)
        {
            AddDistanceConstraint(body, i, i + 2u, desc.stretchCompliance * 12.0f, SoftBodyConstraintKind::Bending);
        }

        body.bounds = ComputeSoftBodyBounds(body);
        return body;
    }

    AABB3 ComputeSoftBodyBounds(const SoftBodyInstance& body)
    {
        AABB3 bounds = MakeEmptyAABB3();
        for (const SoftBodyParticle& particle : body.particles)
        {
            if (!particle.enabled)
            {
                continue;
            }
            const Vec3 radius{particle.radius, particle.radius, particle.radius};
            bounds = Expand(bounds, Subtract(particle.position, radius));
            bounds = Expand(bounds, Add(particle.position, radius));
        }
        return bounds;
    }

    bool IsFinite(const SoftBodyParticle& particle)
    {
        return IsFinite(particle.position) &&
            IsFinite(particle.previousPosition) &&
            IsFinite(particle.velocity) &&
            IsFinite(particle.accumulatedForce) &&
            IsFinite(particle.inverseMass) &&
            IsFinite(particle.radius);
    }

    bool IsFinite(const SoftBodyInstance& body)
    {
        for (const SoftBodyParticle& particle : body.particles)
        {
            if (!IsFinite(particle))
            {
                return false;
            }
        }
        return IsValid(body.bounds);
    }

    SoftBodyStepStats StepSoftBody(SoftBodyInstance& body, const SoftBodyConfig& inputConfig, float deltaSeconds, const PhysicsScene* scene)
    {
        SoftBodyStepStats stats{};
        SoftBodyConfig config = SanitizeSoftBodyConfig(inputConfig);
        deltaSeconds = std::clamp(deltaSeconds, MinimumDeltaSeconds, config.fixedDeltaSeconds * static_cast<float>(config.substeps));
        const float stepSeconds = deltaSeconds / static_cast<float>(config.substeps);
        stats.substeps = config.substeps;
        stats.solverIterations = config.solverIterations;
        stats.simulatedSeconds = deltaSeconds;
        stats.particleCount = body.particles.size();
        stats.constraintCount = body.constraints.size();

        if (!body.enabled)
        {
            AddWarning(stats.warnings, "soft body disabled");
            stats.bounds = body.bounds;
            stats.finite = IsFinite(body);
            return stats;
        }

        for (const SoftBodyParticle& particle : body.particles)
        {
            if (particle.inverseMass > 0.0f)
            {
                ++stats.dynamicParticleCount;
            }
            if (particle.pinned || particle.inverseMass <= 0.0f)
            {
                ++stats.pinnedParticleCount;
            }
        }
        for (const SoftBodyConstraint& constraint : body.constraints)
        {
            CountConstraint(constraint, stats);
        }

        for (u32 substep = 0; substep < config.substeps; ++substep)
        {
            for (SoftBodyParticle& particle : body.particles)
            {
                if (!particle.enabled || particle.inverseMass <= 0.0f || particle.pinned)
                {
                    particle.accumulatedForce = {};
                    continue;
                }

                const float mass = ParticleMass(particle);
                Vec3 acceleration{};
                if (config.enableGravity)
                {
                    acceleration = Add(acceleration, config.gravity);
                }
                acceleration = Add(acceleration, Multiply(particle.accumulatedForce, particle.inverseMass));

                particle.velocity = Add(particle.velocity, Multiply(acceleration, stepSeconds));
                particle.velocity = Multiply(particle.velocity, std::max(0.0f, 1.0f - config.damping * stepSeconds));
                bool clamped = false;
                particle.velocity = ClampLength(particle.velocity, config.velocityLimit, &clamped);
                if (clamped)
                {
                    ++stats.clampedVelocityCount;
                }

                particle.previousPosition = particle.position;
                particle.position = Add(particle.position, Multiply(particle.velocity, stepSeconds));
                particle.accumulatedForce = {};
                (void)mass;
            }

            if (config.enableConstraints)
            {
                for (u32 iteration = 0; iteration < config.solverIterations; ++iteration)
                {
                    for (const SoftBodyConstraint& constraint : body.constraints)
                    {
                        bool solved = false;
                        if (constraint.kind == SoftBodyConstraintKind::Pin)
                        {
                            solved = SolvePinXPBD(body, constraint.pin, stepSeconds);
                        }
                        else
                        {
                            solved = SolveDistanceXPBD(body, constraint.distance, stepSeconds, config.constraintSlop);
                        }
                        if (solved)
                        {
                            ++stats.solvedConstraintCount;
                        }
                    }
                }
            }

            if (config.enableSceneCollision && scene != nullptr)
            {
                for (SoftBodyParticle& particle : body.particles)
                {
                    ProjectParticleAgainstScene(particle, config, *scene, stats);
                }
            }

            for (SoftBodyParticle& particle : body.particles)
            {
                if (!particle.enabled || particle.inverseMass <= 0.0f || particle.pinned)
                {
                    continue;
                }
                particle.velocity = Divide(Subtract(particle.position, particle.previousPosition), stepSeconds);
            }
        }

        for (const SoftBodyConstraint& constraint : body.constraints)
        {
            stats.maxConstraintError = std::max(stats.maxConstraintError, CurrentConstraintError(body, constraint));
        }

        body.bounds = ComputeSoftBodyBounds(body);
        ++body.revision;
        stats.bounds = body.bounds;
        stats.finite = IsFinite(body);
        if (!stats.finite)
        {
            AddWarning(stats.warnings, "soft body produced non-finite state");
        }
        return stats;
    }

    SoftBodySystemStats FixedUpdateSoftBodies(std::vector<SoftBodyComponent>& components, float deltaSeconds, const PhysicsScene* scene)
    {
        SoftBodySystemStats stats{};
        stats.bodyCount = components.size();
        for (SoftBodyComponent& component : components)
        {
            if (!component.enabled || !component.body.enabled)
            {
                continue;
            }
            SoftBodyStepStats step = StepSoftBody(component.body, component.config, deltaSeconds, scene);
            ++stats.updatedBodyCount;
            stats.particleCount += step.particleCount;
            stats.collisionHitCount += step.collisionHitCount;
            stats.solvedConstraintCount += step.solvedConstraintCount;
            stats.finite = stats.finite && step.finite;
            for (const std::string& warning : step.warnings)
            {
                AddWarning(stats.warnings, warning);
            }
        }
        return stats;
    }

    std::string ToDebugString(const SoftBodyConfig& config)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(4);
        stream << "softbody_config substeps=" << config.substeps
            << " iterations=" << config.solverIterations
            << " damping=" << config.damping
            << " velocity_limit=" << config.velocityLimit
            << " collision=" << (config.enableSceneCollision ? "on" : "off")
            << " gravity=" << ToDebugString(config.gravity, 2);
        return stream.str();
    }

    std::string ToDebugString(const SoftBodyStepStats& stats)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(5);
        stream << "softbody_step particles=" << stats.particleCount
            << " dynamic=" << stats.dynamicParticleCount
            << " pinned=" << stats.pinnedParticleCount
            << " constraints=" << stats.constraintCount
            << " distance=" << stats.distanceConstraintCount
            << " bend=" << stats.bendingConstraintCount
            << " pin=" << stats.pinConstraintCount
            << " solved=" << stats.solvedConstraintCount
            << " collisions=" << stats.collisionHitCount << '/' << stats.collisionQueryCount
            << " max_error=" << stats.maxConstraintError
            << " substeps=" << stats.substeps
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.warnings.size();
        return stream.str();
    }

    std::string ToDebugString(const SoftBodySystemStats& stats)
    {
        std::ostringstream stream;
        stream << "softbody_system bodies=" << stats.bodyCount
            << " updated=" << stats.updatedBodyCount
            << " particles=" << stats.particleCount
            << " collisions=" << stats.collisionHitCount
            << " solved=" << stats.solvedConstraintCount
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.warnings.size();
        return stream.str();
    }

    std::string ToDebugString(const SoftBodyInstance& body)
    {
        std::ostringstream stream;
        const Vec3 size = Size(body.bounds);
        stream << "softbody topology=" << ToString(body.topology)
            << " particles=" << body.particles.size()
            << " constraints=" << body.constraints.size()
            << " revision=" << body.revision
            << " bounds_size=" << ToDebugString(size, 3)
            << " enabled=" << (body.enabled ? "true" : "false")
            << " finite=" << (IsFinite(body) ? "true" : "false");
        return stream.str();
    }

    SoftBodyProbeResult BuildSoftBodyProbe()
    {
        SoftBodyProbeResult probe{};

        PhysicsScene scene{};
        scene.config.broadphaseGridCellSize = 0.75f;
        scene.bodies.push_back(MakeStaticBody(1u, {0.0f, -0.12f, 0.0f}));
        PhysicsCollider ground = MakeBoxCollider(1u, {4.0f, 0.12f, 4.0f});
        ground.material = MakePhysicsMaterial(0.7f, 0.55f, 0.02f);
        scene.colliders.push_back(ground);
        probe.scene = scene;

        SoftBodyClothDesc clothDesc{};
        clothDesc.columns = 9u;
        clothDesc.rows = 7u;
        clothDesc.spacing = 0.22f;
        clothDesc.origin = {-0.88f, 1.25f, 0.0f};
        clothDesc.pinTopRow = true;
        clothDesc.addBending = true;
        probe.cloth = MakeClothGridSoftBody(clothDesc);

        SoftBodyRopeDesc ropeDesc{};
        ropeDesc.particleCount = 14u;
        ropeDesc.spacing = 0.16f;
        ropeDesc.origin = {-1.0f, 1.1f, 0.55f};
        ropeDesc.direction = {1.0f, 0.0f, 0.0f};
        ropeDesc.pinFirst = true;
        ropeDesc.pinLast = true;
        probe.rope = MakeRopeSoftBody(ropeDesc);

        SoftBodyConfig config{};
        config.substeps = 3u;
        config.solverIterations = 10u;
        config.fixedDeltaSeconds = 1.0f / 60.0f;
        config.enableSceneCollision = true;

        for (int i = 0; i < 12; ++i)
        {
            probe.clothStats = StepSoftBody(probe.cloth, config, config.fixedDeltaSeconds, &probe.scene);
            probe.ropeStats = StepSoftBody(probe.rope, config, config.fixedDeltaSeconds, &probe.scene);
        }

        std::vector<SoftBodyComponent> components;
        SoftBodyComponent component{};
        component.body = probe.cloth;
        component.config = config;
        components.push_back(component);
        probe.systemStats = FixedUpdateSoftBodies(components, config.fixedDeltaSeconds, &probe.scene);

        const bool clothOk = probe.clothStats.finite && probe.clothStats.particleCount == static_cast<std::size_t>(clothDesc.columns) * static_cast<std::size_t>(clothDesc.rows);
        const bool ropeOk = probe.ropeStats.finite && probe.ropeStats.particleCount == ropeDesc.particleCount;
        const bool constraintOk = probe.clothStats.solvedConstraintCount > 0u && probe.ropeStats.solvedConstraintCount > 0u;
        const bool systemOk = probe.systemStats.finite && probe.systemStats.updatedBodyCount == 1u;
        probe.ok = clothOk && ropeOk && constraintOk && systemOk;

        std::ostringstream stream;
        stream << "[ " << (probe.ok ? "ok" : "fail") << " ] soft body / cloth XPBD foundation"
            << " cloth_particles=" << probe.clothStats.particleCount
            << " rope_particles=" << probe.ropeStats.particleCount
            << " cloth_constraints=" << probe.clothStats.constraintCount
            << " rope_constraints=" << probe.ropeStats.constraintCount
            << " solved=" << (probe.clothStats.solvedConstraintCount + probe.ropeStats.solvedConstraintCount)
            << " collisions=" << (probe.clothStats.collisionHitCount + probe.ropeStats.collisionHitCount)
            << " max_error=" << std::fixed << std::setprecision(5) << std::max(probe.clothStats.maxConstraintError, probe.ropeStats.maxConstraintError)
            << " system_updates=" << probe.systemStats.updatedBodyCount
            << " finite=" << (probe.clothStats.finite && probe.ropeStats.finite ? "true" : "false");
        probe.summary = stream.str();
        return probe;
    }

    std::string BuildSoftBodyProbeSummary()
    {
        return BuildSoftBodyProbe().summary;
    }
}
