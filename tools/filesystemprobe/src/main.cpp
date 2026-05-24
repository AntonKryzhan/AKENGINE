#include <AK/Filesystem/FileSystem.hpp>

#include <iostream>

int main()
{
    const AK::FilesystemProbeResult probe = AK::BuildFilesystemProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::ToDebugString(probe.pathRisk) << '\n';
    std::cout << AK::ToDebugString(probe.fingerprint) << '\n';
    return probe.ok ? 0 : 1;
}
