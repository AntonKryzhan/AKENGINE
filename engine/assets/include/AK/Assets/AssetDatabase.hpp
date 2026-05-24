#pragma once

#include <AK/Core/Guid.hpp>
#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>
#include <AK/Filesystem/FileSystem.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace AK
{
    enum class AssetKind
    {
        Unknown,
        Scene,
        Mesh,
        Texture,
        Material,
        Audio,
        Shader,
        Script,
        Font
    };

    enum class AssetCookStatus
    {
        SourceOnly,
        Cookable,
        Cooked,
        MissingSource,
        Unsupported
    };

    struct AssetImportPolicy final
    {
        bool includeUnknown = true;
        bool hashSourceContents = false;
        usize maxAssets = 8192;
        u64 maxSourceBytes = 512ull * 1024ull * 1024ull;
    };

    struct AssetDependency final
    {
        AssetGuid guid{};
        std::string label;
    };

    struct AssetManifestRecord final
    {
        AssetGuid guid{};
        AssetKind kind = AssetKind::Unknown;
        AssetCookStatus cookStatus = AssetCookStatus::Unsupported;
        std::filesystem::path sourcePath;
        std::filesystem::path cookedPath;
        std::string extension;
        std::string importerName;
        FileFingerprint sourceFingerprint{};
        std::vector<AssetDependency> dependencies;
        bool sourceInsideAssetRoot = false;
        bool pathRisk = false;
        bool duplicateGuid = false;
        std::string summary;
    };

    struct AssetManifestStats final
    {
        u64 totalAssets = 0;
        u64 cookableAssets = 0;
        u64 sourceOnlyAssets = 0;
        u64 unsupportedAssets = 0;
        u64 unknownAssets = 0;
        u64 duplicateGuidCount = 0;
        u64 pathRiskCount = 0;
        u64 totalSourceBytes = 0;
        bool truncated = false;
        std::string summary;
    };

    struct AssetManifest final
    {
        std::filesystem::path assetRoot;
        std::filesystem::path manifestPath;
        std::vector<AssetManifestRecord> records;
        AssetManifestStats stats{};
        std::string summary;

        [[nodiscard]] const AssetManifestRecord* Find(AssetGuid guid) const;
    };

    struct AssetDatabaseProbeResult final
    {
        bool ok = false;
        ProjectLayout layout{};
        AssetManifest manifest{};
        std::filesystem::path writtenManifestPath;
        std::string summary;
    };

    AssetKind DetectAssetKind(const std::filesystem::path& path);
    AssetCookStatus DetermineCookStatus(AssetKind kind, const FileFingerprint& fingerprint, const AssetImportPolicy& policy);
    std::string_view ToString(AssetKind kind);
    std::string_view ToString(AssetCookStatus status);
    std::string AssetImporterName(AssetKind kind);

    std::filesystem::path BuildCookedAssetPath(const ProjectLayout& layout, const AssetManifestRecord& record);
    AssetManifest BuildAssetManifest(const ProjectLayout& layout, const AssetImportPolicy& policy = {});
    Result<void> WriteAssetManifestText(const std::filesystem::path& path, const AssetManifest& manifest);

    std::string ToDebugString(const AssetManifestRecord& record);
    std::string ToDebugString(const AssetManifestStats& stats);
    std::string ToDebugString(const AssetManifest& manifest);

    AssetDatabaseProbeResult BuildAssetDatabaseProbe();
    std::string BuildAssetDatabaseProbeSummary();
}
