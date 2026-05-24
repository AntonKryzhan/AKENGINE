#include <AK/Damage/Damage.hpp>

#include <iostream>

int main()
{
    const AK::DamageProbeResult probe = AK::BuildDamageProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.stats) << '\n';
    if (!probe.system.queue.empty())
    {
        std::cout << AK::ToDebugString(probe.system.queue.front()) << '\n';
    }
    for (const AK::DamageApplicationResult& result : probe.results)
    {
        std::cout << AK::ToDebugString(result) << '\n';
    }
    if (!probe.debris.particles.empty())
    {
        std::cout << "damage_debris particles=" << probe.debris.particles.size() << '\n';
    }
    return probe.ok ? 0 : 1;
}
