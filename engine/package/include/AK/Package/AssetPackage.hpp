#pragma once

#include <AK/Assets/AssetDatabase.hpp>
#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>
#include <AK/Filesystem/FileSystem.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class PackageEntryFlag : u32
    {
        None = 0,
        SourceBytes = 1u << 0u,
        CookedBytes = 1u << 1u,
        PathRisk = 1u << 2u,
        MissingSource = 1u << 3u,
        Unsupported = 1u << 4u
    };

    constexpr PackageEntryFlag operator|(PackageEntryFlag a, PackageEntryFlag b)
    {
        return static_cast<PackageEntryFlag>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    constexpr PackageEntryFlag operator&(PackageEntryFlag a, PackageEntryFlag b)
    {
        return static_cast<PackageEntryFlag>(static_cast<u32>(a) & static_cast<u32>(b));
    }

    struct PackageBuildOptions final
    {
        bool includeSourceBytes = true;
        bool deterministicOrder = true;
        bool hashPayload = true;
        usize maxEntries = 8192;
        u64 maxEntryBytes = 256ull * 1024ull * 1024ull;
    };

    struct PackageEntry final
    {
        AssetGuid guid{};
        AssetKind kind = AssetKind::Unknown;
        PackageEntryFlag flags = PackageEntryFlag::None;
        std::filesystem::path sourcePath;
        std::filesystem::path cookedPath;
        u64 payloadOffset = 0;
        u64 payloadSize = 0;
        u64 payloadHash = 0;
        std::string importerName;
        std::string summary;

        [[nodiscard]] bool HasFlag(PackageEntryFlag flag) const;
    };

    struct PackageBuildStats final
    {
        u64 entryCount = 0;
        u64 payloadBytes = 0;
        u64 tocBytes = 0;
        u64 packageBytes = 0;
        u64 sourceByteEntries = 0;
        u64 cookedByteEntries = 0;
        u64 skippedUnsupported = 0;
        u64 skippedMissing = 0;
        u64 skippedTooLarge = 0;
        u64 pathRiskEntries = 0;
        u64 duplicateGuidEntries = 0;
        bool deterministic = true;
        bool truncated = false;
        std::string summary;
    };

    struct PackageHeader final
    {
        bool valid = false;
        u32 version = 0;
        u64 entryCount = 0;
        u64 payloadBytes = 0;
        u64 tocBytes = 0;
        u64 packageBytes = 0;
        std::string summary;
    };

    struct PackageBuildResult final
    {
        bool ok = false;
        std::filesystem::path packagePath;
        std::vector<PackageEntry> entries;
        PackageBuildStats stats{};
        std::vector<std::string> warnings;
        std::string summary;
    };

    struct PackageProbeResult final
    {
        bool ok = false;
        ProjectLayout layout{};
        AssetManifest manifest{};
        PackageBuildResult package{};
        PackageHeader header{};
        std::string summary;
    };

    std::filesystem::path BuildDefaultPackagePath(const ProjectLayout& layout, std::string_view packageName = "sandbox");

    PackageBuildResult BuildAssetPackage(const ProjectLayout& layout, const AssetManifest& manifest, const PackageBuildOptions& options = {});
    Result<PackageBuildResult> WriteAssetPackage(const std::filesystem::path& path, const ProjectLayout& layout, const AssetManifest& manifest, const PackageBuildOptions& options = {});
    Result<PackageHeader> ReadAssetPackageHeader(const std::filesystem::path& path);

    std::string_view ToString(PackageEntryFlag flag);
    std::string ToDebugString(PackageEntryFlag flags);
    std::string ToDebugString(const PackageEntry& entry);
    std::string ToDebugString(const PackageBuildStats& stats);
    std::string ToDebugString(const PackageBuildResult& package);
    std::string ToDebugString(const PackageHeader& header);

    PackageProbeResult BuildPackageProbe();
    std::string BuildPackageProbeSummary();
}
