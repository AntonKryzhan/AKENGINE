#include <AK/Package/AssetPackage.hpp>

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

        bool HasFlag(PackageEntryFlag flags, PackageEntryFlag flag)
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

        std::string Quote(std::string_view text)
        {
            std::string out;
            out.reserve(text.size() + 2);
            out.push_back('"');
            for (const char c : text)
            {
                if (c == '\\' || c == '"')
                {
                    out.push_back('\\');
                }
                if (c == '\n')
                {
                    out += "\\n";
                }
                else if (c == '\r')
                {
                    out += "\\r";
                }
                else
                {
                    out.push_back(c);
                }
            }
            out.push_back('"');
            return out;
        }

        bool IsPackageCandidate(const AssetManifestRecord& record)
        {
            return record.cookStatus == AssetCookStatus::Cookable || record.cookStatus == AssetCookStatus::Cooked;
        }

        std::filesystem::path AbsoluteSourcePath(const ProjectLayout& layout, const AssetManifestRecord& record)
        {
            if (record.sourcePath.is_absolute())
            {
                return NormalizePath(record.sourcePath);
            }
            return NormalizePath(layout.assets / record.sourcePath);
        }

        void AppendU8(std::vector<u8>& dst, std::string_view text)
        {
            dst.insert(dst.end(), text.begin(), text.end());
        }

        std::string BuildTocText(const PackageBuildResult& package)
        {
            std::ostringstream out;
            out << PackageMagic << "\n";
            out << "entries " << package.entries.size() << "\n";
            out << "payload_bytes " << package.stats.payloadBytes << "\n";
            out << "stats unsupported=" << package.stats.skippedUnsupported
                << " missing=" << package.stats.skippedMissing
                << " too_large=" << package.stats.skippedTooLarge
                << " path_risk=" << package.stats.pathRiskEntries
                << " duplicate_guid=" << package.stats.duplicateGuidEntries
                << " truncated=" << (package.stats.truncated ? "yes" : "no") << "\n";

            for (const PackageEntry& entry : package.entries)
            {
                out << "entry " << ToString(entry.guid)
                    << " kind=" << ToString(entry.kind)
                    << " flags=0x" << std::hex << static_cast<u32>(entry.flags) << std::dec
                    << " offset=" << entry.payloadOffset
                    << " size=" << entry.payloadSize
                    << " hash=0x" << std::hex << entry.payloadHash << std::dec
                    << " importer=" << entry.importerName
                    << " source=" << Quote(entry.sourcePath.generic_string())
                    << " cooked=" << Quote(entry.cookedPath.generic_string()) << "\n";
            }

            out << "ENDTOC\n";
            return out.str();
        }

        bool ParseUnsignedAfterPrefix(std::string_view line, std::string_view prefix, u64& value)
        {
            if (line.rfind(prefix, 0) != 0)
            {
                return false;
            }

            const std::string_view number = line.substr(prefix.size());
            const char* begin = number.data();
            const char* end = begin + number.size();
            const auto result = std::from_chars(begin, end, value);
            return result.ec == std::errc{} && result.ptr == end;
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

        void FinalizeStats(PackageBuildResult& result)
        {
            PackageBuildStats stats{};
            stats.deterministic = result.stats.deterministic;
            stats.truncated = result.stats.truncated;
            stats.skippedUnsupported = result.stats.skippedUnsupported;
            stats.skippedMissing = result.stats.skippedMissing;
            stats.skippedTooLarge = result.stats.skippedTooLarge;
            stats.duplicateGuidEntries = result.stats.duplicateGuidEntries;
            for (const PackageEntry& entry : result.entries)
            {
                ++stats.entryCount;
                stats.payloadBytes += entry.payloadSize;
                if (entry.HasFlag(PackageEntryFlag::SourceBytes))
                {
                    ++stats.sourceByteEntries;
                }
                if (entry.HasFlag(PackageEntryFlag::CookedBytes))
                {
                    ++stats.cookedByteEntries;
                }
                if (entry.HasFlag(PackageEntryFlag::PathRisk))
                {
                    ++stats.pathRiskEntries;
                }
            }
            result.stats = std::move(stats);
        }
    }

    bool PackageEntry::HasFlag(PackageEntryFlag flag) const
    {
        return AK::HasFlag(flags, flag);
    }

    std::filesystem::path BuildDefaultPackagePath(const ProjectLayout& layout, std::string_view packageName)
    {
        std::string safeName(packageName);
        if (safeName.empty())
        {
            safeName = "sandbox";
        }
        return NormalizePath(layout.cache / "packages" / (safeName + ".akpak"));
    }

    PackageBuildResult BuildAssetPackage(const ProjectLayout& layout, const AssetManifest& manifest, const PackageBuildOptions& options)
    {
        PackageBuildResult result{};
        result.packagePath = BuildDefaultPackagePath(layout);
        result.stats.deterministic = options.deterministicOrder;
        result.entries.reserve(std::min(manifest.records.size(), options.maxEntries));

        std::vector<const AssetManifestRecord*> records;
        records.reserve(manifest.records.size());
        for (const AssetManifestRecord& record : manifest.records)
        {
            records.push_back(&record);
        }

        if (options.deterministicOrder)
        {
            std::sort(records.begin(), records.end(), [](const AssetManifestRecord* a, const AssetManifestRecord* b)
            {
                if (a->guid != b->guid)
                {
                    return ToString(a->guid) < ToString(b->guid);
                }
                return a->sourcePath.generic_string() < b->sourcePath.generic_string();
            });
        }

        u64 payloadOffset = 0;
        for (const AssetManifestRecord* record : records)
        {
            if (result.entries.size() >= options.maxEntries)
            {
                result.stats.truncated = true;
                continue;
            }

            if (!IsPackageCandidate(*record))
            {
                ++result.stats.skippedUnsupported;
                continue;
            }

            const std::filesystem::path sourcePath = AbsoluteSourcePath(layout, *record);
            const FileFingerprint fingerprint = BuildFileFingerprint(sourcePath, options.hashPayload);
            if (!fingerprint.exists || !fingerprint.regularFile)
            {
                ++result.stats.skippedMissing;
                continue;
            }
            if (fingerprint.sizeBytes > options.maxEntryBytes)
            {
                ++result.stats.skippedTooLarge;
                continue;
            }

            PackageEntry entry{};
            entry.guid = record->guid;
            entry.kind = record->kind;
            entry.flags = options.includeSourceBytes ? PackageEntryFlag::SourceBytes : PackageEntryFlag::None;
            if (record->pathRisk)
            {
                entry.flags = entry.flags | PackageEntryFlag::PathRisk;
            }
            entry.sourcePath = record->sourcePath;
            entry.cookedPath = record->cookedPath;
            entry.payloadOffset = payloadOffset;
            entry.payloadSize = options.includeSourceBytes ? fingerprint.sizeBytes : 0;
            entry.payloadHash = fingerprint.contentHash;
            entry.importerName = record->importerName;
            entry.summary = ToDebugString(entry);

            payloadOffset += entry.payloadSize;
            if (record->duplicateGuid)
            {
                ++result.stats.duplicateGuidEntries;
            }
            result.entries.push_back(std::move(entry));
        }

        FinalizeStats(result);
        result.ok = !result.stats.truncated && result.stats.duplicateGuidEntries == 0;
        result.stats.summary = ToDebugString(result.stats);
        result.summary = ToDebugString(result);
        return result;
    }

    Result<PackageBuildResult> WriteAssetPackage(const std::filesystem::path& path, const ProjectLayout& layout, const AssetManifest& manifest, const PackageBuildOptions& options)
    {
        PackageBuildResult result = BuildAssetPackage(layout, manifest, options);
        result.packagePath = NormalizePath(path);

        std::vector<u8> payload;
        payload.reserve(static_cast<usize>(std::min<u64>(result.stats.payloadBytes, 64ull * 1024ull * 1024ull)));
        for (PackageEntry& entry : result.entries)
        {
            entry.payloadOffset = static_cast<u64>(payload.size());
            if (entry.payloadSize == 0 || !entry.HasFlag(PackageEntryFlag::SourceBytes))
            {
                continue;
            }

            const std::filesystem::path sourcePath = entry.sourcePath.is_absolute() ? NormalizePath(entry.sourcePath) : NormalizePath(layout.assets / entry.sourcePath);
            Result<std::vector<u8>> bytes = ReadBinaryFile(sourcePath, options.maxEntryBytes);
            if (!bytes)
            {
                result.warnings.push_back("failed to read package entry: " + entry.sourcePath.generic_string());
                entry.flags = entry.flags | PackageEntryFlag::MissingSource;
                entry.payloadSize = 0;
                continue;
            }

            entry.payloadSize = static_cast<u64>(bytes.Value().size());
            if (options.hashPayload)
            {
                entry.payloadHash = Fnv1a64Bytes(bytes.Value().data(), bytes.Value().size());
            }
            payload.insert(payload.end(), bytes.Value().begin(), bytes.Value().end());
            entry.summary = ToDebugString(entry);
        }

        FinalizeStats(result);
        result.stats.payloadBytes = static_cast<u64>(payload.size());

        const std::string tocText = BuildTocText(result);
        std::vector<u8> bytes;
        bytes.reserve(tocText.size() + payload.size());
        AppendU8(bytes, tocText);
        bytes.insert(bytes.end(), payload.begin(), payload.end());

        result.stats.tocBytes = static_cast<u64>(tocText.size());
        result.stats.packageBytes = static_cast<u64>(bytes.size());
        result.stats.summary = ToDebugString(result.stats);
        result.ok = result.warnings.empty() && result.stats.duplicateGuidEntries == 0 && !result.stats.truncated;
        result.summary = ToDebugString(result);

        Result<void> write = WriteBinaryFileAtomic(result.packagePath, bytes);
        if (!write)
        {
            return write.GetError();
        }

        return Ok(std::move(result));
    }

    Result<PackageHeader> ReadAssetPackageHeader(const std::filesystem::path& path)
    {
        Result<std::vector<u8>> bytes = ReadBinaryFile(path, 64ull * 1024ull * 1024ull);
        if (!bytes)
        {
            return bytes.GetError();
        }

        const std::string_view text(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size());
        const usize endToc = text.find("ENDTOC\n");
        if (endToc == std::string_view::npos)
        {
            return MakeError(ErrorCode::ParseError, "package TOC terminator not found: " + path.string());
        }

        const std::string_view toc = text.substr(0, endToc + 7);
        const std::vector<std::string_view> lines = SplitLines(toc);
        if (lines.empty() || lines[0] != PackageMagic)
        {
            return MakeError(ErrorCode::UnsupportedVersion, "unsupported package magic: " + path.string());
        }

        PackageHeader header{};
        header.valid = true;
        header.version = 1;
        header.tocBytes = static_cast<u64>(endToc + 7);
        header.packageBytes = static_cast<u64>(bytes.Value().size());
        for (const std::string_view line : lines)
        {
            u64 parsed = 0;
            if (ParseUnsignedAfterPrefix(line, "entries ", parsed))
            {
                header.entryCount = parsed;
            }
            else if (ParseUnsignedAfterPrefix(line, "payload_bytes ", parsed))
            {
                header.payloadBytes = parsed;
            }
        }

        if (header.tocBytes + header.payloadBytes > header.packageBytes)
        {
            header.valid = false;
            header.summary = ToDebugString(header);
            return MakeError(ErrorCode::ParseError, "package payload exceeds file size: " + path.string());
        }

        header.summary = ToDebugString(header);
        return Ok(header);
    }

    std::string_view ToString(PackageEntryFlag flag)
    {
        switch (flag)
        {
            case PackageEntryFlag::None: return "none";
            case PackageEntryFlag::SourceBytes: return "source-bytes";
            case PackageEntryFlag::CookedBytes: return "cooked-bytes";
            case PackageEntryFlag::PathRisk: return "path-risk";
            case PackageEntryFlag::MissingSource: return "missing-source";
            case PackageEntryFlag::Unsupported: return "unsupported";
        }
        return "unknown";
    }

    std::string ToDebugString(PackageEntryFlag flags)
    {
        if (flags == PackageEntryFlag::None)
        {
            return "none";
        }

        std::vector<std::string_view> names;
        if (HasFlag(flags, PackageEntryFlag::SourceBytes)) names.push_back(ToString(PackageEntryFlag::SourceBytes));
        if (HasFlag(flags, PackageEntryFlag::CookedBytes)) names.push_back(ToString(PackageEntryFlag::CookedBytes));
        if (HasFlag(flags, PackageEntryFlag::PathRisk)) names.push_back(ToString(PackageEntryFlag::PathRisk));
        if (HasFlag(flags, PackageEntryFlag::MissingSource)) names.push_back(ToString(PackageEntryFlag::MissingSource));
        if (HasFlag(flags, PackageEntryFlag::Unsupported)) names.push_back(ToString(PackageEntryFlag::Unsupported));

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

    std::string ToDebugString(const PackageEntry& entry)
    {
        std::ostringstream out;
        out << ToString(entry.guid)
            << " " << ToString(entry.kind)
            << " offset=" << entry.payloadOffset
            << " size=" << entry.payloadSize
            << " hash=0x" << std::hex << entry.payloadHash << std::dec
            << " flags=" << ToDebugString(entry.flags)
            << " source=" << entry.sourcePath.generic_string();
        return out.str();
    }

    std::string ToDebugString(const PackageBuildStats& stats)
    {
        std::ostringstream out;
        out << "package entries=" << stats.entryCount
            << " payload=" << stats.payloadBytes
            << " toc=" << stats.tocBytes
            << " bytes=" << stats.packageBytes
            << " source=" << stats.sourceByteEntries
            << " cooked=" << stats.cookedByteEntries
            << " skippedUnsupported=" << stats.skippedUnsupported
            << " skippedMissing=" << stats.skippedMissing
            << " skippedTooLarge=" << stats.skippedTooLarge
            << " pathRisk=" << stats.pathRiskEntries
            << " duplicateGuid=" << stats.duplicateGuidEntries
            << " truncated=" << (stats.truncated ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const PackageBuildResult& package)
    {
        std::ostringstream out;
        out << "akpak " << (package.ok ? "ok " : "not-ready ")
            << package.packagePath.generic_string()
            << " " << ToDebugString(package.stats)
            << " warnings=" << package.warnings.size();
        return out.str();
    }

    std::string ToDebugString(const PackageHeader& header)
    {
        std::ostringstream out;
        out << "akpak header valid=" << (header.valid ? "yes" : "no")
            << " version=" << header.version
            << " entries=" << header.entryCount
            << " payload=" << header.payloadBytes
            << " toc=" << header.tocBytes
            << " bytes=" << header.packageBytes;
        return out.str();
    }

    PackageProbeResult BuildPackageProbe()
    {
        PackageProbeResult result{};
        std::error_code error;
        const std::filesystem::path root = std::filesystem::temp_directory_path(error) / "akengine_package_probe";
        if (error)
        {
            result.summary = "Package probe: failed to resolve temp directory";
            return result;
        }

        std::filesystem::remove_all(root, error);
        error.clear();

        Result<ProjectLayout> layoutResult = EnsureProjectLayout(root);
        if (!layoutResult)
        {
            result.summary = "Package probe: " + layoutResult.GetError().message;
            return result;
        }
        result.layout = layoutResult.Value();

        const Result<void> writeScene = WriteTextFileAtomic(result.layout.assets / "levels" / "probe.akscene", "AKSCENE 6\nentity Probe\n");
        const Result<void> writeMaterial = WriteTextFileAtomic(result.layout.assets / "materials" / "default.akmat", "AKMAT 1\n");
        const Result<void> writeShader = WriteTextFileAtomic(result.layout.assets / "shaders" / "debug.slang", "// shader\n");

        AssetImportPolicy importPolicy{};
        importPolicy.hashSourceContents = true;
        importPolicy.includeUnknown = false;
        result.manifest = BuildAssetManifest(result.layout, importPolicy);

        PackageBuildOptions packageOptions{};
        packageOptions.hashPayload = true;
        const std::filesystem::path packagePath = BuildDefaultPackagePath(result.layout, "probe");
        Result<PackageBuildResult> packageResult = WriteAssetPackage(packagePath, result.layout, result.manifest, packageOptions);
        if (packageResult)
        {
            result.package = packageResult.Value();
            Result<PackageHeader> headerResult = ReadAssetPackageHeader(result.package.packagePath);
            if (headerResult)
            {
                result.header = headerResult.Value();
            }
        }

        result.ok = static_cast<bool>(writeScene)
            && static_cast<bool>(writeMaterial)
            && static_cast<bool>(writeShader)
            && static_cast<bool>(packageResult)
            && result.package.ok
            && result.header.valid
            && result.header.entryCount == 3
            && result.header.payloadBytes > 0
            && result.header.tocBytes > 0
            && result.header.packageBytes == result.header.tocBytes + result.header.payloadBytes;

        result.summary = std::string("Package probe: ") + (result.ok ? "ok " : "failed ")
            + (result.package.summary.empty() ? std::string{} : result.package.summary + " ")
            + result.header.summary;
        std::filesystem::remove_all(root, error);
        return result;
    }

    std::string BuildPackageProbeSummary()
    {
        return BuildPackageProbe().summary;
    }
}
