#include <AK/PointCloud/PointCloudRuntime.hpp>

#include <iostream>

int main()
{
    const AK::PointCloudRuntimeProbeResult probe = AK::BuildPointCloudRuntimeProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.plan.stats) << '\n';
    std::cout << AK::ToDebugString(probe.plan.outOfCore) << '\n';
    if (!probe.plan.pages.empty())
    {
        std::cout << AK::ToDebugString(probe.plan.pages.front()) << '\n';
        std::cout << AK::ToDebugString(probe.plan.pages.back()) << '\n';
    }
    if (!probe.plan.commands.empty())
    {
        std::cout << AK::ToDebugString(probe.plan.commands.front()) << '\n';
        std::cout << AK::ToDebugString(probe.plan.commands.back()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
