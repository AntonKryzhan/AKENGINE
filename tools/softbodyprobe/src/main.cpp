#include <AK/SoftBody/SoftBody.hpp>

#include <iostream>

int main()
{
    const AK::SoftBodyProbeResult probe = AK::BuildSoftBodyProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.cloth) << '\n';
    std::cout << AK::ToDebugString(probe.clothStats) << '\n';
    std::cout << AK::ToDebugString(probe.rope) << '\n';
    std::cout << AK::ToDebugString(probe.ropeStats) << '\n';
    std::cout << AK::ToDebugString(probe.systemStats) << '\n';
    return probe.ok ? 0 : 1;
}
