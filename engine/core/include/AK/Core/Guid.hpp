#pragma once

#include <AK/Core/Types.hpp>

#include <filesystem>
#include <string>

namespace AK
{
    struct AssetGuid final
    {
        u64 high = 0;
        u64 low = 0;

        constexpr bool IsValid() const
        {
            return high != 0 || low != 0;
        }
    };

    constexpr bool operator==(AssetGuid a, AssetGuid b)
    {
        return a.high == b.high && a.low == b.low;
    }

    constexpr bool operator!=(AssetGuid a, AssetGuid b)
    {
        return !(a == b);
    }

    std::string ToString(AssetGuid guid);
    AssetGuid AssetGuidFromString(std::string_view text);
    AssetGuid BuildAssetGuidFromNormalizedPath(std::string_view normalizedPath);
    AssetGuid BuildAssetGuidFromPath(const std::filesystem::path& path);
}

namespace std
{
    template <>
    struct hash<AK::AssetGuid>
    {
        std::size_t operator()(AK::AssetGuid value) const noexcept
        {
            const std::uint64_t mixed = value.high ^ (value.low + 0x9e3779b97f4a7c15ull + (value.high << 6u) + (value.high >> 2u));
            return std::hash<std::uint64_t>{}(mixed);
        }
    };
}
