#include <AK/Character/CharacterController.hpp>

#include <iostream>

int main()
{
    const AK::CharacterProbeResult probe = AK::BuildCharacterProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.config) << '\n';
    std::cout << AK::ToDebugString(probe.result.ground) << '\n';
    std::cout << AK::ToDebugString(probe.result) << '\n';
    std::cout << AK::ToDebugString(probe.stats) << '\n';
    return probe.ok ? 0 : 1;
}
