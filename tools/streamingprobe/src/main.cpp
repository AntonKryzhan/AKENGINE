#include <AK/Streaming/StreamingSystem.hpp>

#include <iostream>

int main()
{
    const AK::StreamingProbeResult probe = AK::BuildStreamingProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.firstUpdate) << '\n';
    std::cout << AK::ToDebugString(probe.secondUpdate) << '\n';
    return probe.ok ? 0 : 1;
}
