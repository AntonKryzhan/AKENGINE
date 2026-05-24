#include <AK/PointCloud/PointCloudCook.hpp>

#include <iostream>

int main()
{
    const AK::PointCloudCookProbeResult probe = AK::BuildPointCloudCookProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.report.source) << '\n';
    std::cout << AK::ToDebugString(probe.report.manifest.layout) << '\n';
    std::cout << AK::ToDebugString(probe.report.validation) << '\n';
    std::cout << AK::ToDebugString(probe.report.manifest) << '\n';
    if (!probe.report.manifest.pages.empty())
    {
        std::cout << AK::ToDebugString(probe.report.manifest.pages.front()) << '\n';
        std::cout << AK::ToDebugString(probe.report.manifest.pages.back()) << '\n';
    }
    std::cout << AK::ToDebugString(probe.report.streamingPlan) << '\n';
    std::cout << AK::ToDebugString(probe.report.outOfCore) << '\n';
    return probe.ok ? 0 : 1;
}
