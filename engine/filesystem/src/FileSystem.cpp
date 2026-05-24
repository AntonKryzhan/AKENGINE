#include <AK/Filesystem/FileSystem.hpp>

#include <AK/Core/Path.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace AK
{
    namespace
    {
        constexpr u64 MaxSafeWindowsPathLength = 240;

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

        bool HasFlag(PathRiskFlag flags, PathRiskFlag flag)
        {
            return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0u;
        }

        bool ContainsNonAscii(std::string_view text)
        {
            return std::any_of(text.begin(), text.end(), [](unsigned char c)
            {
                return c >= 128u;
            });
        }

        bool ContainsParentTraversal(const std::filesystem::path& path)
        {
            for (const std::filesystem::path& part : path)
            {
                if (part == "..")
                {
                    return true;
                }
            }
            return false;
        }

        i64 LastWriteTimeTicks(const std::filesystem::path& path)
        {
            std::error_code error;
            const auto time = std::filesystem::last_write_time(path, error);
            if (error)
            {
                return 0;
            }
            return static_cast<i64>(time.time_since_epoch().count());
        }

        u64 FileSizeBytes(const std::filesystem::path& path)
        {
            std::error_code error;
            const auto size = std::filesystem::file_size(path, error);
            if (error)
            {
                return 0;
            }
            return static_cast<u64>(size);
        }

        Result<void> CreateDirectoryIfNeeded(const std::filesystem::path& path)
        {
            std::error_code error;
            if (path.empty())
            {
                return MakeError(ErrorCode::InvalidArgument, "directory path is empty");
            }

            std::filesystem::create_directories(path, error);
            if (error)
            {
                return MakeError(ErrorCode::IoError, "failed to create directory: " + path.string());
            }

            return Ok();
        }

        std::filesystem::path TempSiblingPath(const std::filesystem::path& path)
        {
            return path.string() + ".tmp";
        }
    }

    bool PathRiskReport::HasRisk(PathRiskFlag flag) const
    {
        return HasFlag(flags, flag);
    }

    bool PathRiskReport::HasAnyRisk() const
    {
        return flags != PathRiskFlag::None;
    }

    std::filesystem::path NormalizeProjectRoot(const std::filesystem::path& root)
    {
        if (root.empty())
        {
            return std::filesystem::current_path().lexically_normal();
        }
        return root.lexically_normal();
    }

    bool IsPathInsideRoot(const std::filesystem::path& path, const std::filesystem::path& root)
    {
        const std::filesystem::path normalizedPath = NormalizePath(path);
        const std::filesystem::path normalizedRoot = NormalizePath(root);
        const std::string pathText = NormalizePathString(normalizedPath);
        std::string rootText = NormalizePathString(normalizedRoot);
        if (!rootText.empty() && rootText.back() != '/')
        {
            rootText.push_back('/');
        }
        return pathText == NormalizePathString(normalizedRoot) || pathText.rfind(rootText, 0) == 0;
    }

    PathRiskReport AssessPathRisk(const std::filesystem::path& path)
    {
        PathRiskReport report{};
        const std::string original = path.generic_string();
        const std::string normalized = NormalizePathString(path);
        report.normalized = normalized;

        if (original.empty())
        {
            report.flags = report.flags | PathRiskFlag::EmptyPath;
        }
        if (NormalizePath(path).generic_string() != path.generic_string())
        {
            report.flags = report.flags | PathRiskFlag::NotNormalized;
        }
        if (ContainsParentTraversal(path))
        {
            report.flags = report.flags | PathRiskFlag::ContainsParentTraversal;
        }
        if (path.generic_string().size() >= MaxSafeWindowsPathLength)
        {
            report.flags = report.flags | PathRiskFlag::LongPathRisk;
        }
        if (ContainsNonAscii(original))
        {
            report.flags = report.flags | PathRiskFlag::ContainsNonAscii;
        }

        report.summary = ToDebugString(report);
        return report;
    }

    ProjectLayout BuildProjectLayout(const std::filesystem::path& root)
    {
        ProjectLayout layout{};
        layout.root = NormalizeProjectRoot(root);
        layout.assets = layout.root / "assets";
        layout.projects = layout.root / "projects";
        layout.cache = layout.root / ".akcache";
        layout.logs = layout.root / "logs";
        layout.temp = layout.root / "temp";
        layout.intermediate = layout.root / "intermediate";
        layout.shaderCache = layout.cache / "shaders";
        layout.valid = true;

        const PathRiskReport rootRisk = AssessPathRisk(layout.root);
        if (rootRisk.HasRisk(PathRiskFlag::EmptyPath) || rootRisk.HasRisk(PathRiskFlag::ContainsParentTraversal))
        {
            layout.valid = false;
            layout.warnings.push_back("project root has unsafe path traversal or is empty");
        }
        if (rootRisk.HasRisk(PathRiskFlag::LongPathRisk))
        {
            layout.warnings.push_back("project root is close to classic Windows MAX_PATH risk");
        }
        if (rootRisk.HasRisk(PathRiskFlag::ContainsNonAscii))
        {
            layout.warnings.push_back("project root contains non-ASCII characters; keep UTF-8/UTF-16 boundary strict");
        }

        layout.summary = ToDebugString(layout);
        return layout;
    }

    Result<ProjectLayout> EnsureProjectLayout(const std::filesystem::path& root)
    {
        ProjectLayout layout = BuildProjectLayout(root);
        if (!layout.valid)
        {
            return MakeError(ErrorCode::InvalidArgument, "invalid project layout root: " + layout.root.string());
        }

        const std::filesystem::path directories[] = {
            layout.root,
            layout.assets,
            layout.projects,
            layout.cache,
            layout.logs,
            layout.temp,
            layout.intermediate,
            layout.shaderCache
        };

        for (const std::filesystem::path& directory : directories)
        {
            Result<void> result = CreateDirectoryIfNeeded(directory);
            if (!result)
            {
                return result.GetError();
            }
        }

        layout.summary = ToDebugString(layout);
        return Ok(layout);
    }

    Result<std::string> ReadTextFile(const std::filesystem::path& path, u64 maxBytes)
    {
        const FileFingerprint fingerprint = BuildFileFingerprint(path, false);
        if (!fingerprint.exists || !fingerprint.regularFile)
        {
            return MakeError(ErrorCode::NotFound, "text file not found: " + path.string());
        }
        if (fingerprint.sizeBytes > maxBytes)
        {
            return MakeError(ErrorCode::InvalidArgument, "text file too large: " + path.string());
        }

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return MakeError(ErrorCode::IoError, "failed to open text file: " + path.string());
        }

        std::string text;
        text.resize(static_cast<usize>(fingerprint.sizeBytes));
        if (!text.empty())
        {
            file.read(text.data(), static_cast<std::streamsize>(text.size()));
        }
        if (!file && !file.eof())
        {
            return MakeError(ErrorCode::IoError, "failed to read text file: " + path.string());
        }

        return Ok(std::move(text));
    }

    Result<std::vector<u8>> ReadBinaryFile(const std::filesystem::path& path, u64 maxBytes)
    {
        const FileFingerprint fingerprint = BuildFileFingerprint(path, false);
        if (!fingerprint.exists || !fingerprint.regularFile)
        {
            return MakeError(ErrorCode::NotFound, "binary file not found: " + path.string());
        }
        if (fingerprint.sizeBytes > maxBytes)
        {
            return MakeError(ErrorCode::InvalidArgument, "binary file too large: " + path.string());
        }

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return MakeError(ErrorCode::IoError, "failed to open binary file: " + path.string());
        }

        std::vector<u8> bytes(static_cast<usize>(fingerprint.sizeBytes));
        if (!bytes.empty())
        {
            file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        if (!file && !file.eof())
        {
            return MakeError(ErrorCode::IoError, "failed to read binary file: " + path.string());
        }

        return Ok(std::move(bytes));
    }

    Result<void> WriteTextFileAtomic(const std::filesystem::path& path, std::string_view text)
    {
        return AtomicWriteTextFile(path, text);
    }

    Result<void> WriteBinaryFileAtomic(const std::filesystem::path& path, const std::vector<u8>& bytes)
    {
        const std::filesystem::path normalizedPath = NormalizePath(path);
        const std::filesystem::path parent = normalizedPath.parent_path();
        if (!parent.empty())
        {
            Result<void> createDirectory = CreateDirectoryIfNeeded(parent);
            if (!createDirectory)
            {
                return createDirectory;
            }
        }

        const std::filesystem::path tempPath = TempSiblingPath(normalizedPath);
        {
            std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                return MakeError(ErrorCode::IoError, "failed to open temp binary file: " + tempPath.string());
            }
            if (!bytes.empty())
            {
                file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            }
            file.flush();
            if (!file)
            {
                return MakeError(ErrorCode::IoError, "failed to write temp binary file: " + tempPath.string());
            }
        }

        std::error_code error;
        std::filesystem::remove(normalizedPath, error);
        error.clear();
        std::filesystem::rename(tempPath, normalizedPath, error);
        if (error)
        {
            std::filesystem::remove(tempPath, error);
            return MakeError(ErrorCode::IoError, "failed to replace binary file: " + normalizedPath.string());
        }

        return Ok();
    }

    FileFingerprint BuildFileFingerprint(const std::filesystem::path& path, bool hashContents)
    {
        FileFingerprint fingerprint{};
        fingerprint.path = NormalizePath(path);

        std::error_code error;
        fingerprint.exists = std::filesystem::exists(fingerprint.path, error);
        if (error || !fingerprint.exists)
        {
            fingerprint.summary = ToDebugString(fingerprint);
            return fingerprint;
        }

        fingerprint.regularFile = std::filesystem::is_regular_file(fingerprint.path, error);
        if (error || !fingerprint.regularFile)
        {
            fingerprint.summary = ToDebugString(fingerprint);
            return fingerprint;
        }

        fingerprint.sizeBytes = FileSizeBytes(fingerprint.path);
        fingerprint.writeTimeTicks = LastWriteTimeTicks(fingerprint.path);

        if (hashContents)
        {
            Result<std::vector<u8>> bytes = ReadBinaryFile(fingerprint.path);
            if (bytes)
            {
                fingerprint.contentHash = Fnv1a64Bytes(bytes.Value().data(), bytes.Value().size());
            }
        }

        fingerprint.summary = ToDebugString(fingerprint);
        return fingerprint;
    }

    DirectoryScanResult ScanDirectoryTree(const std::filesystem::path& root, const DirectoryScanOptions& options)
    {
        DirectoryScanResult result{};
        result.root = NormalizePath(root);

        std::error_code error;
        if (!std::filesystem::exists(result.root, error) || error)
        {
            result.summary = ToDebugString(result);
            return result;
        }

        const auto addEntry = [&](const std::filesystem::directory_entry& entry)
        {
            if (result.entries.size() >= options.maxEntries)
            {
                ++result.skippedEntries;
                result.truncated = true;
                return;
            }

            FileEntry fileEntry{};
            fileEntry.path = NormalizePath(entry.path());
            fileEntry.relativePath = MakeRelativePath(fileEntry.path, result.root);
            fileEntry.directory = entry.is_directory(error);
            error.clear();
            fileEntry.regularFile = entry.is_regular_file(error);
            error.clear();
            if (fileEntry.regularFile)
            {
                fileEntry.sizeBytes = FileSizeBytes(fileEntry.path);
                fileEntry.writeTimeTicks = LastWriteTimeTicks(fileEntry.path);
                ++result.fileCount;
                result.totalBytes += fileEntry.sizeBytes;
            }
            else if (fileEntry.directory)
            {
                ++result.directoryCount;
            }

            const bool include = (fileEntry.regularFile && options.includeFiles) || (fileEntry.directory && options.includeDirectories);
            if (include)
            {
                result.entries.push_back(std::move(fileEntry));
            }
        };

        if (options.recursive)
        {
            std::filesystem::recursive_directory_iterator it(result.root, std::filesystem::directory_options::skip_permission_denied, error);
            const std::filesystem::recursive_directory_iterator end{};
            while (!error && it != end)
            {
                addEntry(*it);
                it.increment(error);
            }
        }
        else
        {
            std::filesystem::directory_iterator it(result.root, std::filesystem::directory_options::skip_permission_denied, error);
            const std::filesystem::directory_iterator end{};
            while (!error && it != end)
            {
                addEntry(*it);
                it.increment(error);
            }
        }

        std::sort(result.entries.begin(), result.entries.end(), [](const FileEntry& a, const FileEntry& b)
        {
            return a.relativePath.generic_string() < b.relativePath.generic_string();
        });

        result.summary = ToDebugString(result);
        return result;
    }

    std::string ToDebugString(PathRiskFlag flags)
    {
        if (flags == PathRiskFlag::None)
        {
            return "none";
        }

        std::vector<std::string> names;
        if (HasFlag(flags, PathRiskFlag::NotNormalized))
        {
            names.push_back("not-normalized");
        }
        if (HasFlag(flags, PathRiskFlag::ContainsParentTraversal))
        {
            names.push_back("parent-traversal");
        }
        if (HasFlag(flags, PathRiskFlag::LongPathRisk))
        {
            names.push_back("long-path-risk");
        }
        if (HasFlag(flags, PathRiskFlag::ContainsNonAscii))
        {
            names.push_back("non-ascii");
        }
        if (HasFlag(flags, PathRiskFlag::EmptyPath))
        {
            names.push_back("empty");
        }

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

    std::string ToDebugString(const PathRiskReport& report)
    {
        std::ostringstream out;
        out << "path risk=" << ToDebugString(report.flags) << " normalized=" << report.normalized;
        return out.str();
    }

    std::string ToDebugString(const ProjectLayout& layout)
    {
        std::ostringstream out;
        out << "layout root=" << layout.root.generic_string()
            << " valid=" << (layout.valid ? "yes" : "no")
            << " dirs=8 warnings=" << layout.warnings.size();
        return out.str();
    }

    std::string ToDebugString(const FileFingerprint& fingerprint)
    {
        std::ostringstream out;
        out << "file exists=" << (fingerprint.exists ? "yes" : "no")
            << " regular=" << (fingerprint.regularFile ? "yes" : "no")
            << " bytes=" << fingerprint.sizeBytes
            << " hash=0x" << std::hex << fingerprint.contentHash << std::dec;
        return out.str();
    }

    std::string ToDebugString(const DirectoryScanResult& scan)
    {
        std::ostringstream out;
        out << "scan files=" << scan.fileCount
            << " dirs=" << scan.directoryCount
            << " bytes=" << scan.totalBytes
            << " entries=" << scan.entries.size()
            << " skipped=" << scan.skippedEntries
            << " truncated=" << (scan.truncated ? "yes" : "no");
        return out.str();
    }

    FilesystemProbeResult BuildFilesystemProbe()
    {
        FilesystemProbeResult result{};
        std::error_code error;
        const std::filesystem::path root = std::filesystem::temp_directory_path(error) / "akengine_filesystem_probe";
        if (error)
        {
            result.summary = "Filesystem probe: failed to resolve temp directory";
            return result;
        }

        std::filesystem::remove_all(root, error);
        error.clear();

        Result<ProjectLayout> layoutResult = EnsureProjectLayout(root);
        if (!layoutResult)
        {
            result.summary = "Filesystem probe: " + layoutResult.GetError().message;
            return result;
        }
        result.layout = layoutResult.Value();

        const std::filesystem::path textPath = result.layout.assets / "probe" / "config.aktext";
        const std::string payload = "AKFS\nversion=1\n";
        Result<void> writeText = WriteTextFileAtomic(textPath, payload);
        Result<std::string> readText = ReadTextFile(textPath);

        const std::filesystem::path binaryPath = result.layout.cache / "payload.bin";
        const std::vector<u8> bytes = {0x41u, 0x4bu, 0x01u, 0x02u, 0x03u};
        Result<void> writeBinary = WriteBinaryFileAtomic(binaryPath, bytes);
        Result<std::vector<u8>> readBinary = ReadBinaryFile(binaryPath);

        result.pathRisk = AssessPathRisk(textPath / ".." / "config.aktext");
        result.scan = ScanDirectoryTree(root, {true, true, true, 64});
        result.fingerprint = BuildFileFingerprint(textPath, true);
        result.ok = static_cast<bool>(writeText)
            && static_cast<bool>(readText)
            && readText.Value() == payload
            && static_cast<bool>(writeBinary)
            && static_cast<bool>(readBinary)
            && readBinary.Value() == bytes
            && result.fingerprint.exists
            && result.fingerprint.contentHash != 0
            && result.scan.fileCount >= 2;

        result.summary = std::string("Filesystem probe: ") + (result.ok ? "ok " : "failed ")
            + result.layout.summary + " " + result.scan.summary;
        std::filesystem::remove_all(root, error);
        return result;
    }

    std::string BuildFilesystemProbeSummary()
    {
        return BuildFilesystemProbe().summary;
    }
}
