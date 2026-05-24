#pragma once

#include <chrono>
#include <cstdint>

namespace AK
{
    using Tick = std::uint64_t;
    using FrameIndex = std::uint64_t;

    class Stopwatch final
    {
    public:
        Stopwatch();

        void Reset();
        double ElapsedSeconds() const;
        double RestartSeconds();

    private:
        using Clock = std::chrono::steady_clock;

        Clock::time_point mStart;
    };

    struct ClockConfig
    {
        double fixedDeltaSeconds = 1.0 / 60.0;
        double maxFrameDeltaSeconds = 0.25;
        std::uint32_t maxFixedStepsPerFrame = 8;
    };

    struct FrameTiming
    {
        FrameIndex frameIndex = 0;
        Tick fixedTick = 0;
        double realTimeSeconds = 0.0;
        double simulationTimeSeconds = 0.0;
        double frameDeltaSeconds = 0.0;
        double unclampedFrameDeltaSeconds = 0.0;
        double fixedDeltaSeconds = 1.0 / 60.0;
        double accumulatorSeconds = 0.0;
        double interpolationAlpha = 0.0;
        std::uint32_t fixedStepsThisFrame = 0;
        std::uint32_t pendingFixedSteps = 0;
        std::uint32_t droppedFixedSteps = 0;
        bool frameDeltaClamped = false;
    };

    class GameClock final
    {
    public:
        explicit GameClock(const ClockConfig& config = {});

        void Reset();
        void Reset(const ClockConfig& config);
        void BeginFrame(double realFrameDeltaSeconds);
        bool ConsumeFixedStep();

        const ClockConfig& Config() const;
        FrameTiming Snapshot() const;
        FrameIndex CurrentFrame() const;
        Tick CurrentTick() const;

    private:
        ClockConfig mConfig{};
        FrameTiming mTiming{};
    };

    double SanitizeDeltaSeconds(double seconds);
    double ClampFrameDeltaSeconds(double seconds, double maxFrameDeltaSeconds);
}
