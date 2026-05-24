#pragma once

#include <AK/Assets/AssetDatabase.hpp>
#include <AK/Core/Guid.hpp>
#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>
#include <AK/Filesystem/FileSystem.hpp>
#include <AK/Package/AssetPackage.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AK
{
    enum class VirtualMountKind
    {
        LooseDirectory,
        AssetPackage
    };

    enum class VirtualEntryFlag : u32
    {
        None = 0,
        LooseFile = 1u << 0u,
        PackagePayload = 1u << 1u,
        PathRisk = 1u << 2u,
        DuplicateGuid = 1u << 3u,
        HashMismatch = 1u << 4u,
        MissingPayload = 1u << 5u
    };

    constexpr VirtualEntryFlag operator|(VirtualEntryFlag a, VirtualEntryFlag b)
    {
        return static_cast<VirtualEntryFlag>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    constexpr VirtualEntryFlag operator&(VirtualEntryFlag a, VirtualEntryFlag b)
    {
        return static_cast<VirtualEntryFlag>(static_cast<u32>(a) & static_cast<u32>(b));
    }

    struct VirtualMount final
    {
        std::string name;
        VirtualMountKind kind = VirtualMountKind::LooseDirectory;
        std::filesystem::path root;
        u64 tocBytes = 0;
        u64 packageBytes = 0;
        std::string summary;
    };

    struct VirtualFileEntry final
    {
        AssetGuid guid{};
        AssetKind kind = AssetKind::Unknown;
        VirtualEntryFlag flags = VirtualEntryFlag::None;
        std::string mountName;
        std::filesystem::path logicalPath;
        std::filesystem::path physicalPath;
        u64 payloadOffset = 0;
        u64 payloadSize = 0;
        u64 payloadHash = 0;
        std::string importerName;
        std::string summary;

        [[nodiscard]] bool HasFlag(VirtualEntryFlag flag) const;
    };

    struct VirtualFileSystemStats final
    {
        u64 mountCount = 0;
        u64 entryCount = 0;
        u64 looseEntries = 0;
        u64 packageEntries = 0;
        u64 duplicateGuidEntries = 0;
        u64 pathRiskEntries = 0;
        u64 missingPayloadEntries = 0;
        u64 payloadBytes = 0;
        std::string summary;
    };

    struct VirtualReadOptions final
    {
        bool validateHash = true;
        u64 maxBytes = 256ull * 1024ull * 1024ull;
    };

    struct VirtualReadResult final
    {
        bool ok = false;
        AssetGuid guid{};
        VirtualFileEntry entry{};
        std::vector<u8> bytes;
        bool hashValid = true;
        std::string summary;
    };

    struct VirtualFileSystemProbeResult final
    {
        bool ok = false;
        ProjectLayout layout{};
        AssetManifest manifest{};
        PackageBuildResult package{};
        VirtualFileSystemStats stats{};
        VirtualReadResult read{};
        std::string summary;
    };

    class VirtualFileSystem final
    {
    public:
        Result<void> MountLooseDirectory(std::string_view name, const ProjectLayout& layout, const AssetImportPolicy& policy = {});
        Result<void> MountPackage(std::string_view name, const std::filesystem::path& packagePath);

        [[nodiscard]] const VirtualFileEntry* Find(AssetGuid guid) const;
        [[nodiscard]] const VirtualFileEntry* FindByLogicalPath(const std::filesystem::path& logicalPath) const;
        [[nodiscard]] Result<VirtualReadResult> Read(AssetGuid guid, const VirtualReadOptions& options = {}) const;
        [[nodiscard]] Result<VirtualReadResult> ReadByLogicalPath(const std::filesystem::path& logicalPath, const VirtualReadOptions& options = {}) const;
        [[nodiscard]] const std::vector<VirtualMount>& Mounts() const;
        [[nodiscard]] const std::vector<VirtualFileEntry>& Entries() const;
        [[nodiscard]] VirtualFileSystemStats Stats() const;
        void Clear();

    private:
        void AddEntry(VirtualFileEntry entry);
        [[nodiscard]] const VirtualMount* FindMount(std::string_view name) const;

        std::vector<VirtualMount> mMounts;
        std::vector<VirtualFileEntry> mEntries;
        std::unordered_map<AssetGuid, usize> mGuidToEntry;
    };

    std::string_view ToString(VirtualMountKind kind);
    std::string_view ToString(VirtualEntryFlag flag);
    std::string ToDebugString(VirtualEntryFlag flags);
    std::string ToDebugString(const VirtualMount& mount);
    std::string ToDebugString(const VirtualFileEntry& entry);
    std::string ToDebugString(const VirtualFileSystemStats& stats);
    std::string ToDebugString(const VirtualReadResult& read);

    VirtualFileSystemProbeResult BuildVirtualFileSystemProbe();
    std::string BuildVirtualFileSystemProbeSummary();
}
