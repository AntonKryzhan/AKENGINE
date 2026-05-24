#include <AK/Explosion/Explosion.hpp>

#include <iostream>

int main()
{
    const AK::ExplosionProbeResult probe = AK::BuildExplosionProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.explosionResult) << '\n';
    std::cout << AK::ToDebugString(probe.damageStats) << '\n';
    std::cout << AK::ToDebugString(probe.physicsStats) << '\n';
    if (!probe.explosionResult.hits.empty())
    {
        std::cout << AK::ToDebugString(probe.explosionResult.hits.front()) << '\n';
    }
    if (!probe.debris.particles.empty())
    {
        std::cout << "explosion_debris particles=" << probe.debris.particles.size() << '\n';
    }
    return probe.ok ? 0 : 1;
}
