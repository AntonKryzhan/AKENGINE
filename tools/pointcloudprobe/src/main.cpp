#include <AK/PointCloud/PointCloud.hpp>

#include <iostream>

int main()
{
    const AK::PointCloudProbeResult probe = AK::BuildPointCloudProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.asset) << '\n';
    std::cout << AK::ToDebugString(probe.validation) << '\n';
    std::cout << AK::ToDebugString(probe.streamingPlan) << '\n';
    std::cout << AK::ToDebugString(probe.outOfCore) << '\n';
    if (!probe.chunks.empty())
    {
        std::cout << AK::ToDebugString(probe.chunks.front()) << '\n';
    }
    if (!probe.streamingPlan.chunks.empty())
    {
        std::cout << AK::ToDebugString(probe.streamingPlan.chunks.front()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
