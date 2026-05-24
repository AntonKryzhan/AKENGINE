#include <AK/PhysicsTelemetry/PhysicsTelemetry.hpp>

#include <iostream>

int main()
{
    const AK::PhysicsTelemetryProbeResult probe = AK::BuildPhysicsTelemetryProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.frame) << '\n';
    std::cout << AK::ToDebugString(probe.replay) << '\n';
    if (!probe.replay.firstRun.empty())
    {
        std::cout << AK::ToDebugString(probe.replay.firstRun.front()) << '\n';
        std::cout << AK::ToDebugString(probe.replay.firstRun.back()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
