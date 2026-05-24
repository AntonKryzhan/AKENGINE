#include <AK/Surface/Surface.hpp>

#include <iostream>

int main()
{
    const AK::SurfaceProbeResult probe = AK::BuildSurfaceProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.registryStats) << '\n';
    std::cout << AK::ToDebugString(probe.grassSample) << '\n';
    std::cout << AK::ToDebugString(probe.steepSample) << '\n';
    std::cout << AK::ToDebugString(probe.planetSample) << '\n';
    std::cout << AK::ToDebugString(probe.rubberConcreteContact) << '\n';
    std::cout << AK::ToDebugString(probe.physicsStats) << '\n';
    return probe.ok ? 0 : 1;
}
