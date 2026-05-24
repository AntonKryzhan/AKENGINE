#include <AK/Assets/AssetRegistry.hpp>

#include <AK/Assets/AssetDatabase.hpp>
#include <AK/Core/Log.hpp>
#include <AK/Core/Path.hpp>

namespace AK
{
    void AssetRegistry::Clear()
    {
        mAssets.clear();
        mGuidToIndex.clear();
    }

    bool AssetRegistry::ScanDirectory(const std::filesystem::path& root)
    {
        Clear();

        if (!std::filesystem::exists(root))
        {
            LogWarning("Asset directory does not exist: " + root.string());
            return false;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            AssetRecord record{};
            record.path = MakeRelativePath(entry.path(), root);
            record.guid = BuildGuidFromPath(record.path);
            record.type = std::string(ToString(DetectAssetKind(record.path)));

            mGuidToIndex[record.guid] = mAssets.size();
            mAssets.push_back(std::move(record));
        }

        return true;
    }

    const std::vector<AssetRecord>& AssetRegistry::Assets() const
    {
        return mAssets;
    }

    const AssetRecord* AssetRegistry::Find(AssetGuid guid) const
    {
        const auto it = mGuidToIndex.find(guid);
        if (it == mGuidToIndex.end() || it->second >= mAssets.size())
        {
            return nullptr;
        }
        return &mAssets[it->second];
    }

    AssetGuid AssetRegistry::BuildGuidFromPath(const std::filesystem::path& path)
    {
        return BuildAssetGuidFromPath(path);
    }
}
