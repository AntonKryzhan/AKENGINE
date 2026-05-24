#include <AK/Vehicle/Vehicle.hpp>

#include <iostream>

int main()
{
    const AK::VehicleProbeResult probe = AK::BuildVehicleProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.config) << '\n';
    std::cout << AK::ToDebugString(probe.result) << '\n';
    if (!probe.result.wheels.empty())
    {
        std::cout << AK::ToDebugString(probe.result.wheels.front()) << '\n';
    }
    std::cout << AK::ToDebugString(probe.stats) << '\n';
    return probe.ok ? 0 : 1;
}
