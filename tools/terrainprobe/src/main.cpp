#include <AK/Terrain/Terrain.hpp>

#include <iostream>

int main()
{
    const AK::TerrainProbeResult probe = AK::BuildTerrainProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.validation) << '\n';
    std::cout << AK::ToDebugString(probe.heightfieldSample) << '\n';
    std::cout << AK::ToDebugString(probe.planetSample) << '\n';
    std::cout << AK::ToDebugString(probe.toroidalSample) << '\n';
    std::cout << AK::ToDebugString(probe.heightfieldPatch) << '\n';
    std::cout << AK::ToDebugString(probe.planetPatch) << '\n';
    return probe.ok ? 0 : 1;
}
