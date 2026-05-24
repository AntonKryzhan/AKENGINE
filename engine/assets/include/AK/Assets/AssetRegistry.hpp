#pragma once

#include <AK/Core/Guid.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace AK
{
    struct AssetRecord
    {
        AssetGuid guid{};
        std::filesystem::path path;
        std::string type;
    };

    class AssetRegistry final
    {
    public:
        void Clear();
        bool ScanDirectory(const std::filesystem::path& root);
        const std::vector<AssetRecord>& Assets() const;

        const AssetRecord* Find(AssetGuid guid) const;
        static AssetGuid BuildGuidFromPath(const std::filesystem::path& path);

    private:
        std::vector<AssetRecord> mAssets;
        std::unordered_map<AssetGuid, std::size_t> mGuidToIndex;
    };
}
