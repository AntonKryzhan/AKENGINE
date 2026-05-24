#include <AK/Serialization/Schema.hpp>

#include <iostream>

int main()
{
    const AK::SerializationProbeResult probe = AK::BuildSerializationProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.header) << '\n';
    std::cout << AK::ToDebugString(probe.scenePlan) << '\n';
    std::cout << AK::ToDebugString(probe.keyValues) << '\n';
    std::cout << "quoted=" << probe.escaped << " unquoted=" << probe.unescaped << '\n';
    return probe.ok ? 0 : 1;
}
