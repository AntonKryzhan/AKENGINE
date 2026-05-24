#include <AK/Package/AssetPackage.hpp>

#include <iostream>

int main()
{
    const AK::PackageProbeResult result = AK::BuildPackageProbe();
    std::cout << result.summary << '\n';
    std::cout << AK::ToDebugString(result.package.stats) << '\n';
    std::cout << AK::ToDebugString(result.header) << '\n';
    for (const AK::PackageEntry& entry : result.package.entries)
    {
        std::cout << AK::ToDebugString(entry) << '\n';
    }
    return result.ok ? 0 : 1;
}
