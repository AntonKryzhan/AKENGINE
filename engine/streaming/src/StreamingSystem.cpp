#include <AK/Streaming/StreamingSystem.hpp>

#include <AK/Assets/AssetDatabase.hpp>
#include <AK/Filesystem/FileSystem.hpp>
#include <AK/Package/AssetPackage.hpp>

#include <algorithm>
#include <sstream>
#include <system_error>

namespace AK
{
    namespace
    {

        bool PriorityGreater(const StreamingRequest& a, const StreamingRequest& b)
        {
            if (a.priority != b.priority)
            {
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            }
            return a.requestId < b.requestId;
        }

        std::string RequestName(const StreamingRequestDesc& desc)
        {
            if (!desc.debugName.empty())
            {
                return desc.debugName;
            }
            if (!desc.logicalPath.empty())
            {
                return desc.logicalPath.generic_string();
            }
            return ToString(desc.guid);
        }
    }

    StreamingSystem::StreamingSystem(const VirtualFileSystem* vfs, ResourceManager* resources)
        : mVfs(vfs)
        , mResources(resources)
    {
    }

    void StreamingSystem::AttachVirtualFileSystem(const VirtualFileSystem* vfs)
    {
        mVfs = vfs;
    }

    void StreamingSystem::AttachResourceManager(ResourceManager* resources)
    {
        mResources = resources;
    }

    void StreamingSystem::Clear()
    {
        mRequests.clear();
        mNextRequestId = 1;
        mTotalLoaded = 0;
        mTotalFailed = 0;
        mTotalUnloaded = 0;
    }

    u64 StreamingSystem::RequestLoad(const StreamingRequestDesc& desc, u64 frameIndex)
    {
        StreamingRequest request{};
        request.requestId = mNextRequestId++;
        request.kind = desc.logicalPath.empty() ? StreamingRequestKind::LoadByGuid : StreamingRequestKind::LoadByLogicalPath;
        request.status = StreamingRequestStatus::Queued;
        request.priority = desc.priority;
        request.guid = desc.guid;
        request.logicalPath = desc.logicalPath;
        request.resourceType = desc.resourceType;
        request.debugName = RequestName(desc);
        request.requestedFrame = frameIndex;
        RefreshSummary(request);
        mRequests.push_back(std::move(request));
        return mRequests.back().requestId;
    }

    u64 StreamingSystem::RequestLoad(AssetGuid guid, ResourceType type, StreamingPriority priority, std::string_view debugName, u64 frameIndex)
    {
        StreamingRequestDesc desc{};
        desc.guid = guid;
        desc.resourceType = type;
        desc.priority = priority;
        desc.debugName = std::string(debugName);
        return RequestLoad(desc, frameIndex);
    }

    u64 StreamingSystem::RequestLoadByLogicalPath(const std::filesystem::path& logicalPath, ResourceType type, StreamingPriority priority, std::string_view debugName, u64 frameIndex)
    {
        StreamingRequestDesc desc{};
        desc.logicalPath = logicalPath;
        desc.resourceType = type;
        desc.priority = priority;
        desc.debugName = std::string(debugName);
        return RequestLoad(desc, frameIndex);
    }

    bool StreamingSystem::RequestUnload(ResourceHandle handle, u64 frameIndex)
    {
        if (!handle.IsValid())
        {
            return false;
        }

        StreamingRequest request{};
        request.requestId = mNextRequestId++;
        request.kind = StreamingRequestKind::UnloadResource;
        request.status = StreamingRequestStatus::UnloadQueued;
        request.priority = StreamingPriority::Normal;
        request.resource = handle;
        request.requestedFrame = frameIndex;
        request.debugName = "resource-unload";
        RefreshSummary(request);
        mRequests.push_back(std::move(request));
        return true;
    }

    bool StreamingSystem::Cancel(u64 requestId)
    {
        StreamingRequest* request = FindRequest(requestId);
        if (!request || request->status != StreamingRequestStatus::Queued)
        {
            return false;
        }
        request->status = StreamingRequestStatus::Cancelled;
        request->error.clear();
        RefreshSummary(*request);
        return true;
    }

