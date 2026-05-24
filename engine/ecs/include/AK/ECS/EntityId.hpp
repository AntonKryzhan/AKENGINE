#pragma once

#include <AK/Core/Types.hpp>

#include <functional>
#include <string>

namespace AK
{
    struct EntityId final
    {
        u32 index = 0;
        u32 generation = 0;

        constexpr bool IsValid() const
        {
            return index != 0 && generation != 0;
        }
    };

    constexpr EntityId InvalidEntity{};

    constexpr bool operator==(EntityId a, EntityId b)
    {
        return a.index == b.index && a.generation == b.generation;
    }

    constexpr bool operator!=(EntityId a, EntityId b)
    {
        return !(a == b);
    }

    std::string ToString(EntityId entity);
}

namespace std
{
    template <>
    struct hash<AK::EntityId>
    {
        std::size_t operator()(AK::EntityId value) const noexcept
        {
            const std::uint64_t packed = (static_cast<std::uint64_t>(value.index) << 32u) | static_cast<std::uint64_t>(value.generation);
            return std::hash<std::uint64_t>{}(packed);
        }
    };
}
