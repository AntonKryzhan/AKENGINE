#include <AK/ECS/EntityId.hpp>

namespace AK
{
    std::string ToString(EntityId entity)
    {
        if (!entity.IsValid())
        {
            return "invalid";
        }
        return std::to_string(entity.index) + ":" + std::to_string(entity.generation);
    }
}