    StreamingUpdateResult StreamingSystem::Update(const StreamingBudget& budget, u64 frameIndex)
    {
        StreamingUpdateResult result{};

        std::vector<StreamingRequest*> unloads;
        std::vector<StreamingRequest*> loads;
        for (StreamingRequest& request : mRequests)
        {
            if (request.status == StreamingRequestStatus::UnloadQueued)
            {
                unloads.push_back(&request);
            }
            else if (request.status == StreamingRequestStatus::Queued)
            {
                loads.push_back(&request);
            }
        }

        for (StreamingRequest* request : unloads)
        {
            if (!mResources)
            {
                request->status = StreamingRequestStatus::Failed;
                request->error = "ResourceManager is not attached";
                ++mTotalFailed;
            }
            else if (mResources->RequestDestroy(request->resource, frameIndex))
            {
                request->status = StreamingRequestStatus::Unloaded;
                request->completedFrame = frameIndex;
                ++mTotalUnloaded;
                result.completedRequestIds.push_back(request->requestId);
                result.events.push_back("unload queued: " + ToDebugString(*request));
                result.madeProgress = true;
            }
            else
            {
                request->status = StreamingRequestStatus::Failed;
                request->error = "resource handle is stale or already pending destroy";
                ++mTotalFailed;
            }
            RefreshSummary(*request);
        }

        std::sort(loads.begin(), loads.end(), [](const StreamingRequest* a, const StreamingRequest* b)
        {
            return PriorityGreater(*a, *b);
        });

        u32 loadsThisUpdate = 0;
        u64 bytesLoadedThisUpdate = 0;
        for (StreamingRequest* request : loads)
        {
            if (loadsThisUpdate >= budget.maxLoadsPerUpdate)
            {
                ++result.stats.budgetBlocked;
                continue;
            }

            const VirtualFileEntry* entry = ResolveEntry(*request);
            if (!entry)
            {
                request->status = StreamingRequestStatus::Failed;
                request->completedFrame = frameIndex;
                request->error = request->logicalPath.empty()
                    ? "asset GUID is not mounted in VFS"
                    : "logical path is not mounted in VFS";
                ++mTotalFailed;
                result.completedRequestIds.push_back(request->requestId);
                result.events.push_back("load failed: " + ToDebugString(*request));
                RefreshSummary(*request);
                result.madeProgress = true;
                continue;
            }

            const bool criticalOverBudget = budget.allowOverBudgetCritical && request->priority == StreamingPriority::Critical;
            if (!criticalOverBudget && bytesLoadedThisUpdate + entry->payloadSize > budget.maxBytesPerUpdate)
            {
                ++result.stats.budgetBlocked;
                continue;
            }

            request->status = StreamingRequestStatus::Loading;
            RefreshSummary(*request);
            Result<VirtualReadResult> readResult = ReadPayload(*request);
            if (!readResult || !readResult.Value().ok)
            {
                request->status = StreamingRequestStatus::Failed;
                request->completedFrame = frameIndex;
                request->error = readResult ? readResult.Value().summary : readResult.GetError().message;
                ++mTotalFailed;
                result.completedRequestIds.push_back(request->requestId);
                result.events.push_back("load failed: " + ToDebugString(*request));
                RefreshSummary(*request);
                result.madeProgress = true;
                continue;
            }

            const VirtualReadResult& read = readResult.Value();
            request->guid = read.guid;
            request->logicalPath = read.entry.logicalPath;
            request->payloadBytes = static_cast<u64>(read.bytes.size());
            request->resourceType = ResolveResourceType(request->resourceType, read.entry.kind);
            if (mResources)
            {
                request->resource = mResources->Create({request->resourceType, request->guid, request->debugName, request->payloadBytes}, frameIndex);
            }
            request->status = StreamingRequestStatus::Resident;
            request->completedFrame = frameIndex;
            ++mTotalLoaded;
            ++loadsThisUpdate;
            bytesLoadedThisUpdate += request->payloadBytes;
            result.completedRequestIds.push_back(request->requestId);
            result.events.push_back("loaded: " + ToDebugString(*request));
            RefreshSummary(*request);
            result.madeProgress = true;
        }

        result.stats = Stats();
        result.stats.loadsThisUpdate = loadsThisUpdate;
        result.stats.bytesLoadedThisUpdate = bytesLoadedThisUpdate;
        result.summary = ToDebugString(result);
        return result;
    }

    const std::vector<StreamingRequest>& StreamingSystem::Requests() const
    {
        return mRequests;
    }

