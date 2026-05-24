#include <AK/Core/Time.hpp>

#include <cmath>
#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "ak_clockprobe failed: " << message << '\n';
        return 1;
    }

    bool NearlyEqual(double a, double b, double eps = 0.000000001)
    {
        return std::fabs(a - b) <= eps;
    }
}

int main()
{
    AK::GameClock clock({1.0 / 60.0, 0.25, 4});

    clock.BeginFrame(1.0 / 60.0);
    AK::FrameTiming timing = clock.Snapshot();
    if (timing.frameIndex != 1 || timing.fixedStepsThisFrame != 1 || timing.pendingFixedSteps != 1)
    {
        return Fail("one fixed step was not scheduled for 60 Hz frame");
    }

    if (!clock.ConsumeFixedStep() || clock.CurrentTick() != 1)
    {
        return Fail("fixed step consumption failed");
    }

    clock.BeginFrame(-1.0);
    timing = clock.Snapshot();
    if (!NearlyEqual(timing.frameDeltaSeconds, 0.0))
    {
        return Fail("negative delta was not sanitized");
    }

    clock.BeginFrame(10.0);
    timing = clock.Snapshot();
    if (!timing.frameDeltaClamped || !NearlyEqual(timing.frameDeltaSeconds, 0.25))
    {
        return Fail("large frame delta was not clamped");
    }
    if (timing.fixedStepsThisFrame != 4 || timing.droppedFixedSteps == 0)
    {
        return Fail("spiral-of-death guard did not cap fixed steps");
    }

    std::uint32_t consumed = 0;
    while (clock.ConsumeFixedStep())
    {
        ++consumed;
    }
    if (consumed != 4)
    {
        return Fail("unexpected number of consumed capped fixed steps");
    }

    timing = clock.Snapshot();
    std::cout << "AK clock probe OK\n";
    std::cout << "frame=" << timing.frameIndex << " tick=" << timing.fixedTick << '\n';
    std::cout << "dt=" << timing.frameDeltaSeconds << " fixed_dt=" << timing.fixedDeltaSeconds << '\n';
    std::cout << "alpha=" << timing.interpolationAlpha << " dropped=" << timing.droppedFixedSteps << '\n';
    return 0;
}
