#pragma once

#include <AK/Core/Guid.hpp>
#include <AK/Core/Handle.hpp>
#include <AK/Core/Types.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class ResourceType
    {
        Unknown,
        Mesh,
        Texture,
        Material,
        Shader,
        GpuBuffer
    };

    struct MeshResourceTag;
    struct TextureResourceTag;
    struct MaterialResourceTag;
    struct ShaderResourceTag;
    struct GpuBufferResourceTag;

    using MeshHandle = Handle<MeshResourceTag>;
    using TextureHandle = Handle<TextureResourceTag>;
    using MaterialHandle = Handle<MaterialResourceTag>;
    using ShaderHandle = Handle<ShaderResourceTag>;
    using GpuBufferHandle = Handle<GpuBufferResourceTag>;

    struct ResourceDesc
    {
        ResourceType type = ResourceType::Unknown;
        AssetGuid assetGuid{};
        std::string debugName;
        u64 sizeBytes = 0;
    };

    struct ResourceRecord
    {
        ResourceHandle handle{};
        ResourceType type = ResourceType::Unknown;
        AssetGuid assetGuid{};
        std::string debugName;
        u64 sizeBytes = 0;
        u64 createdFrame = 0;
        u64 destroyRequestedFrame = 0;
        u64 releaseAfterFrame = 0;
        bool pendingDestroy = false;
    };

    struct ResourceManagerConfig
    {
        u64 deferredFrameLag = 3;
    };

    struct ResourceStats
    {
        u32 resident = 0;
        u32 alive = 0;
        u32 pendingDestroy = 0;
        u32 freeSlots = 0;
        u32 totalCreated = 0;
        u32 totalReleased = 0;
        u64 residentBytes = 0;
        u64 pendingBytes = 0;
        u64 peakResidentBytes = 0;
    };

    class ResourceManager final
    {
    public:
        explicit ResourceManager(ResourceManagerConfig config = {});

        ResourceHandle Create(const ResourceDesc& desc, u64 frameIndex);
        bool RequestDestroy(ResourceHandle handle, u64 frameIndex);
        u32 ProcessDeferredReleases(u64 frameIndex);
        void Clear();

        bool IsAlive(ResourceHandle handle) const;
        bool IsResident(ResourceHandle handle) const;
        const ResourceRecord* Get(ResourceHandle handle) const;
        ResourceStats Stats() const;

        ResourceManagerConfig Config() const;
        void SetConfig(ResourceManagerConfig config);

    private:
        struct Slot
        {
            ResourceRecord record{};
            u32 generation = 1;
            bool resident = false;
        };

        const Slot* FindSlot(ResourceHandle handle) const;
        Slot* FindSlot(ResourceHandle handle);
        void UpdatePeakResidentBytes();

        ResourceManagerConfig mConfig{};
        std::vector<Slot> mSlots;
        std::vector<u32> mFreeList;
        u32 mTotalCreated = 0;
        u32 mTotalReleased = 0;
        u64 mPeakResidentBytes = 0;
    };

    const char* ToString(ResourceType type);
    std::string ToDebugString(const ResourceStats& stats);
    std::string BuildResourceProbeSummary();
}