    StreamingStats StreamingSystem::Stats() const
    {
        StreamingStats stats{};
        stats.totalRequests = static_cast<u32>(mRequests.size());
        stats.totalLoaded = mTotalLoaded;
        stats.totalFailed = mTotalFailed;
        stats.totalUnloaded = mTotalUnloaded;

        for (const StreamingRequest& request : mRequests)
        {
            switch (request.status)
            {
                case StreamingRequestStatus::Queued: ++stats.queued; break;
                case StreamingRequestStatus::Loading: ++stats.loading; break;
                case StreamingRequestStatus::Resident:
                    ++stats.resident;
                    stats.residentBytes += request.payloadBytes;
                    break;
                case StreamingRequestStatus::Failed: ++stats.failed; break;
                case StreamingRequestStatus::Cancelled: ++stats.cancelled; break;
                case StreamingRequestStatus::UnloadQueued: ++stats.unloadQueued; break;
                case StreamingRequestStatus::Unloaded: ++stats.unloaded; break;
            }
        }
        stats.summary = ToDebugString(stats);
        return stats;
    }

    StreamingRequest* StreamingSystem::FindRequest(u64 requestId)
    {
        const auto it = std::find_if(mRequests.begin(), mRequests.end(), [requestId](const StreamingRequest& request)
        {
            return request.requestId == requestId;
        });
        return it == mRequests.end() ? nullptr : &(*it);
    }

    const StreamingRequest* StreamingSystem::FindRequest(u64 requestId) const
    {
        return const_cast<StreamingSystem*>(this)->FindRequest(requestId);
    }

    const VirtualFileEntry* StreamingSystem::ResolveEntry(const StreamingRequest& request) const
    {
        if (!mVfs)
        {
            return nullptr;
        }
        if (request.kind == StreamingRequestKind::LoadByLogicalPath)
        {
            return mVfs->FindByLogicalPath(request.logicalPath);
        }
        return mVfs->Find(request.guid);
    }

    Result<VirtualReadResult> StreamingSystem::ReadPayload(const StreamingRequest& request) const
    {
        if (!mVfs)
        {
            return MakeError(ErrorCode::InvalidState, "VirtualFileSystem is not attached");
        }

        VirtualReadOptions options{};
        options.validateHash = true;
        if (request.kind == StreamingRequestKind::LoadByLogicalPath)
        {
            return mVfs->ReadByLogicalPath(request.logicalPath, options);
        }
        return mVfs->Read(request.guid, options);
    }

    ResourceType StreamingSystem::ResolveResourceType(ResourceType requested, AssetKind kind) const
    {
        if (requested != ResourceType::Unknown)
        {
            return requested;
        }

        switch (kind)
        {
            case AssetKind::Mesh: return ResourceType::Mesh;
            case AssetKind::Texture: return ResourceType::Texture;
            case AssetKind::Material: return ResourceType::Material;
            case AssetKind::Shader: return ResourceType::Shader;
            default: return ResourceType::Unknown;
        }
    }

    void StreamingSystem::RefreshSummary(StreamingRequest& request) const
    {
        request.summary = ToDebugString(request);
    }

    std::string_view ToString(StreamingPriority priority)
    {
        switch (priority)
        {
            case StreamingPriority::Low: return "low";
            case StreamingPriority::Normal: return "normal";
            case StreamingPriority::High: return "high";
            case StreamingPriority::Critical: return "critical";
        }
        return "unknown";
    }

    std::string_view ToString(StreamingRequestKind kind)
    {
        switch (kind)
        {
            case StreamingRequestKind::LoadByGuid: return "load-guid";
            case StreamingRequestKind::LoadByLogicalPath: return "load-path";
            case StreamingRequestKind::UnloadResource: return "unload";
        }
        return "unknown";
    }

    std::string_view ToString(StreamingRequestStatus status)
    {
        switch (status)
        {
            case StreamingRequestStatus::Queued: return "queued";
            case StreamingRequestStatus::Loading: return "loading";
            case StreamingRequestStatus::Resident: return "resident";
            case StreamingRequestStatus::Failed: return "failed";
            case StreamingRequestStatus::Cancelled: return "cancelled";
            case StreamingRequestStatus::UnloadQueued: return "unload-queued";
            case StreamingRequestStatus::Unloaded: return "unloaded";
        }
        return "unknown";
    }

    std::string ToDebugString(const StreamingRequest& request)
    {
        std::ostringstream out;
        out << "stream#" << request.requestId
            << " " << ToString(request.kind)
            << " " << ToString(request.status)
            << " priority=" << ToString(request.priority)
            << " type=" << ToString(request.resourceType)
            << " bytes=" << request.payloadBytes
            << " name=" << request.debugName;
        if (request.guid.IsValid())
        {
            out << " guid=" << ToString(request.guid);
        }
        if (!request.logicalPath.empty())
        {
            out << " path=" << request.logicalPath.generic_string();
        }
        if (request.resource.IsValid())
        {
            out << " handle=" << request.resource.index << ":" << request.resource.generation;
        }
        if (!request.error.empty())
        {
            out << " error=" << request.error;
        }
        return out.str();
    }

