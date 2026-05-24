#include <AK/Gravity/GravityField.hpp>

#include <iostream>

int main()
{
    const AK::GravityProbeResult probe = AK::BuildGravityProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.uniform) << '\n';
    std::cout << AK::ToDebugString(probe.planet) << '\n';
    std::cout << AK::ToDebugString(probe.point) << '\n';
    std::cout << AK::ToDebugString(probe.planetMove) << '\n';
    std::cout << AK::ToDebugString(probe.toroidalMove) << '\n';
    return probe.ok ? 0 : 1;
}
