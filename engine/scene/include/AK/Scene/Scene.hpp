#pragma once

#include <AK/ECS/World.hpp>

#include <filesystem>
#include <string>

namespace AK
{
    class Scene final
    {
    public:
        explicit Scene(std::string name = "Untitled");

        const std::string& Name() const;
        World& GetWorld();
        const World& GetWorld() const;

        bool SaveToFile(const std::filesystem::path& path) const;
        bool LoadFromFile(const std::filesystem::path& path);

    private:
        std::string mName;
        World mWorld;
    };
}
