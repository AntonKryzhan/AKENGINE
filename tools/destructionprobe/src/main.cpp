#include <AK/Destruction/Destruction.hpp>

#include <iostream>

int main()
{
    const AK::DestructionProbeResult probe = AK::BuildDestructionProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.result) << '\n';
    std::cout << AK::ToDebugString(probe.result.collisionProxies) << '\n';
    std::cout << AK::ToDebugString(probe.resourceStats) << '\n';
    std::cout << AK::ToDebugString(probe.partitionStats) << '\n';
    std::cout << AK::ToDebugString(probe.diagnostics) << '\n';
    return probe.ok ? 0 : 1;
}
