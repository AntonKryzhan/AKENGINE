#include <AK/Physics/Physics.hpp>

#include <iostream>

int main()
{
    const AK::PhysicsProbeResult probe = AK::BuildPhysicsProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.stats) << '\n';
    std::cout << AK::ToDebugString(probe.stats.broadphaseGrid) << '\n';
    std::cout << AK::ToDebugString(probe.raycast) << '\n';
    std::cout << AK::ToDebugString(probe.sweep) << '\n';

    if (!probe.contacts.empty())
    {
        std::cout << AK::ToDebugString(probe.contacts.front()) << '\n';
    }
    if (!probe.events.empty())
    {
        std::cout << AK::ToDebugString(probe.events.front()) << '\n';
    }

    return probe.ok ? 0 : 1;
}
