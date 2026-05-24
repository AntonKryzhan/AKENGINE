#include <AK/Memory/Memory.hpp>

#include <cstdlib>
#include <iostream>

int main()
{
    const AK::MemoryProbeResult probe = AK::BuildMemoryProbe();
    std::cout << probe.summary << '\n';
    std::cout << "tracking: " << AK::ToDebugString(probe.trackingStats) << '\n';
    std::cout << "linear:   " << AK::ToDebugString(probe.linearStats) << '\n';
    std::cout << "policy: hot frame code uses arenas; resources use handles/deferred release; diagnostics use tracking allocator" << '\n';
    return probe.ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
