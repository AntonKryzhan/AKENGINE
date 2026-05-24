#include <AK/Core/Path.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>

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
    }

    std::filesystem::path NormalizePath(const std::filesystem::path& path)
    {
        return path.lexically_normal();
    }

    std::string NormalizePathString(const std::filesystem::path& path)
    {
        return ToLowerAscii(NormalizePath(path).generic_string());
    }

    std::filesystem::path MakeRelativePath(const std::filesystem::path& path, const std::filesystem::path& root)
    {
        std::error_code error;
        std::filesystem::path relative = std::filesystem::relative(path, root, error);
        if (error)
        {
            return NormalizePath(path);
        }
        return NormalizePath(relative);
    }

    Result<void> AtomicWriteTextFile(const std::filesystem::path& path, std::string_view text)
    {
        std::error_code error;
        const std::filesystem::path normalizedPath = NormalizePath(path);
        const std::filesystem::path parent = normalizedPath.parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent, error);
            if (error)
            {
                return MakeError(ErrorCode::IoError, "failed to create directory: " + parent.string());
            }
        }

        const std::filesystem::path tempPath = normalizedPath.string() + ".tmp";
        {
            std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                return MakeError(ErrorCode::IoError, "failed to open temp file for write: " + tempPath.string());
            }

            file.write(text.data(), static_cast<std::streamsize>(text.size()));
            file.flush();
            if (!file)
            {
                return MakeError(ErrorCode::IoError, "failed to write temp file: " + tempPath.string());
            }
        }

        std::filesystem::remove(normalizedPath, error);
        error.clear();
        std::filesystem::rename(tempPath, normalizedPath, error);
        if (error)
        {
            std::filesystem::remove(tempPath, error);
            return MakeError(ErrorCode::IoError, "failed to replace file atomically: " + normalizedPath.string());
        }

        return Ok();
    }
}
