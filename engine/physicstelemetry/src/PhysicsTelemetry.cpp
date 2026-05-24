#include <AK/PhysicsTelemetry/PhysicsTelemetry.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr u64 FnvOffset = 1469598103934665603ull;
        constexpr u64 FnvPrime = 1099511628211ull;

        u64 HashBytes(u64 hash, const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const u8*>(data);
            for (std::size_t i = 0; i < size; ++i)
            {
                hash ^= static_cast<u64>(bytes[i]);
                hash *= FnvPrime;
            }
            return hash;
        }

        template <typename T>
        u64 HashValue(u64 hash, const T& value)
        {
            return HashBytes(hash, &value, sizeof(T));
        }

        u64 HashString(u64 hash, const std::string& text)
        {
            const u64 size = static_cast<u64>(text.size());
            hash = HashValue(hash, size);
            return HashBytes(hash, text.data(), text.size());
        }

        i64 Quantize(float value, float quantum)
        {
            if (!std::isfinite(value))
            {
                return std::numeric_limits<i64>::min();
            }
            const float safeQuantum = std::max(quantum, 0.000001f);
            return static_cast<i64>(std::llround(static_cast<double>(value) / static_cast<double>(safeQuantum)));
        }

        u64 HashQuantizedFloat(u64 hash, float value, float quantum)
        {
            const i64 q = Quantize(value, quantum);
            return HashValue(hash, q);
        }

        u64 HashVec3(u64 hash, Vec3 value, float quantum)
        {
            hash = HashQuantizedFloat(hash, value.x, quantum);
            hash = HashQuantizedFloat(hash, value.y, quantum);
            hash = HashQuantizedFloat(hash, value.z, quantum);
            return hash;
        }

        u64 HashQuat(u64 hash, Quat value, float quantum)
        {
            hash = HashQuantizedFloat(hash, value.x, quantum);
            hash = HashQuantizedFloat(hash, value.y, quantum);
            hash = HashQuantizedFloat(hash, value.z, quantum);
            hash = HashQuantizedFloat(hash, value.w, quantum);
            return hash;
        }

        u64 HashAabb(u64 hash, AABB3 bounds, float quantum)
        {
            hash = HashVec3(hash, bounds.min, quantum);
            hash = HashVec3(hash, bounds.max, quantum);
            return hash;
        }

        std::vector<PhysicsBody> SortedBodies(const PhysicsScene& scene)
        {
            std::vector<PhysicsBody> bodies = scene.bodies;
            std::sort(bodies.begin(), bodies.end(), [](const PhysicsBody& a, const PhysicsBody& b) {
                return a.id < b.id;
            });
            return bodies;
        }

        std::vector<PhysicsCollider> SortedColliders(const PhysicsScene& scene)
        {
            std::vector<PhysicsCollider> colliders = scene.colliders;
            std::sort(colliders.begin(), colliders.end(), [](const PhysicsCollider& a, const PhysicsCollider& b) {
                if (a.bodyId != b.bodyId)
                {
                    return a.bodyId < b.bodyId;
                }
                if (a.kind != b.kind)
                {
                    return static_cast<u32>(a.kind) < static_cast<u32>(b.kind);
                }
                return a.debugName < b.debugName;
            });
            return colliders;
        }

        std::vector<PhysicsEvent> SortedEvents(const PhysicsScene& scene)
        {
            std::vector<PhysicsEvent> events = scene.events;
            std::sort(events.begin(), events.end(), [](const PhysicsEvent& a, const PhysicsEvent& b) {
                if (a.bodyA != b.bodyA) return a.bodyA < b.bodyA;
                if (a.bodyB != b.bodyB) return a.bodyB < b.bodyB;
                if (a.kind != b.kind) return static_cast<u32>(a.kind) < static_cast<u32>(b.kind);
                return a.age < b.age;
            });
            return events;
        }

        std::vector<PhysicsContactCacheEntry> SortedContactCache(const PhysicsScene& scene)
        {
            std::vector<PhysicsContactCacheEntry> cache = scene.contactCache;
            std::sort(cache.begin(), cache.end(), [](const PhysicsContactCacheEntry& a, const PhysicsContactCacheEntry& b) {
                return a.pairKey < b.pairKey;
            });
            return cache;
        }

        void PushBudgetWarning(std::vector<std::string>* warnings, const std::string& warning)
        {
            if (warnings)
            {
                warnings->push_back(warning);
            }
        }

        PhysicsTelemetryBudgetStatus Escalate(PhysicsTelemetryBudgetStatus current, PhysicsTelemetryBudgetStatus next)
        {
            return static_cast<u32>(next) > static_cast<u32>(current) ? next : current;
        }

        PhysicsScene BuildTelemetryProbeScene()
        {
            PhysicsScene scene{};
            scene.config.fixedDeltaSeconds = 1.0f / 60.0f;
            scene.config.broadphaseGridCellSize = 0.75f;
            scene.config.solverIterations = 4;
            scene.config.constraintIterations = 3;
            scene.config.enableContinuousCollision = true;
            scene.config.enableSpatialBroadphase = true;
            scene.config.enableSleeping = true;

            PhysicsBody floor = MakeStaticBody(1, {0.0f, -0.50f, 0.0f});
            PhysicsBody crate = MakeDynamicBody(2, {0.0f, 0.26f, 0.0f}, 4.0f);
            PhysicsBody fast = MakeDynamicBody(3, {-2.0f, 0.42f, 0.0f}, 0.35f);
            fast.velocity = {18.0f, 0.0f, 0.0f};
            fast.continuousCollision = true;
            PhysicsBody anchor = MakeStaticBody(4, {1.25f, 0.75f, 0.0f});
            PhysicsBody pendulum = MakeDynamicBody(5, {1.25f, 0.10f, 0.0f}, 1.5f);
            pendulum.velocity = {0.4f, 0.0f, 0.0f};

            scene.bodies.push_back(floor);
            scene.bodies.push_back(crate);
            scene.bodies.push_back(fast);
            scene.bodies.push_back(anchor);
            scene.bodies.push_back(pendulum);

            PhysicsCollider floorCollider = MakeBoxCollider(1, {3.0f, 0.25f, 3.0f});
            floorCollider.filter.layerMask = PhysicsLayer_Static;
            floorCollider.filter.collidesWithMask = PhysicsLayer_All;
            floorCollider.material = MakePhysicsMaterial(0.9f, 0.75f, 0.02f, 2400.0f);
            floorCollider.debugName = "telemetry_floor";

            PhysicsCollider crateCollider = MakeBoxCollider(2, {0.35f, 0.35f, 0.35f});
            crateCollider.filter.layerMask = PhysicsLayer_Dynamic;
            crateCollider.material = MakePhysicsMaterial(0.65f, 0.52f, 0.04f, 700.0f);
            crateCollider.debugName = "telemetry_crate";

            PhysicsCollider fastCollider = MakeSphereCollider(3, 0.12f);
            fastCollider.filter.layerMask = PhysicsLayer_Projectile;
            fastCollider.filter.collidesWithMask = PhysicsLayer_Static | PhysicsLayer_Dynamic;
            fastCollider.material = MakePhysicsMaterial(0.4f, 0.3f, 0.15f, 7800.0f);
            fastCollider.debugName = "telemetry_fast";

            PhysicsCollider anchorCollider = MakeSphereCollider(4, 0.08f);
            anchorCollider.filter.layerMask = PhysicsLayer_QueryOnly;
            anchorCollider.filter.collidesWithMask = 0u;
            anchorCollider.filter.queryOnly = true;
            anchorCollider.trigger = true;
            anchorCollider.debugName = "telemetry_anchor";

            PhysicsCollider pendulumCollider = MakeCapsuleCollider(5, 0.10f, 0.25f);
            pendulumCollider.filter.layerMask = PhysicsLayer_Dynamic;
            pendulumCollider.debugName = "telemetry_pendulum";

            scene.colliders.push_back(floorCollider);
            scene.colliders.push_back(crateCollider);
            scene.colliders.push_back(fastCollider);
            scene.colliders.push_back(anchorCollider);
            scene.colliders.push_back(pendulumCollider);
            scene.constraints.push_back(MakeDistanceConstraint(4, 5, 0.65f));
            return scene;
        }
    }

    const char* ToString(PhysicsTelemetryBudgetStatus status)
    {
        switch (status)
        {
        case PhysicsTelemetryBudgetStatus::Ok: return "Ok";
        case PhysicsTelemetryBudgetStatus::Warning: return "Warning";
        case PhysicsTelemetryBudgetStatus::Critical: return "Critical";
        }
        return "Unknown";
    }

    u64 HashPhysicsScene(const PhysicsScene& scene, const PhysicsHashConfig& config)
    {
        u64 hash = FnvOffset;
        hash = HashString(hash, "AK_PHYSICS_SCENE_HASH_V1");
        hash = HashVec3(hash, scene.config.gravity, config.velocityQuantumMetersPerSecond);
        hash = HashQuantizedFloat(hash, scene.config.fixedDeltaSeconds, 0.0000001f);
        hash = HashQuantizedFloat(hash, scene.config.broadphaseGridCellSize, config.positionQuantumMeters);
        hash = HashQuantizedFloat(hash, scene.config.defaultContactOffset, config.positionQuantumMeters);
        hash = HashQuantizedFloat(hash, scene.config.defaultRestOffset, config.positionQuantumMeters);
        hash = HashQuantizedFloat(hash, scene.config.staticLeakageGuardSkin, config.positionQuantumMeters);
        hash = HashQuantizedFloat(hash, scene.config.maxDepenetrationVelocity, config.velocityQuantumMetersPerSecond);
        hash = HashValue(hash, scene.config.enableSpeculativeContacts);
        hash = HashValue(hash, scene.config.enableStaticLeakageGuard);
        hash = HashValue(hash, scene.config.solverIterations);
        hash = HashValue(hash, scene.config.constraintIterations);

        const std::vector<PhysicsBody> bodies = SortedBodies(scene);
        const u64 bodyCount = static_cast<u64>(bodies.size());
        hash = HashValue(hash, bodyCount);
        for (const PhysicsBody& body : bodies)
        {
            hash = HashValue(hash, body.id);
            hash = HashValue(hash, static_cast<u32>(body.kind));
            hash = HashVec3(hash, body.position, config.positionQuantumMeters);
            hash = HashQuat(hash, body.orientation, config.positionQuantumMeters);
            hash = HashVec3(hash, body.velocity, config.velocityQuantumMetersPerSecond);
            hash = HashVec3(hash, body.accumulatedForce, config.velocityQuantumMetersPerSecond);
            hash = HashQuantizedFloat(hash, body.massKilograms, config.massQuantumKilograms);
            hash = HashQuantizedFloat(hash, body.inverseMass, config.massQuantumKilograms);
            hash = HashQuantizedFloat(hash, body.linearDamping, 0.0001f);
            hash = HashQuantizedFloat(hash, body.gravityScale, 0.0001f);
            hash = HashValue(hash, body.enabled);
            hash = HashValue(hash, body.continuousCollision);
            if (config.includeSleepingState)
            {
                hash = HashValue(hash, body.sleeping);
                hash = HashQuantizedFloat(hash, body.sleepTimerSeconds, 0.0001f);
            }
        }

        const std::vector<PhysicsCollider> colliders = SortedColliders(scene);
        const u64 colliderCount = static_cast<u64>(colliders.size());
        hash = HashValue(hash, colliderCount);
        for (const PhysicsCollider& collider : colliders)
        {
            hash = HashValue(hash, collider.bodyId);
            hash = HashValue(hash, static_cast<u32>(collider.kind));
            hash = HashVec3(hash, collider.localCenter, config.positionQuantumMeters);
            hash = HashQuat(hash, collider.localRotation, config.positionQuantumMeters);
            hash = HashVec3(hash, collider.halfExtents, config.positionQuantumMeters);
            hash = HashQuantizedFloat(hash, collider.radius, config.positionQuantumMeters);
            hash = HashQuantizedFloat(hash, collider.halfHeight, config.positionQuantumMeters);
            hash = HashQuantizedFloat(hash, collider.contactOffset, config.positionQuantumMeters);
            hash = HashQuantizedFloat(hash, collider.restOffset, config.positionQuantumMeters);
            hash = HashAabb(hash, collider.localBounds, config.positionQuantumMeters);
            hash = HashValue(hash, collider.enabled);
            hash = HashValue(hash, collider.trigger);
            hash = HashValue(hash, collider.filter.layerMask);
            hash = HashValue(hash, collider.filter.collidesWithMask);
            hash = HashValue(hash, collider.filter.eventMask);
            hash = HashValue(hash, collider.filter.queryOnly);
            hash = HashQuantizedFloat(hash, collider.material.staticFriction, 0.0001f);
            hash = HashQuantizedFloat(hash, collider.material.dynamicFriction, 0.0001f);
            hash = HashQuantizedFloat(hash, collider.material.restitution, 0.0001f);
            hash = HashQuantizedFloat(hash, collider.material.densityKgPerCubicMeter, 0.01f);
            hash = HashString(hash, collider.debugName);
        }

        const u64 constraintCount = static_cast<u64>(scene.constraints.size());
        hash = HashValue(hash, constraintCount);
        for (const PhysicsConstraint& constraint : scene.constraints)
        {
            hash = HashValue(hash, static_cast<u32>(constraint.kind));
            if (constraint.kind == PhysicsConstraintKind::Distance)
            {
                hash = HashValue(hash, constraint.distance.bodyA);
                hash = HashValue(hash, constraint.distance.bodyB);
                hash = HashVec3(hash, constraint.distance.localAnchorA, config.positionQuantumMeters);
                hash = HashVec3(hash, constraint.distance.localAnchorB, config.positionQuantumMeters);
                hash = HashQuantizedFloat(hash, constraint.distance.restDistance, config.positionQuantumMeters);
                hash = HashQuantizedFloat(hash, constraint.distance.minDistance, config.positionQuantumMeters);
                hash = HashQuantizedFloat(hash, constraint.distance.maxDistance, config.positionQuantumMeters);
                hash = HashQuantizedFloat(hash, constraint.distance.stiffness, 0.0001f);
                hash = HashValue(hash, constraint.distance.enabled);
            }
        }

        if (config.includeContactCache)
        {
            const std::vector<PhysicsContactCacheEntry> cache = SortedContactCache(scene);
            hash = HashValue(hash, static_cast<u64>(cache.size()));
            for (const PhysicsContactCacheEntry& entry : cache)
            {
                hash = HashValue(hash, entry.pairKey);
                hash = HashVec3(hash, entry.normal, config.contactQuantumMeters);
                hash = HashVec3(hash, entry.point, config.contactQuantumMeters);
                hash = HashQuantizedFloat(hash, entry.penetration, config.contactQuantumMeters);
                hash = HashValue(hash, entry.age);
                hash = HashValue(hash, entry.touching);
            }
        }

        if (config.includeEvents)
        {
            const std::vector<PhysicsEvent> events = SortedEvents(scene);
            hash = HashValue(hash, static_cast<u64>(events.size()));
            for (const PhysicsEvent& event : events)
            {
                hash = HashValue(hash, static_cast<u32>(event.kind));
                hash = HashValue(hash, event.bodyA);
                hash = HashValue(hash, event.bodyB);
                hash = HashVec3(hash, event.point, config.contactQuantumMeters);
                hash = HashVec3(hash, event.normal, config.contactQuantumMeters);
                hash = HashQuantizedFloat(hash, event.penetration, config.contactQuantumMeters);
                hash = HashValue(hash, event.age);
            }
        }

        return hash;
    }

    u64 HashPhysicsStepStats(const PhysicsStepStats& stats)
    {
        u64 hash = FnvOffset;
        hash = HashString(hash, "AK_PHYSICS_STEP_STATS_HASH_V1");
        hash = HashValue(hash, static_cast<u64>(stats.bodyCount));
        hash = HashValue(hash, static_cast<u64>(stats.colliderCount));
        hash = HashValue(hash, static_cast<u64>(stats.dynamicBodyCount));
        hash = HashValue(hash, static_cast<u64>(stats.integratedBodyCount));
        hash = HashValue(hash, static_cast<u64>(stats.broadphasePairCount));
        hash = HashValue(hash, static_cast<u64>(stats.contactCount));
        hash = HashValue(hash, static_cast<u64>(stats.manifoldCount));
        hash = HashValue(hash, static_cast<u64>(stats.resolvedContactCount));
        hash = HashValue(hash, static_cast<u64>(stats.persistentContactCount));
        hash = HashValue(hash, static_cast<u64>(stats.islandCount));
        hash = HashValue(hash, static_cast<u64>(stats.sleepingBodyCount));
        hash = HashValue(hash, static_cast<u64>(stats.ccdSweepCount));
        hash = HashValue(hash, static_cast<u64>(stats.ccdHitCount));
        hash = HashValue(hash, static_cast<u64>(stats.constraintCount));
        hash = HashValue(hash, static_cast<u64>(stats.solvedConstraintCount));
        hash = HashValue(hash, static_cast<u64>(stats.triggerContactCount));
        hash = HashValue(hash, static_cast<u64>(stats.filteredPairCount));
        hash = HashValue(hash, static_cast<u64>(stats.materialPairCount));
        hash = HashValue(hash, stats.substeps);
        hash = HashValue(hash, stats.finite);
        hash = HashValue(hash, static_cast<u64>(stats.warnings.size()));
        return hash;
    }

    PhysicsTelemetryBudgetStatus EvaluatePhysicsBudget(const PhysicsStepStats& stats, const PhysicsFrameBudget& budget, std::vector<std::string>* outWarnings)
    {
        PhysicsTelemetryBudgetStatus status = PhysicsTelemetryBudgetStatus::Ok;
        if (budget.requireFinite && !stats.finite)
        {
            PushBudgetWarning(outWarnings, "physics stats contain non-finite values");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Critical);
        }
        if (stats.bodyCount > budget.maxBodies)
        {
            PushBudgetWarning(outWarnings, "body budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Critical);
        }
        if (stats.colliderCount > budget.maxColliders)
        {
            PushBudgetWarning(outWarnings, "collider budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Critical);
        }
        if (stats.broadphasePairCount > budget.maxBroadphasePairs)
        {
            PushBudgetWarning(outWarnings, "broadphase pair budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Warning);
        }
        if (stats.contactCount > budget.maxContacts)
        {
            PushBudgetWarning(outWarnings, "contact budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Warning);
        }
        if (stats.manifoldPointCount > budget.maxManifoldPoints)
        {
            PushBudgetWarning(outWarnings, "manifold point budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Warning);
        }
        const std::size_t eventCount = stats.contactStartedEventCount + stats.contactStayedEventCount + stats.contactEndedEventCount + stats.triggerEventCount;
        if (eventCount > budget.maxEvents)
        {
            PushBudgetWarning(outWarnings, "physics event budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Warning);
        }
        if (stats.ccdSweepCount > budget.maxCcdSweeps)
        {
            PushBudgetWarning(outWarnings, "CCD sweep budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Warning);
        }
        if (stats.simulatedSeconds > budget.maxSimulatedSecondsPerFrame)
        {
            PushBudgetWarning(outWarnings, "simulated seconds per frame budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Warning);
        }
        if (stats.warnings.size() > budget.maxWarnings)
        {
            PushBudgetWarning(outWarnings, "physics warning budget exceeded");
            status = Escalate(status, PhysicsTelemetryBudgetStatus::Warning);
        }
        return status;
    }

    PhysicsFrameTelemetry CapturePhysicsFrameTelemetry(const PhysicsScene& sceneBefore, const PhysicsScene& sceneAfter, const PhysicsStepStats& stats, u64 frameIndex, u64 fixedTick, const PhysicsHashConfig& hashConfig, const PhysicsFrameBudget& budget)
    {
        PhysicsFrameTelemetry frame{};
        frame.frameIndex = frameIndex;
        frame.fixedTick = fixedTick;
        frame.sceneHashBefore = HashPhysicsScene(sceneBefore, hashConfig);
        frame.sceneHashAfter = HashPhysicsScene(sceneAfter, hashConfig);
        frame.statsHash = HashPhysicsStepStats(stats);
        frame.stats = stats;
        frame.finite = stats.finite;
        frame.deterministicHashChanged = frame.sceneHashBefore != frame.sceneHashAfter;
        frame.budgetStatus = EvaluatePhysicsBudget(stats, budget, &frame.warnings);
        for (const std::string& warning : stats.warnings)
        {
            frame.warnings.push_back(warning);
        }
        return frame;
    }

    PhysicsReplayResult RunPhysicsDeterminismReplay(PhysicsScene scene, const PhysicsReplayConfig& config)
    {
        PhysicsReplayResult result{};
        const u32 stepCount = std::max(1u, config.stepCount);

        auto run = [&](PhysicsScene runScene) {
            std::vector<PhysicsReplayFrame> frames;
            frames.reserve(stepCount);
            for (u32 i = 0; i < stepCount; ++i)
            {
                PhysicsReplayFrame frame{};
                frame.stepIndex = i;
                frame.hashBefore = HashPhysicsScene(runScene, config.hashConfig);
                frame.stats = StepPhysics(runScene, config.fixedDeltaSeconds);
                frame.hashAfter = HashPhysicsScene(runScene, config.hashConfig);
                frame.statsHash = HashPhysicsStepStats(frame.stats);
                frame.finite = frame.stats.finite;
                frame.budgetStatus = EvaluatePhysicsBudget(frame.stats, config.budget, nullptr);
                frames.push_back(frame);
            }
            return frames;
        };

        result.firstRun = run(scene);
        result.secondRun = run(scene);
        result.firstRunFinalHash = result.firstRun.empty() ? HashPhysicsScene(scene, config.hashConfig) : result.firstRun.back().hashAfter;
        result.secondRunFinalHash = result.secondRun.empty() ? HashPhysicsScene(scene, config.hashConfig) : result.secondRun.back().hashAfter;
        result.deterministic = result.firstRun.size() == result.secondRun.size();
        result.mismatchStep = stepCount;

        if (config.compareFrameHashes)
        {
            for (u32 i = 0; i < stepCount && i < result.firstRun.size() && i < result.secondRun.size(); ++i)
            {
                const PhysicsReplayFrame& a = result.firstRun[i];
                const PhysicsReplayFrame& b = result.secondRun[i];
                if (a.hashBefore != b.hashBefore || a.hashAfter != b.hashAfter || a.statsHash != b.statsHash || a.finite != b.finite)
                {
                    result.deterministic = false;
                    result.mismatchStep = i;
                    result.warnings.push_back("determinism replay mismatch");
                    break;
                }
            }
        }
        else
        {
            result.deterministic = result.firstRunFinalHash == result.secondRunFinalHash;
            if (!result.deterministic)
            {
                result.warnings.push_back("final scene hash mismatch");
            }
        }

        for (const PhysicsReplayFrame& frame : result.firstRun)
        {
            if (!frame.finite)
            {
                result.warnings.push_back("non-finite frame in replay");
                result.deterministic = false;
                break;
            }
            if (frame.budgetStatus == PhysicsTelemetryBudgetStatus::Critical)
            {
                result.warnings.push_back("critical physics budget status in replay");
                break;
            }
        }

        result.ok = result.deterministic && result.warnings.empty();
        result.summary = BuildPhysicsTelemetryProbeSummary();
        return result;
    }

    std::string ToDebugString(const PhysicsFrameTelemetry& frame)
    {
        std::ostringstream out;
        out << "physics_telemetry frame=" << frame.frameIndex
            << " tick=" << frame.fixedTick
            << " status=" << ToString(frame.budgetStatus)
            << " hash_before=0x" << std::hex << frame.sceneHashBefore
            << " hash_after=0x" << frame.sceneHashAfter
            << " stats=0x" << frame.statsHash << std::dec
            << " bodies=" << frame.stats.bodyCount
            << " colliders=" << frame.stats.colliderCount
            << " pairs=" << frame.stats.broadphasePairCount
            << " contacts=" << frame.stats.contactCount
            << " events=" << (frame.stats.contactStartedEventCount + frame.stats.contactStayedEventCount + frame.stats.contactEndedEventCount + frame.stats.triggerEventCount)
            << " ccd=" << frame.stats.ccdHitCount << '/' << frame.stats.ccdSweepCount
            << " constraints=" << frame.stats.solvedConstraintCount << '/' << frame.stats.constraintCount
            << " finite=" << (frame.finite ? "true" : "false")
            << " warnings=" << frame.warnings.size();
        return out.str();
    }

    std::string ToDebugString(const PhysicsReplayFrame& frame)
    {
        std::ostringstream out;
        out << "replay_frame step=" << frame.stepIndex
            << " status=" << ToString(frame.budgetStatus)
            << " before=0x" << std::hex << frame.hashBefore
            << " after=0x" << frame.hashAfter
            << " stats=0x" << frame.statsHash << std::dec
            << " contacts=" << frame.stats.contactCount
            << " events=" << (frame.stats.contactStartedEventCount + frame.stats.contactStayedEventCount + frame.stats.contactEndedEventCount + frame.stats.triggerEventCount)
            << " finite=" << (frame.finite ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PhysicsReplayResult& replay)
    {
        std::ostringstream out;
        out << "physics_replay deterministic=" << (replay.deterministic ? "true" : "false")
            << " ok=" << (replay.ok ? "true" : "false")
            << " steps=" << replay.firstRun.size()
            << " mismatch=" << replay.mismatchStep
            << " final_a=0x" << std::hex << replay.firstRunFinalHash
            << " final_b=0x" << replay.secondRunFinalHash << std::dec
            << " warnings=" << replay.warnings.size();
        return out.str();
    }

    std::string BuildPhysicsTelemetryProbeSummary()
    {
        return "Physics telemetry / determinism / replay foundation";
    }

    PhysicsTelemetryProbeResult BuildPhysicsTelemetryProbe()
    {
        PhysicsTelemetryProbeResult result{};
        PhysicsHashConfig hashConfig{};
        PhysicsFrameBudget budget{};
        budget.maxBodies = 32;
        budget.maxColliders = 64;
        budget.maxBroadphasePairs = 128;
        budget.maxContacts = 64;
        budget.maxManifoldPoints = 256;
        budget.maxEvents = 64;
        budget.maxCcdSweeps = 16;
        budget.maxSimulatedSecondsPerFrame = 1.0f / 20.0f;

        PhysicsScene before = BuildTelemetryProbeScene();
        result.scene = before;
        PhysicsStepStats stats = StepPhysics(result.scene, 1.0f / 60.0f);
        result.frame = CapturePhysicsFrameTelemetry(before, result.scene, stats, 1, 1, hashConfig, budget);

        PhysicsReplayConfig replayConfig{};
        replayConfig.stepCount = 10;
        replayConfig.fixedDeltaSeconds = 1.0f / 60.0f;
        replayConfig.hashConfig = hashConfig;
        replayConfig.budget = budget;
        result.replay = RunPhysicsDeterminismReplay(BuildTelemetryProbeScene(), replayConfig);

        result.ok = result.frame.finite && result.frame.budgetStatus != PhysicsTelemetryBudgetStatus::Critical && result.replay.ok;

        std::ostringstream summary;
        summary << "[ ok ] physics telemetry/determinism/replay foundation"
                << " bodies=" << result.frame.stats.bodyCount
                << " colliders=" << result.frame.stats.colliderCount
                << " pairs=" << result.frame.stats.broadphasePairCount
                << " contacts=" << result.frame.stats.contactCount
                << " events=" << (result.frame.stats.contactStartedEventCount + result.frame.stats.contactStayedEventCount + result.frame.stats.contactEndedEventCount + result.frame.stats.triggerEventCount)
                << " ccd=" << result.frame.stats.ccdHitCount << '/' << result.frame.stats.ccdSweepCount
                << " replay_steps=" << result.replay.firstRun.size()
                << " deterministic=" << (result.replay.deterministic ? "true" : "false")
                << " status=" << ToString(result.frame.budgetStatus)
                << " hash=0x" << std::hex << result.frame.sceneHashAfter << std::dec
                << " finite=" << (result.frame.finite ? "true" : "false")
                << " warnings=" << (result.frame.warnings.size() + result.replay.warnings.size());
        result.summary = summary.str();
        if (!result.ok)
        {
            result.summary.replace(2, 2, "fail");
        }
        return result;
    }
}
