#include <AK/Spline/Spline.hpp>

#include <iostream>

int main()
{
    const AK::SplineProbeResult probe = AK::BuildSplineProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.validation) << '\n';
    std::cout << AK::ToDebugString(probe.midFrame) << '\n';
    std::cout << AK::ToDebugString(probe.closest) << '\n';
    std::cout << "advanced=(" << probe.advancedPosition.x << ", " << probe.advancedPosition.y << ", " << probe.advancedPosition.z << ")\n";
    return probe.ok ? 0 : 1;
}
