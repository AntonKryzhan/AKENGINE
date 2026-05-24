#include <AK/Ragdoll/Ragdoll.hpp>

#include <iostream>

int main()
{
    const AK::RagdollProbeResult probe = AK::BuildRagdollProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.config) << '\n';
    std::cout << AK::ToDebugString(probe.stats) << '\n';
    std::cout << AK::ToDebugString(probe.physicsStats) << '\n';
    if (!probe.pose.empty())
    {
        std::cout << AK::ToDebugString(probe.pose.front()) << '\n';
        std::cout << AK::ToDebugString(probe.pose.back()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
