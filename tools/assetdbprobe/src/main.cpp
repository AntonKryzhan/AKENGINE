#include <AK/Assets/AssetDatabase.hpp>

#include <iostream>

int main()
{
    const AK::AssetDatabaseProbeResult result = AK::BuildAssetDatabaseProbe();
    std::cout << result.summary << '\n';
    std::cout << AK::ToDebugString(result.manifest.stats) << '\n';
    for (const AK::AssetManifestRecord& record : result.manifest.records)
    {
        std::cout << AK::ToDebugString(record) << '\n';
    }
    return result.ok ? 0 : 1;
}
