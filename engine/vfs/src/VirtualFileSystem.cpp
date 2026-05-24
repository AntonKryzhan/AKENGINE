#include <AK/VFS/VirtualFileSystem.hpp>

#include <AK/Core/Path.hpp>

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace AK
{
    namespace
    {
        constexpr std::string_view PackageMagic = "AKPAK 1";

        bool HasFlag(VirtualEntryFlag flags, VirtualEntryFlag flag)
        {
            return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0u;
        }

        u64 Fnv1a64Bytes(const u8* data, usize size, u64 seed = 14695981039346656037ull)
        {
            u64 hash = seed;
            for (usize i = 0; i < size; ++i)
            {
                hash ^= static_cast<u64>(data[i]);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        std::vector<std::string_view> SplitLines(std::string_view text)
        {
            std::vector<std::string_view> lines;
            usize start = 0;
            for (usize i = 0; i <= text.size(); ++i)
            {
                if (i == text.size() || text[i] == '\n')
                {
                    std::string_view line = text.substr(start, i - start);
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }
                    lines.push_back(line);
                    start = i + 1;
                }
            }
            return lines;
        }

        std::string UnescapeQuoted(std::string_view text)
        {
            std::string out;
            out.reserve(text.size());
            bool escaped = false;
            for (const char c : text)
            {
                if (escaped)
                {
                    if (c == 'n')
                    {
                        out.push_back('\n');
                    }
                    else if (c == 'r')
                    {
                        out.push_back('\r');
                    }
                    else
                    {
                        out.push_back(c);
                    }
                    escaped = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaped = true;
                }
                else
                {
                    out.push_back(c);
                }
            }
            return out;
        }

        std::string GetQuotedValue(std::string_view line, std::string_view key)
        {
            const usize keyPos = line.find(key);
            if (keyPos == std::string_view::npos)
            {
                return {};
            }
            const usize firstQuote = line.find('"', keyPos + key.size());
            if (firstQuote == std::string_view::npos)
            {
                return {};
            }

            bool escaped = false;
            for (usize i = firstQuote + 1; i < line.size(); ++i)
            {
                const char c = line[i];
                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (c == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (c == '"')
                {
                    return UnescapeQuoted(line.substr(firstQuote + 1, i - firstQuote - 1));
                }
            }
            return {};
        }

        std::string_view GetTokenValue(std::string_view line, std::string_view key)
        {
            const usize keyPos = line.find(key);
            if (keyPos == std::string_view::npos)
            {
                return {};
            }
            const usize begin = keyPos + key.size();
            usize end = begin;
            while (end < line.size() && line[end] != ' ')
            {
                ++end;
            }
            return line.substr(begin, end - begin);
        }

        bool ParseU64(std::string_view text, u64& out, int base = 10)
        {
            if (text.empty())
            {
                return false;
            }
            const char* begin = text.data();
            const char* end = begin + text.size();
            const auto result = std::from_chars(begin, end, out, base);
            return result.ec == std::errc{} && result.ptr == end;
        }

        bool ParseHexU64(std::string_view text, u64& out)
        {
            if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0)
            {
                text.remove_prefix(2);
            }
            return ParseU64(text, out, 16);
        }

        AssetKind ParseAssetKind(std::string_view text)
        {
            if (text == "scene") return AssetKind::Scene;
            if (text == "mesh") return AssetKind::Mesh;
            if (text == "texture") return AssetKind::Texture;
            if (text == "material") return AssetKind::Material;
            if (text == "audio") return AssetKind::Audio;
            if (text == "shader") return AssetKind::Shader;
            if (text == "script") return AssetKind::Script;
            if (text == "font") return AssetKind::Font;
            return AssetKind::Unknown;
        }

        std::string MountSummary(std::string_view name, VirtualMountKind kind, const std::filesystem::path& root, u64 entries)
        {
            std::ostringstream out;
            out << name << " " << ToString(kind) << " " << root.generic_string() << " entries=" << entries;
            return out.str();
        }
    }

    bool VirtualFileEntry::HasFlag(VirtualEntryFlag flag) const
    {
        return AK::HasFlag(flags, flag);
    }

    Result<void> VirtualFileSystem::MountLooseDirectory(std::string_view name, const ProjectLayout& layout, const AssetImportPolicy& policy)
    {
        if (name.empty())
        {
            return MakeError(ErrorCode::InvalidArgument, "VFS loose mount name is empty");
        }

        const AssetManifest manifest = BuildAssetManifest(layout, policy);
        VirtualMount mount{};
        mount.name = std::string(name);
        mount.kind = VirtualMountKind::LooseDirectory;
        mount.root = NormalizePath(layout.assets);
        mount.summary = MountSummary(mount.name, mount.kind, mount.root, manifest.records.size());
        mMounts.push_back(std::move(mount));

        for (const AssetManifestRecord& record : manifest.records)
        {
            if (record.cookStatus == AssetCookStatus::MissingSource || record.cookStatus == AssetCookStatus::Unsupported)
            {
                continue;
            }

            VirtualFileEntry entry{};
            entry.guid = record.guid;
            entry.kind = record.kind;
            entry.flags = VirtualEntryFlag::LooseFile;
            if (record.pathRisk)
            {
                entry.flags = entry.flags | VirtualEntryFlag::PathRisk;
            }
            entry.mountName = std::string(name);
            entry.logicalPath = NormalizePath(record.sourcePath);
            entry.physicalPath = NormalizePath(layout.assets / record.sourcePath);
            entry.payloadSize = record.sourceFingerprint.sizeBytes;
            entry.payloadHash = record.sourceFingerprint.contentHash;
            entry.importerName = record.importerName;
            entry.summary = ToDebugString(entry);
            AddEntry(std::move(entry));
        }

        return Ok();
    }

    Result<void> VirtualFileSystem::MountPackage(std::string_view name, const std::filesystem::path& packagePath)
    {
        if (name.empty())
        {
            return MakeError(ErrorCode::InvalidArgument, "VFS package mount name is empty");
        }

        const std::filesystem::path normalizedPackagePath = NormalizePath(packagePath);
        Result<std::vector<u8>> bytes = ReadBinaryFile(normalizedPackagePath, 1024ull * 1024ull * 1024ull);
        if (!bytes)
        {
            return bytes.GetError();
        }

        const std::string_view text(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size());
        const usize endToc = text.find("ENDTOC\n");
        if (endToc == std::string_view::npos)
        {
            return MakeError(ErrorCode::ParseError, "VFS package TOC terminator not found: " + normalizedPackagePath.string());
        }

        const std::string_view toc = text.substr(0, endToc + 7);
        const std::vector<std::string_view> lines = SplitLines(toc);
        if (lines.empty() || lines[0] != PackageMagic)
        {
            return MakeError(ErrorCode::UnsupportedVersion, "VFS unsupported package magic: " + normalizedPackagePath.string());
        }

        VirtualMount mount{};
        mount.name = std::string(name);
        mount.kind = VirtualMountKind::AssetPackage;
        mount.root = normalizedPackagePath;
        mount.tocBytes = static_cast<u64>(endToc + 7);
        mount.packageBytes = static_cast<u64>(bytes.Value().size());

        u64 parsedEntries = 0;
        for (const std::string_view line : lines)
        {
            if (line.rfind("entry ", 0) != 0)
            {
                continue;
            }

            std::string_view rest = line.substr(6);
            const usize firstSpace = rest.find(' ');
            if (firstSpace == std::string_view::npos)
            {
                continue;
            }

            const AssetGuid guid = AssetGuidFromString(rest.substr(0, firstSpace));
            if (!guid.IsValid())
            {
                continue;
            }

            u64 offset = 0;
            u64 size = 0;
            u64 hash = 0;
            const bool hasOffset = ParseU64(GetTokenValue(line, " offset="), offset);
            const bool hasSize = ParseU64(GetTokenValue(line, " size="), size);
            const bool hasHash = ParseHexU64(GetTokenValue(line, " hash="), hash);
            if (!hasOffset || !hasSize)
            {
                continue;
            }

            VirtualFileEntry entry{};
            entry.guid = guid;
            entry.kind = ParseAssetKind(GetTokenValue(line, " kind="));
            entry.flags = VirtualEntryFlag::PackagePayload;
            if (mount.tocBytes + offset + size > bytes.Value().size())
            {
                entry.flags = entry.flags | VirtualEntryFlag::MissingPayload;
            }
            entry.mountName = std::string(name);
            entry.logicalPath = NormalizePath(GetQuotedValue(line, " source="));
            entry.physicalPath = normalizedPackagePath;
            entry.payloadOffset = offset;
            entry.payloadSize = size;
            entry.payloadHash = hasHash ? hash : 0;
            entry.importerName = std::string(GetTokenValue(line, " importer="));
            entry.summary = ToDebugString(entry);
            AddEntry(std::move(entry));
            ++parsedEntries;
        }

        mount.summary = MountSummary(mount.name, mount.kind, mount.root, parsedEntries);
        mMounts.push_back(std::move(mount));
        return Ok();
    }

    const VirtualFileEntry* VirtualFileSystem::Find(AssetGuid guid) const
    {
        const auto it = mGuidToEntry.find(guid);
        if (it == mGuidToEntry.end() || it->second >= mEntries.size())
        {
            return nullptr;
        }
        return &mEntries[it->second];
    }

    const VirtualFileEntry* VirtualFileSystem::FindByLogicalPath(const std::filesystem::path& logicalPath) const
    {
        const std::filesystem::path normalized = NormalizePath(logicalPath);
        const auto it = std::find_if(mEntries.begin(), mEntries.end(), [&normalized](const VirtualFileEntry& entry)
        {
            return NormalizePath(entry.logicalPath) == normalized;
        });
        return it == mEntries.end() ? nullptr : &(*it);
    }

    Result<VirtualReadResult> VirtualFileSystem::Read(AssetGuid guid, const VirtualReadOptions& options) const
    {
        const VirtualFileEntry* entry = Find(guid);
        if (!entry)
        {
            return MakeError(ErrorCode::NotFound, "VFS asset not found: " + ToString(guid));
        }

        VirtualReadResult result{};
        result.guid = guid;
        result.entry = *entry;

        if (entry->payloadSize > options.maxBytes)
        {
            return MakeError(ErrorCode::InvalidArgument, "VFS asset exceeds read limit: " + entry->logicalPath.string());
        }

        if (entry->HasFlag(VirtualEntryFlag::LooseFile))
        {
            Result<std::vector<u8>> bytes = ReadBinaryFile(entry->physicalPath, options.maxBytes);
            if (!bytes)
            {
                return bytes.GetError();
            }
            result.bytes = std::move(bytes.Value());
        }
        else if (entry->HasFlag(VirtualEntryFlag::PackagePayload))
        {
            const VirtualMount* mount = FindMount(entry->mountName);
            if (!mount || mount->kind != VirtualMountKind::AssetPackage)
            {
                return MakeError(ErrorCode::InvalidState, "VFS package mount missing for asset: " + ToString(guid));
            }

            Result<std::vector<u8>> packageBytes = ReadBinaryFile(entry->physicalPath, 1024ull * 1024ull * 1024ull);
            if (!packageBytes)
            {
                return packageBytes.GetError();
            }

            const u64 begin = mount->tocBytes + entry->payloadOffset;
            const u64 end = begin + entry->payloadSize;
            if (end > packageBytes.Value().size())
            {
                return MakeError(ErrorCode::ParseError, "VFS package payload outside file: " + entry->logicalPath.string());
            }

            result.bytes.assign(packageBytes.Value().begin() + static_cast<std::ptrdiff_t>(begin), packageBytes.Value().begin() + static_cast<std::ptrdiff_t>(end));
        }
        else
        {
            return MakeError(ErrorCode::InvalidState, "VFS entry has no readable source: " + entry->logicalPath.string());
        }

        if (options.validateHash && entry->payloadHash != 0)
        {
            const u64 actualHash = Fnv1a64Bytes(result.bytes.data(), result.bytes.size());
            result.hashValid = actualHash == entry->payloadHash;
            if (!result.hashValid)
            {
                result.entry.flags = result.entry.flags | VirtualEntryFlag::HashMismatch;
            }
        }

        result.ok = result.hashValid && result.bytes.size() == entry->payloadSize;
        result.summary = ToDebugString(result);
        return Ok(std::move(result));
    }

    Result<VirtualReadResult> VirtualFileSystem::ReadByLogicalPath(const std::filesystem::path& logicalPath, const VirtualReadOptions& options) const
    {
        const VirtualFileEntry* entry = FindByLogicalPath(logicalPath);
        if (!entry)
        {
            return MakeError(ErrorCode::NotFound, "VFS logical path not found: " + logicalPath.string());
        }
        return Read(entry->guid, options);
    }

    const std::vector<VirtualMount>& VirtualFileSystem::Mounts() const
    {
        return mMounts;
    }

    const std::vector<VirtualFileEntry>& VirtualFileSystem::Entries() const
    {
        return mEntries;
    }

    VirtualFileSystemStats VirtualFileSystem::Stats() const
    {
        VirtualFileSystemStats stats{};
        stats.mountCount = mMounts.size();
        stats.entryCount = mEntries.size();
        for (const VirtualFileEntry& entry : mEntries)
        {
            stats.payloadBytes += entry.payloadSize;
            if (entry.HasFlag(VirtualEntryFlag::LooseFile)) ++stats.looseEntries;
            if (entry.HasFlag(VirtualEntryFlag::PackagePayload)) ++stats.packageEntries;
            if (entry.HasFlag(VirtualEntryFlag::DuplicateGuid)) ++stats.duplicateGuidEntries;
            if (entry.HasFlag(VirtualEntryFlag::PathRisk)) ++stats.pathRiskEntries;
            if (entry.HasFlag(VirtualEntryFlag::MissingPayload)) ++stats.missingPayloadEntries;
        }
        stats.summary = ToDebugString(stats);
        return stats;
    }

    void VirtualFileSystem::Clear()
    {
        mMounts.clear();
        mEntries.clear();
        mGuidToEntry.clear();
    }

    void VirtualFileSystem::AddEntry(VirtualFileEntry entry)
    {
        const auto duplicate = mGuidToEntry.find(entry.guid);
        if (duplicate != mGuidToEntry.end())
        {
            entry.flags = entry.flags | VirtualEntryFlag::DuplicateGuid;
        }
        else
        {
            mGuidToEntry[entry.guid] = mEntries.size();
        }
        entry.summary = ToDebugString(entry);
        mEntries.push_back(std::move(entry));
    }

    const VirtualMount* VirtualFileSystem::FindMount(std::string_view name) const
    {
        const auto it = std::find_if(mMounts.begin(), mMounts.end(), [name](const VirtualMount& mount)
        {
            return mount.name == name;
        });
        return it == mMounts.end() ? nullptr : &(*it);
    }

    std::string_view ToString(VirtualMountKind kind)
    {
        switch (kind)
        {
            case VirtualMountKind::LooseDirectory: return "loose";
            case VirtualMountKind::AssetPackage: return "akpak";
        }
        return "unknown";
    }

    std::string_view ToString(VirtualEntryFlag flag)
    {
        switch (flag)
        {
            case VirtualEntryFlag::None: return "none";
            case VirtualEntryFlag::LooseFile: return "loose-file";
            case VirtualEntryFlag::PackagePayload: return "package-payload";
            case VirtualEntryFlag::PathRisk: return "path-risk";
            case VirtualEntryFlag::DuplicateGuid: return "duplicate-guid";
            case VirtualEntryFlag::HashMismatch: return "hash-mismatch";
            case VirtualEntryFlag::MissingPayload: return "missing-payload";
        }
        return "unknown";
    }

    std::string ToDebugString(VirtualEntryFlag flags)
    {
        if (flags == VirtualEntryFlag::None)
        {
            return "none";
        }

        std::vector<std::string_view> names;
        if (HasFlag(flags, VirtualEntryFlag::LooseFile)) names.push_back(ToString(VirtualEntryFlag::LooseFile));
        if (HasFlag(flags, VirtualEntryFlag::PackagePayload)) names.push_back(ToString(VirtualEntryFlag::PackagePayload));
        if (HasFlag(flags, VirtualEntryFlag::PathRisk)) names.push_back(ToString(VirtualEntryFlag::PathRisk));
        if (HasFlag(flags, VirtualEntryFlag::DuplicateGuid)) names.push_back(ToString(VirtualEntryFlag::DuplicateGuid));
        if (HasFlag(flags, VirtualEntryFlag::HashMismatch)) names.push_back(ToString(VirtualEntryFlag::HashMismatch));
        if (HasFlag(flags, VirtualEntryFlag::MissingPayload)) names.push_back(ToString(VirtualEntryFlag::MissingPayload));

        std::ostringstream out;
        for (usize i = 0; i < names.size(); ++i)
        {
            if (i != 0)
            {
                out << '|';
            }
            out << names[i];
        }
        return out.str();
    }

    std::string ToDebugString(const VirtualMount& mount)
    {
        std::ostringstream out;
        out << mount.name << " " << ToString(mount.kind)
            << " root=" << mount.root.generic_string()
            << " toc=" << mount.tocBytes
            << " bytes=" << mount.packageBytes;
        return out.str();
    }

    std::string ToDebugString(const VirtualFileEntry& entry)
    {
        std::ostringstream out;
        out << ToString(entry.guid)
            << " " << ToString(entry.kind)
            << " mount=" << entry.mountName
            << " path=" << entry.logicalPath.generic_string()
            << " size=" << entry.payloadSize
            << " hash=0x" << std::hex << entry.payloadHash << std::dec
            << " flags=" << ToDebugString(entry.flags);
        return out.str();
    }

    std::string ToDebugString(const VirtualFileSystemStats& stats)
    {
        std::ostringstream out;
        out << "vfs mounts=" << stats.mountCount
            << " entries=" << stats.entryCount
            << " loose=" << stats.looseEntries
            << " pak=" << stats.packageEntries
            << " bytes=" << stats.payloadBytes
            << " duplicateGuid=" << stats.duplicateGuidEntries
            << " pathRisk=" << stats.pathRiskEntries
            << " missingPayload=" << stats.missingPayloadEntries;
        return out.str();
    }

    std::string ToDebugString(const VirtualReadResult& read)
    {
        std::ostringstream out;
        out << "vfs read " << (read.ok ? "ok" : "failed")
            << " " << ToString(read.guid)
            << " bytes=" << read.bytes.size()
            << " hash=" << (read.hashValid ? "ok" : "mismatch")
            << " path=" << read.entry.logicalPath.generic_string();
        return out.str();
    }

    VirtualFileSystemProbeResult BuildVirtualFileSystemProbe()
    {
        VirtualFileSystemProbeResult result{};
        std::error_code error;
        const std::filesystem::path root = std::filesystem::temp_directory_path(error) / "akengine_vfs_probe";
        if (error)
        {
            result.summary = "VFS probe: failed to resolve temp directory";
            return result;
        }

        std::filesystem::remove_all(root, error);
        error.clear();

        Result<ProjectLayout> layoutResult = EnsureProjectLayout(root);
        if (!layoutResult)
        {
            result.summary = "VFS probe: " + layoutResult.GetError().message;
            return result;
        }
        result.layout = layoutResult.Value();

        const Result<void> writeScene = WriteTextFileAtomic(result.layout.assets / "levels" / "probe.akscene", "AKSCENE 6\nentity Probe\n");
        const Result<void> writeMaterial = WriteTextFileAtomic(result.layout.assets / "materials" / "default.akmat", "AKMAT 1\nname default\n");
        const Result<void> writeShader = WriteTextFileAtomic(result.layout.assets / "shaders" / "debug.slang", "// shader\n");

        AssetImportPolicy policy{};
        policy.hashSourceContents = true;
        policy.includeUnknown = false;
        result.manifest = BuildAssetManifest(result.layout, policy);

        Result<PackageBuildResult> packageResult = WriteAssetPackage(BuildDefaultPackagePath(result.layout, "probe"), result.layout, result.manifest);
        if (packageResult)
        {
            result.package = packageResult.Value();
        }

        VirtualFileSystem vfs;
        Result<void> mountPackage = packageResult ? vfs.MountPackage("probe-pak", result.package.packagePath) : Result<void>(MakeError(ErrorCode::InvalidState, "package not built"));
        Result<void> mountLoose = vfs.MountLooseDirectory("probe-loose", result.layout, policy);
        result.stats = vfs.Stats();

        const AssetManifestRecord* firstRecord = result.manifest.records.empty() ? nullptr : &result.manifest.records.front();
        Result<VirtualReadResult> readResult = firstRecord ? vfs.Read(firstRecord->guid) : Result<VirtualReadResult>(MakeError(ErrorCode::NotFound, "probe manifest is empty"));
        if (readResult)
        {
            result.read = std::move(readResult.Value());
        }

        result.ok = static_cast<bool>(writeScene)
            && static_cast<bool>(writeMaterial)
            && static_cast<bool>(writeShader)
            && static_cast<bool>(packageResult)
            && static_cast<bool>(mountPackage)
            && static_cast<bool>(mountLoose)
            && result.stats.mountCount == 2
            && result.stats.packageEntries == 3
            && result.stats.looseEntries == 3
            && result.stats.duplicateGuidEntries == 3
            && result.read.ok
            && result.read.bytes.size() > 0;

        result.summary = std::string("VFS probe: ") + (result.ok ? "ok " : "failed ")
            + result.stats.summary
            + " "
            + (result.read.summary.empty() ? std::string{} : result.read.summary);
        std::filesystem::remove_all(root, error);
        return result;
    }

    std::string BuildVirtualFileSystemProbeSummary()
    {
        return BuildVirtualFileSystemProbe().summary;
    }
}
