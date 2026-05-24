#pragma once

#include <AK/Core/Types.hpp>

#include <functional>
#include <string>

namespace AK
{
    template <typename Tag>
    struct Handle final
    {
        u32 index = 0;
        u32 generation = 0;

        constexpr bool IsValid() const
        {
            return index != 0 && generation != 0;
        }
    };

    template <typename Tag>
    constexpr bool operator==(Handle<Tag> a, Handle<Tag> b)
    {
        return a.index == b.index && a.generation == b.generation;
    }

    template <typename Tag>
    constexpr bool operator!=(Handle<Tag> a, Handle<Tag> b)
    {
        return !(a == b);
    }

    template <typename Tag>
    std::string ToString(Handle<Tag> handle)
    {
        return std::to_string(handle.index) + ":" + std::to_string(handle.generation);
    }

    struct ResourceHandleTag;
    using ResourceHandle = Handle<ResourceHandleTag>;
}

namespace std
{
    template <typename Tag>
    struct hash<AK::Handle<Tag>>
    {
        std::size_t operator()(AK::Handle<Tag> value) const noexcept
        {
            const std::uint64_t packed = (static_cast<std::uint64_t>(value.index) << 32u) | static_cast<std::uint64_t>(value.generation);
            return std::hash<std::uint64_t>{}(packed);
        }
    };
}
