#include <AK/WorldTopology/WorldTopology.hpp>

#include <iostream>

int main()
{
    const AK::WorldTopologyProbeResult probe = AK::BuildWorldTopologyProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.earthEquator) << '\n';
    std::cout << AK::ToDebugString(probe.planetFrame) << '\n';
    std::cout << AK::ToDebugString(probe.wrapped) << '\n';
    std::cout << "shortest torus delta " << AK::ToDebugString(probe.shortestToroidalDelta) << '\n';
    std::cout << AK::ToDebugString(probe.validation) << '\n';
    return probe.ok ? 0 : 1;
}