    std::string ToDebugString(const StreamingStats& stats)
    {
        std::ostringstream out;
        out << "streaming queued=" << stats.queued
            << " resident=" << stats.resident
            << " failed=" << stats.failed
            << " unloaded=" << stats.unloaded
            << " bytes=" << stats.residentBytes
            << " loaded=" << stats.totalLoaded
            << " loads/update=" << stats.loadsThisUpdate
            << " bytes/update=" << stats.bytesLoadedThisUpdate
            << " blocked=" << stats.budgetBlocked;
        return out.str();
    }

    std::string ToDebugString(const StreamingUpdateResult& result)
    {
        std::ostringstream out;
        out << ToDebugString(result.stats)
            << " completed=" << result.completedRequestIds.size()
            << " events=" << result.events.size()
            << " progress=" << (result.madeProgress ? "yes" : "no");
        return out.str();
    }

    StreamingProbeResult BuildStreamingProbe()
    {
        StreamingProbeResult result{};
        std::error_code error;
        const std::filesystem::path root = std::filesystem::temp_directory_path(error) / "akengine_streaming_probe";
        if (error)
        {
            result.summary = "Streaming probe: failed to resolve temp directory";
            return result;
        }

        std::filesystem::remove_all(root, error);
        error.clear();

        Result<ProjectLayout> layoutResult = EnsureProjectLayout(root);
        if (!layoutResult)
        {
            result.summary = "Streaming probe: " + layoutResult.GetError().message;
            return result;
        }
        result.layout = layoutResult.Value();

        const Result<void> writeMesh = WriteBinaryFileAtomic(result.layout.assets / "meshes" / "probe.glb", std::vector<u8>{1, 2, 3, 4, 5, 6});
        const Result<void> writeTexture = WriteBinaryFileAtomic(result.layout.assets / "textures" / "probe.png", std::vector<u8>{7, 8, 9, 10});
        const Result<void> writeMaterial = WriteTextFileAtomic(result.layout.assets / "materials" / "probe.akmat", "AKMAT 1\nname probe\n");

        AssetImportPolicy policy{};
        policy.hashSourceContents = true;
        policy.includeUnknown = false;
        result.manifest = BuildAssetManifest(result.layout, policy);

        Result<PackageBuildResult> packageResult = WriteAssetPackage(BuildDefaultPackagePath(result.layout, "streaming"), result.layout, result.manifest);
        if (packageResult)
        {
            result.package = packageResult.Value();
        }

        VirtualFileSystem vfs;
        Result<void> mountPackage = packageResult ? vfs.MountPackage("streaming-pak", result.package.packagePath) : Result<void>(MakeError(ErrorCode::InvalidState, "package not built"));
        result.vfsStats = vfs.Stats();

        ResourceManager resources({2});
        StreamingSystem streaming(&vfs, &resources);
        for (const AssetManifestRecord& record : result.manifest.records)
        {
            if (record.kind == AssetKind::Mesh || record.kind == AssetKind::Texture || record.kind == AssetKind::Material)
            {
                const u64 requestId = streaming.RequestLoad(record.guid, ResourceType::Unknown, record.kind == AssetKind::Mesh ? StreamingPriority::High : StreamingPriority::Normal, record.sourcePath.generic_string(), 1);
                (void)requestId;
            }
        }

        StreamingBudget budget{};
        budget.maxLoadsPerUpdate = 2;
        budget.maxBytesPerUpdate = 1024;
        budget.maxResidentBytes = 1024;
        result.firstUpdate = streaming.Update(budget, 2);
        result.secondUpdate = streaming.Update(budget, 3);
        result.resourceStats = resources.Stats();

        result.ok = static_cast<bool>(writeMesh)
            && static_cast<bool>(writeTexture)
            && static_cast<bool>(writeMaterial)
            && static_cast<bool>(packageResult)
            && static_cast<bool>(mountPackage)
            && result.manifest.records.size() == 3
            && result.vfsStats.packageEntries == 3
            && result.firstUpdate.stats.totalLoaded == 2
            && result.secondUpdate.stats.totalLoaded == 3
            && result.resourceStats.resident == 3
            && result.resourceStats.residentBytes > 0;

        result.summary = std::string("Streaming probe: ") + (result.ok ? "ok " : "failed ")
            + result.secondUpdate.stats.summary
            + " resources=" + ToDebugString(result.resourceStats);

        std::filesystem::remove_all(root, error);
        return result;
    }

    std::string BuildStreamingProbeSummary()
    {
        return BuildStreamingProbe().summary;
    }
}
