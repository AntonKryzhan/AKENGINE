#include <AK/Diagnostics/Diagnostics.hpp>

#include <iostream>

int main()
{
    const AK::DiagnosticsProbeResult probe = AK::BuildDiagnosticsProbe();
    std::cout << probe.summary << '\n';

    for (const AK::ProfileZoneStats& zone : probe.snapshot.zones)
    {
        std::cout << "  zone " << AK::ToDebugString(zone) << '\n';
    }

    for (const AK::DiagnosticCounter& counter : probe.snapshot.counters)
    {
        std::cout << "  counter " << counter.name << '=' << counter.value << '\n';
    }

    for (const AK::DiagnosticEvent& event : probe.snapshot.recentEvents)
    {
        std::cout << "  event #" << event.sequence << ' ' << AK::ToString(event.severity) << ' ' << event.category << ": " << event.message << '\n';
    }

    return probe.ok ? 0 : 1;
}
