#include <AK/WorldPartition/WorldPartition.hpp>

#include <iostream>

int main()
{
    const AK::WorldPartitionProbeResult probe = AK::BuildWorldPartitionProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.firstUpdate) << '\n';
    std::cout << AK::ToDebugString(probe.farUpdate) << '\n';
    return probe.ok ? 0 : 1;
}
