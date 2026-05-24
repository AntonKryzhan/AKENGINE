#include <AK/Remesh/Remesh.hpp>

#include <iostream>

int main()
{
    const AK::RemeshProbeResult probe = AK::BuildRemeshProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.remesh.stats) << '\n';
    std::cout << AK::ToDebugString(probe.remesh.mesh) << '\n';
    return probe.ok ? 0 : 1;
}
