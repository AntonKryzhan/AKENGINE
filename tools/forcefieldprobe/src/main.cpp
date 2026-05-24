#include <AK/ForceField/ForceField.hpp>

#include <iostream>

int main()
{
    const AK::ForceFieldProbeResult probe = AK::BuildForceFieldProbe();
    std::cout << probe.summary << '\n';
    for (const AK::ForceFieldDesc& field : probe.fields)
    {
        std::cout << AK::ToDebugString(field) << '\n';
    }
    for (const AK::ForceFieldBodySample& sample : probe.samples)
    {
        std::cout << AK::ToDebugString(sample) << '\n';
    }
    std::cout << AK::ToDebugString(probe.stats) << '\n';
    std::cout << AK::ToDebugString(probe.damageStats) << '\n';
    std::cout << AK::ToDebugString(probe.physicsStats) << '\n';
    return probe.ok ? 0 : 1;
}
