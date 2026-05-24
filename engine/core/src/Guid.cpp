#include <AK/Core/Guid.hpp>

#include <AK/Core/Path.hpp>

#include <charconv>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        u64 Fnv1a64(std::string_view text, u64 seed)
        {
            u64 hash = seed;
            for (const char c : text)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        bool ParseHex64(std::string_view text, u64& value)
        {
            value = 0;
            if (text.empty())
            {
                return false;
            }

            const char* begin = text.data();
            const char* end = begin + text.size();
            const auto result = std::from_chars(begin, end, value, 16);
            return result.ec == std::errc{} && result.ptr == end;
        }
    }

    std::string ToString(AssetGuid guid)
    {
        std::ostringstream out;
        out << std::hex << std::setfill('0')
            << std::setw(16) << guid.high
            << std::setw(16) << guid.low;
        return out.str();
    }

    AssetGuid AssetGuidFromString(std::string_view text)
    {
        if (text.size() != 32)
        {
            return {};
        }

        AssetGuid guid{};
        if (!ParseHex64(text.substr(0, 16), guid.high) || !ParseHex64(text.substr(16, 16), guid.low))
        {
            return {};
        }
        return guid;
    }

    AssetGuid BuildAssetGuidFromNormalizedPath(std::string_view normalizedPath)
    {
        return {
            Fnv1a64(normalizedPath, 14695981039346656037ull),
            Fnv1a64(normalizedPath, 1099511628211ull ^ 0x9e3779b97f4a7c15ull)
        };
    }

    AssetGuid BuildAssetGuidFromPath(const std::filesystem::path& path)
    {
        return BuildAssetGuidFromNormalizedPath(NormalizePathString(path));
    }
}
