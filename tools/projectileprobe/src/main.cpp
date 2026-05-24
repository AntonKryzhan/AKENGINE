#include <AK/Projectile/Projectile.hpp>

#include <iostream>

int main()
{
    const AK::ProjectileProbeResult probe = AK::BuildProjectileProbe();
    std::cout << AK::ToDebugString(probe) << '\n';
    return probe.ok ? 0 : 1;
}
