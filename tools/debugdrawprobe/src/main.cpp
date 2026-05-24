#include <AK/DebugDraw/DebugDraw.hpp>

#include <iostream>

int main()
{
    const AK::DebugDrawProbeResult probe = AK::BuildDebugDrawProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.list.stats) << '\n';
    for (std::size_t i = 0; i < probe.list.commands.size() && i < 16; ++i)
    {
        std::cout << AK::ToDebugString(probe.list.commands[i]) << '\n';
    }
    if (probe.list.commands.size() > 16)
    {
        std::cout << "debug_draw omitted=" << (probe.list.commands.size() - 16) << '\n';
    }
    return probe.ok ? 0 : 1;
}
