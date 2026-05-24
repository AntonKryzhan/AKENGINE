#include <AK/Fluid/Fluid.hpp>

#include <iostream>

int main()
{
    const AK::FluidProbeResult probe = AK::BuildFluidProbe();
    std::cout << probe.summary << '\n';
    for (const AK::FluidVolume& volume : probe.volumes)
    {
        std::cout << AK::ToDebugString(volume) << '\n';
    }
    std::cout << AK::ToDebugString(probe.surfaceSample) << '\n';
    for (const AK::FluidColliderInteraction& interaction : probe.interactions)
    {
        std::cout << AK::ToDebugString(interaction) << '\n';
    }
    std::cout << AK::ToDebugString(probe.fluidStats) << '\n';
    std::cout << AK::ToDebugString(probe.physicsStats) << '\n';
    return probe.ok ? 0 : 1;
}
