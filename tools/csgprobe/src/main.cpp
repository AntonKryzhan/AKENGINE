#include <AK/CSG/Boolean.hpp>

#include <iostream>

int main()
{
    const AK::CsgProbeResult probe = AK::BuildCsgProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.primitiveDifference.stats) << '\n';
    std::cout << AK::ToDebugString(probe.meshDifference.stats) << '\n';
    return probe.ok ? 0 : 1;
}
