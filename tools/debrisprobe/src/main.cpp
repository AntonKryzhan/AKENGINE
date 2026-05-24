#include <AK/Debris/Debris.hpp>

#include <iostream>

int main()
{
    const AK::DebrisProbeResult probe = AK::BuildDebrisProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.spawn) << '\n';
    std::cout << AK::ToDebugString(probe.step) << '\n';
    std::cout << AK::ToDebugString(probe.proxy) << '\n';
    if (!probe.system.particles.empty())
    {
        std::cout << AK::ToDebugString(probe.system.particles.front()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
