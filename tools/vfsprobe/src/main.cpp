#include <AK/VFS/VirtualFileSystem.hpp>

#include <iostream>
#include <vector>

int main()
{
    const AK::VirtualFileSystemProbeResult result = AK::BuildVirtualFileSystemProbe();
    std::cout << result.summary << '\n';
    std::cout << AK::ToDebugString(result.stats) << '\n';
    std::cout << AK::ToDebugString(result.read) << '\n';
    for (const AK::VirtualFileEntry& entry : result.read.ok ? std::vector<AK::VirtualFileEntry>{result.read.entry} : std::vector<AK::VirtualFileEntry>{})
    {
        std::cout << AK::ToDebugString(entry) << '\n';
    }
    return result.ok ? 0 : 1;
}
