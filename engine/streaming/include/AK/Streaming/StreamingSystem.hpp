#pragma once

#include <AK/Core/Guid.hpp>
#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>
#include <AK/Resources/ResourceManager.hpp>
#include <AK/VFS/VirtualFileSystem.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class StreamingPriority
    {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    enum class StreamingRequestKind
    {
        LoadByGuid,
        LoadByLogicalPath,
        UnloadResource
    };

    enum class StreamingRequestStatus
    {
        Queued,
        Loading,
        Resident,
        Failed,
        Cancelled,
        UnloadQueued,
        Unloaded
    };

    struct StreamingBudget final
    {
        u32 maxLoadsPerUpdate = 4;
        u64 maxBytesPerUpdate = 16ull * 1024ull * 1024ull;
        u64 maxResidentBytes = 512ull * 1024ull * 1024ull;
        bool allowOverBudgetCritical = true;
    };

    struct StreamingRequestDesc final
    {
        AssetGuid guid{};
        std::filesystem::path logicalPath;
        ResourceType resourceType = ResourceType::Unknown;
        StreamingPriority priority = StreamingPriority::Normal;
        std::string debugName;
        bool validateHash = true;
    };

    struct StreamingRequest final
    {
        u64 requestId = 0;
        StreamingRequestKind kind = StreamingRequestKind::LoadByGuid;
        StreamingRequestStatus status = StreamingRequestStatus::Queued;
        StreamingPriority priority = StreamingPriority::Normal;
        AssetGuid guid{};
        std::filesystem::path logicalPath;
        ResourceType resourceType = ResourceType::Unknown;
        std::string debugName;
        ResourceHandle resource{};
        u64 requestedFrame = 0;
        u64 completedFrame = 0;
        u64 payloadBytes = 0;
        std::string error;
        std::string summary;
    };

    struct StreamingStats final
    {
        u32 queued = 0;
        u32 loading = 0;
        u32 resident = 0;
        u32 failed = 0;
        u32 cancelled = 0;
        u32 unloadQueued = 0;
        u32 unloaded = 0;
        u32 totalRequests = 0;
        u32 totalLoaded = 0;
        u32 totalFailed = 0;
        u32 totalUnloaded = 0;
        u64 residentBytes = 0;
        u64 bytesLoadedThisUpdate = 0;
        u32 loadsThisUpdate = 0;
        u32 budgetBlocked = 0;
        std::string summary;
    };

    struct StreamingUpdateResult final
    {
        StreamingStats stats{};
        std::vector<u64> completedRequestIds;
        std::vector<std::string> events;
        bool madeProgress = false;
        std::string summary;
    };

    struct StreamingProbeResult final
    {
        bool ok = false;
        ProjectLayout layout{};
        AssetManifest manifest{};
        PackageBuildResult package{};
        VirtualFileSystemStats vfsStats{};
        StreamingUpdateResult firstUpdate{};
        StreamingUpdateResult secondUpdate{};
        ResourceStats resourceStats{};
        std::string summary;
    };

    class StreamingSystem final
    {
    public:
        StreamingSystem() = default;
        StreamingSystem(const VirtualFileSystem* vfs, ResourceManager* resources);

        void AttachVirtualFileSystem(const VirtualFileSystem* vfs);
        void AttachResourceManager(ResourceManager* resources);
        void Clear();

        [[nodiscard]] u64 RequestLoad(const StreamingRequestDesc& desc, u64 frameIndex);
        [[nodiscard]] u64 RequestLoad(AssetGuid guid, ResourceType type, StreamingPriority priority, std::string_view debugName, u64 frameIndex);
        [[nodiscard]] u64 RequestLoadByLogicalPath(const std::filesystem::path& logicalPath, ResourceType type, StreamingPriority priority, std::string_view debugName, u64 frameIndex);
        [[nodiscard]] bool RequestUnload(ResourceHandle handle, u64 frameIndex);
        [[nodiscard]] bool Cancel(u64 requestId);

        StreamingUpdateResult Update(const StreamingBudget& budget, u64 frameIndex);
        [[nodiscard]] const std::vector<StreamingRequest>& Requests() const;
        [[nodiscard]] StreamingStats Stats() const;

    private:
        StreamingRequest* FindRequest(u64 requestId);
        const StreamingRequest* FindRequest(u64 requestId) const;
        [[nodiscard]] const VirtualFileEntry* ResolveEntry(const StreamingRequest& request) const;
        [[nodiscard]] Result<VirtualReadResult> ReadPayload(const StreamingRequest& request) const;
        [[nodiscard]] ResourceType ResolveResourceType(ResourceType requested, AssetKind kind) const;
        void RefreshSummary(StreamingRequest& request) const;

        const VirtualFileSystem* mVfs = nullptr;
        ResourceManager* mResources = nullptr;
        std::vector<StreamingRequest> mRequests;
        u64 mNextRequestId = 1;
        u32 mTotalLoaded = 0;
        u32 mTotalFailed = 0;
        u32 mTotalUnloaded = 0;
    };

    std::string_view ToString(StreamingPriority priority);
    std::string_view ToString(StreamingRequestKind kind);
    std::string_view ToString(StreamingRequestStatus status);
    std::string ToDebugString(const StreamingRequest& request);
    std::string ToDebugString(const StreamingStats& stats);
    std::string ToDebugString(const StreamingUpdateResult& result);

    StreamingProbeResult BuildStreamingProbe();
    std::string BuildStreamingProbeSummary();
}
