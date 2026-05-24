#include <AK/Core/Time.hpp>

#include <algorithm>
#include <cmath>

namespace AK
{
    Stopwatch::Stopwatch()
        : mStart(Clock::now())
    {
    }

    void Stopwatch::Reset()
    {
        mStart = Clock::now();
    }

    double Stopwatch::ElapsedSeconds() const
    {
        const auto now = Clock::now();
        const std::chrono::duration<double> elapsed = now - mStart;
        return elapsed.count();
    }

    double Stopwatch::RestartSeconds()
    {
        const auto now = Clock::now();
        const std::chrono::duration<double> elapsed = now - mStart;
        mStart = now;
        return elapsed.count();
    }

    double SanitizeDeltaSeconds(double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0.0)
        {
            return 0.0;
        }

        return seconds;
    }

    double ClampFrameDeltaSeconds(double seconds, double maxFrameDeltaSeconds)
    {
        const double safeSeconds = SanitizeDeltaSeconds(seconds);
        const double safeMax = maxFrameDeltaSeconds > 0.0 ? maxFrameDeltaSeconds : 0.25;
        return std::min(safeSeconds, safeMax);
    }

    GameClock::GameClock(const ClockConfig& config)
    {
        Reset(config);
    }

    void GameClock::Reset()
    {
        Reset(mConfig);
    }

    void GameClock::Reset(const ClockConfig& config)
    {
        mConfig = config;
        if (!std::isfinite(mConfig.fixedDeltaSeconds) || mConfig.fixedDeltaSeconds <= 0.0)
        {
            mConfig.fixedDeltaSeconds = 1.0 / 60.0;
        }
        if (!std::isfinite(mConfig.maxFrameDeltaSeconds) || mConfig.maxFrameDeltaSeconds <= 0.0)
        {
            mConfig.maxFrameDeltaSeconds = 0.25;
        }
        if (mConfig.maxFixedStepsPerFrame == 0)
        {
            mConfig.maxFixedStepsPerFrame = 1;
        }

        mTiming = {};
        mTiming.fixedDeltaSeconds = mConfig.fixedDeltaSeconds;
    }

    void GameClock::BeginFrame(double realFrameDeltaSeconds)
    {
        const double safeDelta = SanitizeDeltaSeconds(realFrameDeltaSeconds);
        const double clampedDelta = ClampFrameDeltaSeconds(safeDelta, mConfig.maxFrameDeltaSeconds);

        ++mTiming.frameIndex;
        mTiming.unclampedFrameDeltaSeconds = safeDelta;
        mTiming.frameDeltaSeconds = clampedDelta;
        mTiming.frameDeltaClamped = safeDelta != clampedDelta;
        mTiming.realTimeSeconds += safeDelta;
        mTiming.fixedDeltaSeconds = mConfig.fixedDeltaSeconds;
        mTiming.fixedStepsThisFrame = 0;
        mTiming.pendingFixedSteps = 0;
        mTiming.droppedFixedSteps = 0;
        mTiming.accumulatorSeconds += clampedDelta;

        while (mTiming.accumulatorSeconds + 0.000000001 >= mConfig.fixedDeltaSeconds
            && mTiming.pendingFixedSteps < mConfig.maxFixedStepsPerFrame)
        {
            mTiming.accumulatorSeconds -= mConfig.fixedDeltaSeconds;
            ++mTiming.pendingFixedSteps;
            ++mTiming.fixedStepsThisFrame;
        }

        while (mTiming.accumulatorSeconds + 0.000000001 >= mConfig.fixedDeltaSeconds)
        {
            mTiming.accumulatorSeconds -= mConfig.fixedDeltaSeconds;
            ++mTiming.droppedFixedSteps;
        }

        mTiming.interpolationAlpha = std::clamp(mTiming.accumulatorSeconds / mConfig.fixedDeltaSeconds, 0.0, 1.0);
    }

    bool GameClock::ConsumeFixedStep()
    {
        if (mTiming.pendingFixedSteps == 0)
        {
            return false;
        }

        --mTiming.pendingFixedSteps;
        ++mTiming.fixedTick;
        mTiming.simulationTimeSeconds = static_cast<double>(mTiming.fixedTick) * mConfig.fixedDeltaSeconds;
        return true;
    }

    const ClockConfig& GameClock::Config() const
    {
        return mConfig;
    }

    FrameTiming GameClock::Snapshot() const
    {
        return mTiming;
    }

    FrameIndex GameClock::CurrentFrame() const
    {
        return mTiming.frameIndex;
    }

    Tick GameClock::CurrentTick() const
    {
        return mTiming.fixedTick;
    }
}
