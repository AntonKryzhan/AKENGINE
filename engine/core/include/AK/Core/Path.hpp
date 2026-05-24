#pragma once

#include <AK/Core/Result.hpp>

#include <filesystem>
#include <string>

namespace AK
{
    std::filesystem::path NormalizePath(const std::filesystem::path& path);
    std::string NormalizePathString(const std::filesystem::path& path);
    std::filesystem::path MakeRelativePath(const std::filesystem::path& path, const std::filesystem::path& root);
    Result<void> AtomicWriteTextFile(const std::filesystem::path& path, std::string_view text);
}
