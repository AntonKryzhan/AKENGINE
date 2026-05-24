#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Physics/Physics.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class PhysicsTelemetryBudgetStatus : u32
    {
        Ok = 0,
        Warning = 1,
        Critical = 2
    };

    struct PhysicsHashConfig
    {
        float positionQuantumMeters = 0.0001f;
        float velocityQuantumMetersPerSecond = 0.0001f;
        float massQuantumKilograms = 0.0001f;
        float contactQuantumMeters = 0.0001f;
        bool includeSleepingState = true;
        bool includeEvents = true;
        bool includeContactCache = true;
    };

    struct PhysicsFrameBudget
    {
        std::size_t maxBodies = 20000;
        std::size_t maxColliders = 40000;
        std::size_t maxBroadphasePairs = 120000;
        std::size_t maxContacts = 60000;
        std::size_t maxManifoldPoints = 180000;
        std::size_t maxEvents = 120000;
        std::size_t maxCcdSweeps = 8000;
        std::size_t maxWarnings = 0;
        float maxSimulatedSecondsPerFrame = 1.0f / 15.0f;
        bool requireFinite = true;
    };

    struct PhysicsFrameTelemetry
    {
        u64 frameIndex = 0;
        u64 fixedTick = 0;
        u64 sceneHashBefore = 0;
        u64 sceneHashAfter = 0;
        u64 statsHash = 0;
        PhysicsStepStats stats{};
        PhysicsTelemetryBudgetStatus budgetStatus = PhysicsTelemetryBudgetStatus::Ok;
        bool finite = true;
        bool deterministicHashChanged = false;
        std::vector<std::string> warnings;
    };

    struct PhysicsReplayConfig
    {
        u32 stepCount = 8;
        float fixedDeltaSeconds = 1.0f / 60.0f;
        PhysicsHashConfig hashConfig{};
        PhysicsFrameBudget budget{};
        bool compareFrameHashes = true;
    };

    struct PhysicsReplayFrame
    {
        u32 stepIndex = 0;
        u64 hashBefore = 0;
        u64 hashAfter = 0;
        u64 statsHash = 0;
        PhysicsStepStats stats{};
        PhysicsTelemetryBudgetStatus budgetStatus = PhysicsTelemetryBudgetStatus::Ok;
        bool finite = true;
    };

    struct PhysicsReplayResult
    {
        bool ok = false;
        bool deterministic = false;
        u32 mismatchStep = 0;
        u64 firstRunFinalHash = 0;
        u64 secondRunFinalHash = 0;
        std::vector<PhysicsReplayFrame> firstRun;
        std::vector<PhysicsReplayFrame> secondRun;
        std::vector<std::string> warnings;
        std::string summary;
    };

    struct PhysicsTelemetryProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        PhysicsFrameTelemetry frame{};
        PhysicsReplayResult replay{};
    };

    const char* ToString(PhysicsTelemetryBudgetStatus status);

    u64 HashPhysicsScene(const PhysicsScene& scene, const PhysicsHashConfig& config = {});
    u64 HashPhysicsStepStats(const PhysicsStepStats& stats);
    PhysicsTelemetryBudgetStatus EvaluatePhysicsBudget(const PhysicsStepStats& stats, const PhysicsFrameBudget& budget, std::vector<std::string>* outWarnings = nullptr);
    PhysicsFrameTelemetry CapturePhysicsFrameTelemetry(const PhysicsScene& sceneBefore, const PhysicsScene& sceneAfter, const PhysicsStepStats& stats, u64 frameIndex, u64 fixedTick, const PhysicsHashConfig& hashConfig = {}, const PhysicsFrameBudget& budget = {});
    PhysicsReplayResult RunPhysicsDeterminismReplay(PhysicsScene scene, const PhysicsReplayConfig& config = {});

    std::string ToDebugString(const PhysicsFrameTelemetry& frame);
    std::string ToDebugString(const PhysicsReplayFrame& frame);
    std::string ToDebugString(const PhysicsReplayResult& replay);
    std::string BuildPhysicsTelemetryProbeSummary();
    PhysicsTelemetryProbeResult BuildPhysicsTelemetryProbe();
}
