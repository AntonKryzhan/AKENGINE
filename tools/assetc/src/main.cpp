#include <AK/Assets/AssetDatabase.hpp>
#include <AK/Assets/AssetRegistry.hpp>
#include <AK/Core/Log.hpp>
#include <AK/Filesystem/FileSystem.hpp>
#include <AK/Package/AssetPackage.hpp>

#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    const std::filesystem::path projectRoot = argc > 1 ? argv[1] : ".";

    const AK::Result<AK::ProjectLayout> layoutResult = AK::EnsureProjectLayout(projectRoot);
    if (!layoutResult)
    {
        AK::LogError(layoutResult.GetError().message);
        return 1;
    }

    const AK::ProjectLayout& layout = layoutResult.Value();

    AK::AssetRegistry registry;
    if (!registry.ScanDirectory(layout.assets))
    {
        AK::LogWarning("No assets were indexed");
    }

    AK::AssetImportPolicy policy{};
    policy.hashSourceContents = true;
    const AK::AssetManifest manifest = AK::BuildAssetManifest(layout, policy);
    const AK::Result<void> writeManifest = AK::WriteAssetManifestText(manifest.manifestPath, manifest);
    if (!writeManifest)
    {
        AK::LogError(writeManifest.GetError().message);
        return 1;
    }

    AK::PackageBuildOptions packageOptions{};
    packageOptions.hashPayload = true;
    const std::filesystem::path packagePath = AK::BuildDefaultPackagePath(layout, "sandbox");
    const AK::Result<AK::PackageBuildResult> packageResult = AK::WriteAssetPackage(packagePath, layout, manifest, packageOptions);
    if (!packageResult)
    {
        AK::LogError(packageResult.GetError().message);
        return 1;
    }

    std::cout << "AK Asset Compiler\n";
    std::cout << "Project: " << layout.root.string() << "\n";
    std::cout << "Asset root: " << layout.assets.string() << "\n";
    std::cout << "Registry assets: " << registry.Assets().size() << "\n";
    std::cout << "Manifest: " << manifest.manifestPath.string() << "\n";
    std::cout << "Package: " << packageResult.Value().packagePath.string() << "\n";
    std::cout << AK::ToDebugString(manifest.stats) << "\n";
    std::cout << AK::ToDebugString(packageResult.Value().stats) << "\n";

    for (const AK::AssetManifestRecord& asset : manifest.records)
    {
        std::cout << AK::ToDebugString(asset) << "\n";
    }

    for (const AK::PackageEntry& entry : packageResult.Value().entries)
    {
        std::cout << "pack " << AK::ToDebugString(entry) << "\n";
    }

    return packageResult.Value().ok ? 0 : 2;
}
