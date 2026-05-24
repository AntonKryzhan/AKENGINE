#pragma once

#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class PathRiskFlag : u32
    {
        None = 0,
        NotNormalized = 1u << 0u,
        ContainsParentTraversal = 1u << 1u,
        LongPathRisk = 1u << 2u,
        ContainsNonAscii = 1u << 3u,
        EmptyPath = 1u << 4u
    };

    constexpr PathRiskFlag operator|(PathRiskFlag a, PathRiskFlag b)
    {
        return static_cast<PathRiskFlag>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    constexpr PathRiskFlag operator&(PathRiskFlag a, PathRiskFlag b)
    {
        return static_cast<PathRiskFlag>(static_cast<u32>(a) & static_cast<u32>(b));
    }

    struct PathRiskReport final
    {
        PathRiskFlag flags = PathRiskFlag::None;
        std::string normalized;
        std::string summary;

        [[nodiscard]] bool HasRisk(PathRiskFlag flag) const;
        [[nodiscard]] bool HasAnyRisk() const;
    };

    struct ProjectLayout final
    {
        std::filesystem::path root;
        std::filesystem::path assets;
        std::filesystem::path projects;
        std::filesystem::path cache;
        std::filesystem::path logs;
        std::filesystem::path temp;
        std::filesystem::path intermediate;
        std::filesystem::path shaderCache;
        bool valid = false;
        std::vector<std::string> warnings;
        std::string summary;
    };

    struct FileEntry final
    {
        std::filesystem::path path;
        std::filesystem::path relativePath;
        bool directory = false;
        bool regularFile = false;
        u64 sizeBytes = 0;
        i64 writeTimeTicks = 0;
    };

    struct FileFingerprint final
    {
        std::filesystem::path path;
        bool exists = false;
        bool regularFile = false;
        u64 sizeBytes = 0;
        i64 writeTimeTicks = 0;
        u64 contentHash = 0;
        std::string summary;
    };

    struct DirectoryScanOptions final
    {
        bool recursive = true;
        bool includeFiles = true;
        bool includeDirectories = false;
        usize maxEntries = 4096;
    };

    struct DirectoryScanResult final
    {
        std::filesystem::path root;
        std::vector<FileEntry> entries;
        u64 fileCount = 0;
        u64 directoryCount = 0;
        u64 totalBytes = 0;
        u64 skippedEntries = 0;
        bool truncated = false;
        std::string summary;
    };

    struct FilesystemProbeResult final
    {
        bool ok = false;
        ProjectLayout layout{};
        PathRiskReport pathRisk{};
        DirectoryScanResult scan{};
        FileFingerprint fingerprint{};
        std::string summary;
    };

    std::filesystem::path NormalizeProjectRoot(const std::filesystem::path& root);
    bool IsPathInsideRoot(const std::filesystem::path& path, const std::filesystem::path& root);
    PathRiskReport AssessPathRisk(const std::filesystem::path& path);

    ProjectLayout BuildProjectLayout(const std::filesystem::path& root);
    Result<ProjectLayout> EnsureProjectLayout(const std::filesystem::path& root);

    Result<std::string> ReadTextFile(const std::filesystem::path& path, u64 maxBytes = 64ull * 1024ull * 1024ull);
    Result<std::vector<u8>> ReadBinaryFile(const std::filesystem::path& path, u64 maxBytes = 256ull * 1024ull * 1024ull);
    Result<void> WriteTextFileAtomic(const std::filesystem::path& path, std::string_view text);
    Result<void> WriteBinaryFileAtomic(const std::filesystem::path& path, const std::vector<u8>& bytes);

    FileFingerprint BuildFileFingerprint(const std::filesystem::path& path, bool hashContents);
    DirectoryScanResult ScanDirectoryTree(const std::filesystem::path& root, const DirectoryScanOptions& options = {});

    std::string ToDebugString(PathRiskFlag flags);
    std::string ToDebugString(const PathRiskReport& report);
    std::string ToDebugString(const ProjectLayout& layout);
    std::string ToDebugString(const FileFingerprint& fingerprint);
    std::string ToDebugString(const DirectoryScanResult& scan);

    FilesystemProbeResult BuildFilesystemProbe();
    std::string BuildFilesystemProbeSummary();
}
