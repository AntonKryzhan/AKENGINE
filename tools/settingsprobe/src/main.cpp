#include <AK/Settings/ProjectSettings.hpp>

#include <iostream>

int main()
{
    const AK::SettingsProbeResult probe = AK::BuildSettingsProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.defaults.units) << '\n';
    std::cout << AK::ToDebugString(probe.defaults.coordinates) << '\n';
    std::cout << AK::ToDebugString(probe.defaults.runtime) << '\n';
    return probe.ok ? 0 : 1;
}
