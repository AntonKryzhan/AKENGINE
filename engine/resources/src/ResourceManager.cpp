#include <AK/Resources/ResourceManager.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr u32 InvalidSlotIndex = 0;

        u32 NextGeneration(u32 generation)
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }
    }

    ResourceManager::ResourceManager(ResourceManagerConfig config)
        : mConfig(config)
    {
        mSlots.push_back({});
        mSlots.front().generation = 0;
    }

    ResourceHandle ResourceManager::Create(const ResourceDesc& desc, u64 frameIndex)
    {
        u32 index = InvalidSlotIndex;
        if (!mFreeList.empty())
        {
            index = mFreeList.back();
            mFreeList.pop_back();
        }
        else
        {
            index = static_cast<u32>(mSlots.size());
            mSlots.push_back({});
            if (mSlots.back().generation == 0)
            {
                mSlots.back().generation = 1;
            }
        }

        Slot& slot = mSlots[index];
        slot.resident = true;

        ResourceHandle handle{};
        handle.index = index;
        handle.generation = slot.generation;

        ResourceRecord record{};
        record.handle = handle;
        record.type = desc.type;
        record.assetGuid = desc.assetGuid;
        record.debugName = desc.debugName;
        record.sizeBytes = desc.sizeBytes;
        record.createdFrame = frameIndex;
        record.pendingDestroy = false;

        slot.record = std::move(record);
        ++mTotalCreated;
        UpdatePeakResidentBytes();
        return handle;
    }

    bool ResourceManager::RequestDestroy(ResourceHandle handle, u64 frameIndex)
    {
        Slot* slot = FindSlot(handle);
        if (!slot || !slot->resident || slot->record.pendingDestroy)
        {
            return false;
        }

        slot->record.pendingDestroy = true;
        slot->record.destroyRequestedFrame = frameIndex;
        slot->record.releaseAfterFrame = frameIndex + mConfig.deferredFrameLag;
        return true;
    }

    u32 ResourceManager::ProcessDeferredReleases(u64 frameIndex)
    {
        u32 released = 0;
        for (u32 index = 1; index < mSlots.size(); ++index)
        {
            Slot& slot = mSlots[index];
            if (!slot.resident || !slot.record.pendingDestroy || slot.record.releaseAfterFrame > frameIndex)
            {
                continue;
            }

            slot.record = {};
            slot.resident = false;
            slot.generation = NextGeneration(slot.generation);
            mFreeList.push_back(index);
            ++mTotalReleased;
            ++released;
        }
        return released;
    }

    void ResourceManager::Clear()
    {
        mSlots.clear();
        mFreeList.clear();
        mSlots.push_back({});
        mSlots.front().generation = 0;
        mTotalCreated = 0;
        mTotalReleased = 0;
        mPeakResidentBytes = 0;
    }

    bool ResourceManager::IsAlive(ResourceHandle handle) const
    {
        const Slot* slot = FindSlot(handle);
        return slot && slot->resident && !slot->record.pendingDestroy;
    }

    bool ResourceManager::IsResident(ResourceHandle handle) const
    {
        const Slot* slot = FindSlot(handle);
        return slot && slot->resident;
    }

    const ResourceRecord* ResourceManager::Get(ResourceHandle handle) const
    {
        const Slot* slot = FindSlot(handle);
        if (!slot || !slot->resident)
        {
            return nullptr;
        }
        return &slot->record;
    }

    ResourceStats ResourceManager::Stats() const
    {
        ResourceStats stats{};
        stats.freeSlots = static_cast<u32>(mFreeList.size());
        stats.totalCreated = mTotalCreated;
        stats.totalReleased = mTotalReleased;
        stats.peakResidentBytes = mPeakResidentBytes;

        for (std::size_t i = 1; i < mSlots.size(); ++i)
        {
            const Slot& slot = mSlots[i];
            if (!slot.resident)
            {
                continue;
            }

            ++stats.resident;
            stats.residentBytes += slot.record.sizeBytes;
            if (slot.record.pendingDestroy)
            {
                ++stats.pendingDestroy;
                stats.pendingBytes += slot.record.sizeBytes;
            }
            else
            {
                ++stats.alive;
            }
        }

        return stats;
    }

    ResourceManagerConfig ResourceManager::Config() const
    {
        return mConfig;
    }

    void ResourceManager::SetConfig(ResourceManagerConfig config)
    {
        mConfig = config;
    }

    const ResourceManager::Slot* ResourceManager::FindSlot(ResourceHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= mSlots.size())
        {
            return nullptr;
        }

        const Slot& slot = mSlots[handle.index];
        if (slot.generation != handle.generation)
        {
            return nullptr;
        }
        return &slot;
    }

    ResourceManager::Slot* ResourceManager::FindSlot(ResourceHandle handle)
    {
        return const_cast<Slot*>(static_cast<const ResourceManager*>(this)->FindSlot(handle));
    }

    void ResourceManager::UpdatePeakResidentBytes()
    {
        mPeakResidentBytes = std::max(mPeakResidentBytes, Stats().residentBytes);
    }

    const char* ToString(ResourceType type)
    {
        switch (type)
        {
            case ResourceType::Mesh:
                return "mesh";
            case ResourceType::Texture:
                return "texture";
            case ResourceType::Material:
                return "material";
            case ResourceType::Shader:
                return "shader";
            case ResourceType::GpuBuffer:
                return "gpu-buffer";
            case ResourceType::Unknown:
            default:
                return "unknown";
        }
    }

    std::string ToDebugString(const ResourceStats& stats)
    {
        std::ostringstream out;
        out << "resident=" << stats.resident
            << " alive=" << stats.alive
            << " pending=" << stats.pendingDestroy
            << " bytes=" << stats.residentBytes
            << " peak=" << stats.peakResidentBytes
            << " created=" << stats.totalCreated
            << " released=" << stats.totalReleased;
        return out.str();
    }

    std::string BuildResourceProbeSummary()
    {
        ResourceManager manager({2});
        const ResourceHandle mesh = manager.Create({ResourceType::Mesh, BuildAssetGuidFromNormalizedPath("builtin:cube"), "builtin:cube", 65536}, 10);
        const ResourceHandle texture = manager.Create({ResourceType::Texture, BuildAssetGuidFromNormalizedPath("builtin:white"), "builtin:white", 4096}, 10);
        const bool beforeDestroy = manager.IsAlive(mesh) && manager.IsAlive(texture);
        const bool requestOk = manager.RequestDestroy(mesh, 11);
        const bool stillResident = manager.IsResident(mesh) && !manager.IsAlive(mesh);
        const u32 earlyRelease = manager.ProcessDeferredReleases(12);
        const u32 finalRelease = manager.ProcessDeferredReleases(13);
        const bool staleRejected = !manager.IsResident(mesh) && !manager.IsAlive(mesh);
        const ResourceHandle reused = manager.Create({ResourceType::Material, BuildAssetGuidFromNormalizedPath("builtin:default"), "builtin:default", 2048}, 14);
        const bool generationChanged = reused.index == mesh.index && reused.generation != mesh.generation;

        std::ostringstream out;
        out << "Resource probe: "
            << (beforeDestroy && requestOk && stillResident && earlyRelease == 0 && finalRelease == 1 && staleRejected && generationChanged ? "ok" : "failed")
            << " " << ToDebugString(manager.Stats());
        return out.str();
    }
}
