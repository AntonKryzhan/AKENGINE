#include <AK/Assets/AssetDatabase.hpp>

#include <AK/Core/Path.hpp>

#include <algorithm>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace AK
{
    namespace
    {
        std::string ToLowerAscii(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
            {
                if (c >= 'A' && c <= 'Z')
                {
                    return static_cast<char>(c - 'A' + 'a');
                }
                return static_cast<char>(c);
            });
            return text;
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

        bool IsCookableKind(AssetKind kind)
        {
            switch (kind)
            {
                case AssetKind::Scene:
                case AssetKind::Mesh:
                case AssetKind::Texture:
                case AssetKind::Material:
                case AssetKind::Audio:
                case AssetKind::Shader:
                case AssetKind::Script:
                case AssetKind::Font:
                    return true;
                case AssetKind::Unknown:
                    return false;
            }
            return false;
        }

        std::string CookedExtensionForKind(AssetKind kind)
        {
            switch (kind)
            {
                case AssetKind::Scene: return ".akscene.cooked";
                case AssetKind::Mesh: return ".akmesh";
                case AssetKind::Texture: return ".aktex";
                case AssetKind::Material: return ".akmatc";
                case AssetKind::Audio: return ".akaudio";
                case AssetKind::Shader: return ".akshader";
                case AssetKind::Script: return ".akscript";
                case AssetKind::Font: return ".akfont";
                case AssetKind::Unknown: return ".akasset";
            }
            return ".akasset";
        }

        void FinalizeStats(AssetManifest& manifest)
        {
            AssetManifestStats stats{};
            stats.truncated = manifest.stats.truncated;
            for (const AssetManifestRecord& record : manifest.records)
            {
                ++stats.totalAssets;
                stats.totalSourceBytes += record.sourceFingerprint.sizeBytes;
                if (record.kind == AssetKind::Unknown)
                {
                    ++stats.unknownAssets;
                }
                if (record.pathRisk)
                {
                    ++stats.pathRiskCount;
                }
                if (record.duplicateGuid)
                {
                    ++stats.duplicateGuidCount;
                }
                switch (record.cookStatus)
                {
                    case AssetCookStatus::Cookable:
                    case AssetCookStatus::Cooked:
                        ++stats.cookableAssets;
                        break;
                    case AssetCookStatus::SourceOnly:
                        ++stats.sourceOnlyAssets;
                        break;
                    case AssetCookStatus::MissingSource:
                    case AssetCookStatus::Unsupported:
                        ++stats.unsupportedAssets;
                        break;
                }
            }
            stats.summary = ToDebugString(stats);
            manifest.stats = std::move(stats);
            manifest.summary = ToDebugString(manifest);
        }
    }

    const AssetManifestRecord* AssetManifest::Find(AssetGuid guid) const
    {
        const auto it = std::find_if(records.begin(), records.end(), [guid](const AssetManifestRecord& record)
        {
            return record.guid == guid;
        });
        return it == records.end() ? nullptr : &(*it);
    }

    AssetKind DetectAssetKind(const std::filesystem::path& path)
    {
        const std::string ext = ToLowerAscii(path.extension().string());
        if (ext == ".akscene") return AssetKind::Scene;
        if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") return AssetKind::Mesh;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".dds" || ext == ".ktx" || ext == ".ktx2") return AssetKind::Texture;
        if (ext == ".akmat" || ext == ".mat") return AssetKind::Material;
        if (ext == ".wav" || ext == ".ogg" || ext == ".flac" || ext == ".mp3") return AssetKind::Audio;
        if (ext == ".hlsl" || ext == ".slang" || ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".comp") return AssetKind::Shader;
        if (ext == ".lua" || ext == ".luau" || ext == ".cs") return AssetKind::Script;
        if (ext == ".ttf" || ext == ".otf") return AssetKind::Font;
        return AssetKind::Unknown;
    }

    AssetCookStatus DetermineCookStatus(AssetKind kind, const FileFingerprint& fingerprint, const AssetImportPolicy& policy)
    {
        if (!fingerprint.exists || !fingerprint.regularFile)
        {
            return AssetCookStatus::MissingSource;
        }
        if (!IsCookableKind(kind) || fingerprint.sizeBytes > policy.maxSourceBytes)
        {
            return AssetCookStatus::Unsupported;
        }
        return AssetCookStatus::Cookable;
    }

    std::string_view ToString(AssetKind kind)
    {
        switch (kind)
        {
            case AssetKind::Unknown: return "unknown";
            case AssetKind::Scene: return "scene";
            case AssetKind::Mesh: return "mesh";
            case AssetKind::Texture: return "texture";
            case AssetKind::Material: return "material";
            case AssetKind::Audio: return "audio";
            case AssetKind::Shader: return "shader";
            case AssetKind::Script: return "script";
            case AssetKind::Font: return "font";
        }
        return "unknown";
    }

    std::string_view ToString(AssetCookStatus status)
    {
        switch (status)
        {
            case AssetCookStatus::SourceOnly: return "source-only";
            case AssetCookStatus::Cookable: return "cookable";
            case AssetCookStatus::Cooked: return "cooked";
            case AssetCookStatus::MissingSource: return "missing-source";
            case AssetCookStatus::Unsupported: return "unsupported";
        }
        return "unsupported";
    }

    std::string AssetImporterName(AssetKind kind)
    {
        switch (kind)
        {
            case AssetKind::Scene: return "ak.scene";
            case AssetKind::Mesh: return "ak.mesh";
            case AssetKind::Texture: return "ak.texture";
            case AssetKind::Material: return "ak.material";
            case AssetKind::Audio: return "ak.audio";
            case AssetKind::Shader: return "ak.shader";
            case AssetKind::Script: return "ak.script";
            case AssetKind::Font: return "ak.font";
            case AssetKind::Unknown: return "ak.unknown";
        }
        return "ak.unknown";
    }

    std::filesystem::path BuildCookedAssetPath(const ProjectLayout& layout, const AssetManifestRecord& record)
    {
        return NormalizePath(layout.cache / "cooked" / (ToString(record.guid) + CookedExtensionForKind(record.kind)));
    }

    AssetManifest BuildAssetManifest(const ProjectLayout& layout, const AssetImportPolicy& policy)
    {
        AssetManifest manifest{};
        manifest.assetRoot = NormalizePath(layout.assets);
        manifest.manifestPath = NormalizePath(layout.cache / "assets" / "asset_manifest.akassetdb");

        DirectoryScanOptions scanOptions{};
        scanOptions.recursive = true;
        scanOptions.includeFiles = true;
        scanOptions.includeDirectories = false;
        scanOptions.maxEntries = policy.maxAssets;

        const DirectoryScanResult scan = ScanDirectoryTree(manifest.assetRoot, scanOptions);
        manifest.stats.truncated = scan.truncated;

        std::unordered_map<AssetGuid, usize> guidToRecord;
        manifest.records.reserve(scan.entries.size());

        for (const FileEntry& entry : scan.entries)
        {
            AssetManifestRecord record{};
            record.sourcePath = NormalizePath(entry.relativePath);
            record.extension = ToLowerAscii(record.sourcePath.extension().string());
            record.kind = DetectAssetKind(record.sourcePath);
            if (!policy.includeUnknown && record.kind == AssetKind::Unknown)
            {
                continue;
            }

            record.guid = BuildAssetGuidFromPath(record.sourcePath);
            record.importerName = AssetImporterName(record.kind);
            record.sourceFingerprint = BuildFileFingerprint(entry.path, policy.hashSourceContents);
            record.cookStatus = DetermineCookStatus(record.kind, record.sourceFingerprint, policy);
            record.sourceInsideAssetRoot = IsPathInsideRoot(entry.path, manifest.assetRoot);
            record.pathRisk = AssessPathRisk(record.sourcePath).HasAnyRisk();
            record.cookedPath = MakeRelativePath(BuildCookedAssetPath(layout, record), layout.root);

            const auto duplicate = guidToRecord.find(record.guid);
            if (duplicate != guidToRecord.end())
            {
                record.duplicateGuid = true;
                if (duplicate->second < manifest.records.size())
                {
                    manifest.records[duplicate->second].duplicateGuid = true;
                    manifest.records[duplicate->second].summary = ToDebugString(manifest.records[duplicate->second]);
                }
            }
            else
            {
                guidToRecord[record.guid] = manifest.records.size();
            }

            record.summary = ToDebugString(record);
            manifest.records.push_back(std::move(record));
        }

        std::sort(manifest.records.begin(), manifest.records.end(), [](const AssetManifestRecord& a, const AssetManifestRecord& b)
        {
            return a.sourcePath.generic_string() < b.sourcePath.generic_string();
        });

        FinalizeStats(manifest);
        return manifest;
    }

    Result<void> WriteAssetManifestText(const std::filesystem::path& path, const AssetManifest& manifest)
    {
        std::ostringstream out;
        out << "AKASSETDB 1\n";
        out << "asset_root " << Quote(manifest.assetRoot.generic_string()) << "\n";
        out << "records " << manifest.records.size() << "\n";
        out << "stats total=" << manifest.stats.totalAssets
            << " cookable=" << manifest.stats.cookableAssets
            << " unknown=" << manifest.stats.unknownAssets
            << " duplicate_guid=" << manifest.stats.duplicateGuidCount
            << " path_risk=" << manifest.stats.pathRiskCount
            << " bytes=" << manifest.stats.totalSourceBytes
            << " truncated=" << (manifest.stats.truncated ? "yes" : "no") << "\n";

        for (const AssetManifestRecord& record : manifest.records)
        {
            out << "asset " << ToString(record.guid)
                << " kind=" << ToString(record.kind)
                << " status=" << ToString(record.cookStatus)
                << " importer=" << record.importerName
                << " bytes=" << record.sourceFingerprint.sizeBytes
                << " mtime=" << record.sourceFingerprint.writeTimeTicks
                << " hash=0x" << std::hex << record.sourceFingerprint.contentHash << std::dec
                << " duplicate=" << (record.duplicateGuid ? "yes" : "no")
                << " risk=" << (record.pathRisk ? "yes" : "no")
                << " source=" << Quote(record.sourcePath.generic_string())
                << " cooked=" << Quote(record.cookedPath.generic_string()) << "\n";
        }

        return WriteTextFileAtomic(path, out.str());
    }

    std::string ToDebugString(const AssetManifestRecord& record)
    {
        std::ostringstream out;
        out << ToString(record.kind)
            << " " << ToString(record.guid)
            << " " << ToString(record.cookStatus)
            << " " << record.sourcePath.generic_string()
            << " -> " << record.cookedPath.generic_string();
        if (record.duplicateGuid)
        {
            out << " duplicate-guid";
        }
        if (record.pathRisk)
        {
            out << " path-risk";
        }
        return out.str();
    }

    std::string ToDebugString(const AssetManifestStats& stats)
    {
        std::ostringstream out;
        out << "assets total=" << stats.totalAssets
            << " cookable=" << stats.cookableAssets
            << " sourceOnly=" << stats.sourceOnlyAssets
            << " unsupported=" << stats.unsupportedAssets
            << " unknown=" << stats.unknownAssets
            << " duplicateGuid=" << stats.duplicateGuidCount
            << " pathRisk=" << stats.pathRiskCount
            << " bytes=" << stats.totalSourceBytes
            << " truncated=" << (stats.truncated ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const AssetManifest& manifest)
    {
        std::ostringstream out;
        out << "assetdb root=" << manifest.assetRoot.generic_string() << " " << ToDebugString(manifest.stats);
        return out.str();
    }

    AssetDatabaseProbeResult BuildAssetDatabaseProbe()
    {
        AssetDatabaseProbeResult result{};
        std::error_code error;
        const std::filesystem::path root = std::filesystem::temp_directory_path(error) / "akengine_assetdb_probe";
        if (error)
        {
            result.summary = "AssetDB probe: failed to resolve temp directory";
            return result;
        }

        std::filesystem::remove_all(root, error);
        error.clear();

        Result<ProjectLayout> layoutResult = EnsureProjectLayout(root);
        if (!layoutResult)
        {
            result.summary = "AssetDB probe: " + layoutResult.GetError().message;
            return result;
        }
        result.layout = layoutResult.Value();

        const Result<void> writeScene = WriteTextFileAtomic(result.layout.assets / "levels" / "probe.akscene", "AKSCENE 6\n");
        const Result<void> writeMaterial = WriteTextFileAtomic(result.layout.assets / "materials" / "default.akmat", "AKMAT 1\n");
        const Result<void> writeShader = WriteTextFileAtomic(result.layout.assets / "shaders" / "debug.slang", "// shader\n");
        const Result<void> writeUnknown = WriteTextFileAtomic(result.layout.assets / "notes" / "readme.txt", "notes\n");

        AssetImportPolicy policy{};
        policy.hashSourceContents = true;
        result.manifest = BuildAssetManifest(result.layout, policy);
        result.writtenManifestPath = result.manifest.manifestPath;
        const Result<void> writeManifest = WriteAssetManifestText(result.writtenManifestPath, result.manifest);
        const Result<std::string> readManifest = ReadTextFile(result.writtenManifestPath);

        result.ok = static_cast<bool>(writeScene)
            && static_cast<bool>(writeMaterial)
            && static_cast<bool>(writeShader)
            && static_cast<bool>(writeUnknown)
            && static_cast<bool>(writeManifest)
            && static_cast<bool>(readManifest)
            && readManifest.Value().rfind("AKASSETDB 1", 0) == 0
            && result.manifest.stats.totalAssets == 4
            && result.manifest.stats.cookableAssets == 3
            && result.manifest.stats.unknownAssets == 1
            && result.manifest.stats.duplicateGuidCount == 0;

        result.summary = std::string("AssetDB probe: ") + (result.ok ? "ok " : "failed ") + result.manifest.summary;
        std::filesystem::remove_all(root, error);
        return result;
    }

    std::string BuildAssetDatabaseProbeSummary()
    {
        return BuildAssetDatabaseProbe().summary;
    }
}
