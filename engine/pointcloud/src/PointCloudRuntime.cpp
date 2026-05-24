#include <AK/PointCloud/PointCloudRuntime.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr u64 FnvOffset = 14695981039346656037ull;
        constexpr u64 FnvPrime = 1099511628211ull;

        u64 HashCombine(u64 hash, u64 value)
        {
            for (u32 i = 0; i < 8; ++i)
            {
                hash ^= (value >> (i * 8u)) & 0xffull;
                hash *= FnvPrime;
            }
            return hash;
        }

        u32 StreamIndex(PointCloudCookStream stream)
        {
            return static_cast<u32>(stream);
        }

        float RuntimeDistance(Vec3 a, Vec3 b)
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        float RuntimePriority(float distance, u32 lod, bool prefetch)
        {
            const float lodBias = 1.0f + static_cast<float>(lod) * 0.35f;
            const float prefetchPenalty = prefetch ? 0.25f : 0.0f;
            return 1.0f / (1.0f + distance * 0.001f * lodBias) - prefetchPenalty;
        }

        bool PageIdLess(PointCloudPageId a, PointCloudPageId b)
        {
            if (a.lod != b.lod)
            {
                return a.lod < b.lod;
            }
            if (a.ordinal != b.ordinal)
            {
                return a.ordinal < b.ordinal;
            }
            if (a.x != b.x)
            {
                return a.x < b.x;
            }
            if (a.y != b.y)
            {
                return a.y < b.y;
            }
            return a.z < b.z;
        }

        void PushCommand(std::vector<PointCloudRuntimeCommand>& commands,
            PointCloudRuntimeCommandType type,
            const PointCloudRuntimePage& page,
            u64 bytes,
            std::string debugName)
        {
            PointCloudRuntimeCommand command{};
            command.type = type;
            command.page = page.cooked.page;
            command.lod = page.cooked.lod;
            command.bytes = bytes;
            command.attributeMask = page.requestedAttributeMask;
            command.gpuBufferToken = page.gpuBufferToken;
            command.debugName = std::move(debugName);
            commands.push_back(command);
        }

        void UpdateSummary(PointCloudRuntimePlan& plan)
        {
            plan.stats.summary = ToDebugString(plan.stats);
            plan.summary = ToDebugString(plan);
        }
    }

    const char* ToString(PointCloudRuntimePageState state)
    {
        switch (state)
        {
            case PointCloudRuntimePageState::Unloaded:
                return "unloaded";
            case PointCloudRuntimePageState::Requested:
                return "requested";
            case PointCloudRuntimePageState::PayloadResident:
                return "payload-resident";
            case PointCloudRuntimePageState::GpuResident:
                return "gpu-resident";
            case PointCloudRuntimePageState::Evictable:
                return "evictable";
            case PointCloudRuntimePageState::Failed:
                return "failed";
            default:
                return "unknown";
        }
    }

    const char* ToString(PointCloudRuntimeCommandType type)
    {
        switch (type)
        {
            case PointCloudRuntimeCommandType::RequestPage:
                return "request-page";
            case PointCloudRuntimeCommandType::ReadPayload:
                return "read-payload";
            case PointCloudRuntimeCommandType::UploadGpuBuffer:
                return "upload-gpu-buffer";
            case PointCloudRuntimeCommandType::BindAttributeStreams:
                return "bind-attribute-streams";
            case PointCloudRuntimeCommandType::DrawPage:
                return "draw-page";
            case PointCloudRuntimeCommandType::EvictPage:
                return "evict-page";
            default:
                return "unknown";
        }
    }

    u32 PointCloudRuntimeStreamCount()
    {
        return 4;
    }

    u64 PointCloudRuntimeStreamMaskForAttributes(const PointCloudAttributeLayout& layout, u64 attributeMask)
    {
        u64 streamMask = 0;
        for (const PointCloudAttributeLayoutElement& element : layout.elements)
        {
            if ((attributeMask & AttributeBit(element.semantic)) != 0)
            {
                streamMask |= (1ull << StreamIndex(element.stream));
            }
        }
        return streamMask;
    }

    u32 PointCloudRuntimeBytesPerPoint(const PointCloudAttributeLayout& layout, u64 attributeMask)
    {
        u32 bytes = 0;
        for (const PointCloudAttributeLayoutElement& element : layout.elements)
        {
            if ((attributeMask & AttributeBit(element.semantic)) != 0)
            {
                bytes += element.byteSize;
            }
        }
        return bytes;
    }

    u64 PointCloudRuntimeEstimatePageBytes(const PointCloudCookPageRecord& page, const PointCloudAttributeLayout& layout, u64 attributeMask)
    {
        const u32 selectedBytesPerPoint = PointCloudRuntimeBytesPerPoint(layout, attributeMask & page.attributeMask);
        const u32 fullBytesPerPoint = std::max<u32>(1, layout.bytesPerPoint);
        if (selectedBytesPerPoint == 0 || page.pointCount == 0)
        {
            return 0;
        }
        const u64 rawSelectedBytes = static_cast<u64>(page.pointCount) * static_cast<u64>(selectedBytesPerPoint);
        const u64 rawFullBytes = static_cast<u64>(page.pointCount) * static_cast<u64>(fullBytesPerPoint);
        if (rawFullBytes == 0 || page.payloadBytes == 0)
        {
            return rawSelectedBytes;
        }
        return std::max<u64>(1, (page.payloadBytes * rawSelectedBytes) / rawFullBytes);
    }

    u64 PointCloudRuntimeGpuToken(const PointCloudCookPageRecord& page, u64 attributeMask)
    {
        u64 hash = FnvOffset;
        hash = HashCombine(hash, static_cast<u64>(page.page.x));
        hash = HashCombine(hash, static_cast<u64>(page.page.y));
        hash = HashCombine(hash, static_cast<u64>(page.page.z));
        hash = HashCombine(hash, page.page.lod);
        hash = HashCombine(hash, page.page.ordinal);
        hash = HashCombine(hash, page.payloadHash);
        hash = HashCombine(hash, attributeMask);
        return hash;
    }

    PointCloudRuntimePlan BuildPointCloudRuntimePlan(
        const PointCloudCookManifest& manifest,
        const PointCloudRuntimeFrameDesc& frame,
        const PointCloudRuntimeResidencyPolicy& policy)
    {
        PointCloudRuntimePlan plan{};
        plan.stats.consideredPages = static_cast<u32>(manifest.pages.size());

        if (manifest.pages.empty())
        {
            plan.warnings.push_back("point cloud runtime manifest has no pages");
            UpdateSummary(plan);
            return plan;
        }

        if (!manifest.streamable)
        {
            plan.warnings.push_back("point cloud manifest is not marked streamable");
        }

        u64 requestedAttributeMask = frame.material.requiredAttributeMask;
        if (requestedAttributeMask == 0)
        {
            requestedAttributeMask = AttributeMaskForColorMode(frame.material.colorMode, frame.material.customColorSemantic);
            requestedAttributeMask |= AttributeBit(PointCloudAttributeSemantic::Position);
        }
        requestedAttributeMask &= manifest.layout.packedAttributeMask;
        if ((requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Position)) == 0)
        {
            requestedAttributeMask |= AttributeBit(PointCloudAttributeSemantic::Position);
        }
        plan.stats.requestedAttributeMask = requestedAttributeMask;

        std::vector<PointCloudRuntimePage> candidates;
        candidates.reserve(manifest.pages.size());
        for (const PointCloudCookPageRecord& page : manifest.pages)
        {
            const float distance = RuntimeDistance(Center(page.bounds), frame.cameraLocalMeters);
            if (frame.maxVisibleDistanceMeters > 0.0f && distance > frame.maxVisibleDistanceMeters && page.lod == 0)
            {
                ++plan.stats.skippedByDistance;
                continue;
            }

            PointCloudRuntimePage runtimePage{};
            runtimePage.cooked = page;
            runtimePage.distanceToCameraMeters = distance;
            runtimePage.prefetch = distance > frame.maxVisibleDistanceMeters * 0.75f || page.lod > 0;
            runtimePage.requestedAttributeMask = requestedAttributeMask & page.attributeMask;
            if ((runtimePage.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Position)) == 0)
            {
                runtimePage.requestedAttributeMask |= AttributeBit(PointCloudAttributeSemantic::Position);
            }
            runtimePage.requestedBytes = PointCloudRuntimeEstimatePageBytes(page, manifest.layout, runtimePage.requestedAttributeMask);
            runtimePage.fullPayloadBytes = page.payloadBytes;
            runtimePage.priority = RuntimePriority(distance, page.lod, runtimePage.prefetch);
            runtimePage.gpuBufferToken = PointCloudRuntimeGpuToken(page, runtimePage.requestedAttributeMask);
            candidates.push_back(runtimePage);
        }

        std::sort(candidates.begin(), candidates.end(), [](const PointCloudRuntimePage& a, const PointCloudRuntimePage& b)
        {
            if (a.priority != b.priority)
            {
                return a.priority > b.priority;
            }
            if (a.distanceToCameraMeters != b.distanceToCameraMeters)
            {
                return a.distanceToCameraMeters < b.distanceToCameraMeters;
            }
            return PageIdLess(a.cooked.page, b.cooked.page);
        });

        const u64 streamingBudget = std::min(frame.streamingByteBudget, policy.maxResidentBytes);
        const u32 pageBudget = std::min<u32>(policy.maxResidentPages, static_cast<u32>(candidates.size()));
        u64 accumulatedBytes = 0;
        u64 uploadBytes = 0;
        u64 accumulatedPoints = 0;
        u32 selectedPages = 0;
        u32 prefetchPages = 0;

        OutOfCorePolicy outOfCorePolicy{};
        outOfCorePolicy.chunkPayloadBytes = 256ull * 1024ull;
        outOfCorePolicy.maxResidentChunks = std::max<u32>(1, std::min<u32>(pageBudget == 0 ? 1 : pageBudget, 8));
        outOfCorePolicy.allowSpillFile = policy.allowOutOfCoreSpill;
        outOfCorePolicy.spillFilePath = std::filesystem::temp_directory_path() / "ak_pointcloud_runtime_probe.spill";
        VirtualChunkCache cache(outOfCorePolicy);

        for (PointCloudRuntimePage page : candidates)
        {
            const bool wantsPrefetch = page.prefetch || selectedPages >= pageBudget;
            if (selectedPages >= pageBudget && !wantsPrefetch)
            {
                ++plan.stats.skippedByByteBudget;
                plan.stats.clipped = true;
                continue;
            }
            if (accumulatedBytes + page.requestedBytes > streamingBudget)
            {
                if (prefetchPages < policy.prefetchPageCount && policy.keepCoarserLodsResident && page.cooked.lod > 0)
                {
                    page.prefetch = true;
                }
                else
                {
                    ++plan.stats.skippedByByteBudget;
                    plan.stats.clipped = true;
                    continue;
                }
            }
            if (!page.prefetch && frame.pointBudget > 0 && accumulatedPoints + page.cooked.pointCount > frame.pointBudget)
            {
                ++plan.stats.skippedByPointBudget;
                plan.stats.clipped = true;
                continue;
            }

            page.state = PointCloudRuntimePageState::Requested;
            PushCommand(plan.commands, PointCloudRuntimeCommandType::RequestPage, page, 0, "queue visible point-cloud page");
            PushCommand(plan.commands, PointCloudRuntimeCommandType::ReadPayload, page, page.requestedBytes, "read selected attribute payload");

            const u64 uploadBudgetLeft = frame.uploadByteBudget > uploadBytes ? frame.uploadByteBudget - uploadBytes : 0;
            if (uploadBudgetLeft >= page.requestedBytes)
            {
                page.state = PointCloudRuntimePageState::GpuResident;
                page.drawThisFrame = !page.prefetch;
                PushCommand(plan.commands, PointCloudRuntimeCommandType::UploadGpuBuffer, page, page.requestedBytes, "upload storage-buffer page slice");
                PushCommand(plan.commands, PointCloudRuntimeCommandType::BindAttributeStreams, page, 0, "bind runtime point attribute streams");
                uploadBytes += page.requestedBytes;
                ++plan.stats.gpuResidentPages;
                if (page.drawThisFrame)
                {
                    PushCommand(plan.commands, PointCloudRuntimeCommandType::DrawPage, page, page.requestedBytes, "draw point-cloud page");
                    ++plan.stats.drawnPages;
                    plan.stats.drawnPoints += page.cooked.pointCount;
                }
            }
            else
            {
                page.state = PointCloudRuntimePageState::PayloadResident;
                ++plan.stats.payloadResidentPages;
            }

            std::vector<u8> payload(static_cast<usize>(std::min<u64>(page.requestedBytes, 4096ull)), static_cast<u8>((page.cooked.page.ordinal + page.cooked.lod * 17u) & 0xffu));
            const OutOfCoreChunkId runtimeChunk = cache.AddChunk(payload, frame.frameIndex + selectedPages + 1ull);
            if (!runtimeChunk.IsValid())
            {
                page.state = PointCloudRuntimePageState::Failed;
                plan.warnings.push_back("failed to add runtime point-cloud chunk to out-of-core cache");
            }

            accumulatedBytes += page.requestedBytes;
            accumulatedPoints += page.prefetch ? 0ull : page.cooked.pointCount;
            ++selectedPages;
            if (page.prefetch)
            {
                ++prefetchPages;
                ++plan.stats.prefetchedPages;
            }
            ++plan.stats.requestedPages;
            plan.stats.requestedBytes += page.requestedBytes;
            plan.stats.residentBytes += page.requestedBytes;
            plan.pages.push_back(page);
        }

        if (policy.evictOutsideFramePlan && plan.stats.clipped)
        {
            const u32 evictCount = std::min<u32>(3, plan.stats.skippedByByteBudget + plan.stats.skippedByDistance);
            for (u32 i = 0; i < evictCount; ++i)
            {
                PointCloudRuntimePage evictPage{};
                evictPage.cooked = manifest.pages[manifest.pages.size() - 1u - (i % manifest.pages.size())];
                evictPage.state = PointCloudRuntimePageState::Evictable;
                evictPage.requestedAttributeMask = requestedAttributeMask;
                evictPage.gpuBufferToken = PointCloudRuntimeGpuToken(evictPage.cooked, requestedAttributeMask);
                PushCommand(plan.commands, PointCloudRuntimeCommandType::EvictPage, evictPage, evictPage.cooked.payloadBytes, "evict page outside current frame plan");
                ++plan.stats.evictedPages;
            }
        }

        plan.stats.uploadedBytes = uploadBytes;
        plan.outOfCore = cache.Stats();
        plan.stats.readyForRender = plan.stats.drawnPages > 0 && plan.stats.gpuResidentPages >= plan.stats.drawnPages;
        UpdateSummary(plan);
        return plan;
    }

    std::string ToDebugString(const PointCloudRuntimePage& page)
    {
        std::ostringstream out;
        out << ToDebugString(page.cooked.page)
            << " lod=" << page.cooked.lod
            << " state=" << ToString(page.state)
            << " points=" << page.cooked.pointCount
            << " bytes=" << page.requestedBytes
            << " full=" << page.fullPayloadBytes
            << " attrMask=0x" << std::hex << page.requestedAttributeMask << std::dec
            << " gpu=0x" << std::hex << page.gpuBufferToken << std::dec
            << " dist=" << std::fixed << std::setprecision(1) << page.distanceToCameraMeters
            << " priority=" << std::setprecision(3) << page.priority
            << " draw=" << (page.drawThisFrame ? "true" : "false")
            << " prefetch=" << (page.prefetch ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PointCloudRuntimeCommand& command)
    {
        std::ostringstream out;
        out << ToString(command.type)
            << " " << ToDebugString(command.page)
            << " lod=" << command.lod
            << " bytes=" << command.bytes
            << " attrMask=0x" << std::hex << command.attributeMask << std::dec
            << " gpu=0x" << std::hex << command.gpuBufferToken << std::dec
            << " note='" << command.debugName << "'";
        return out.str();
    }

    std::string ToDebugString(const PointCloudRuntimeStats& stats)
    {
        std::ostringstream out;
        out << "runtime stats considered=" << stats.consideredPages
            << " requested=" << stats.requestedPages
            << " payloadResident=" << stats.payloadResidentPages
            << " gpuResident=" << stats.gpuResidentPages
            << " drawnPages=" << stats.drawnPages
            << " drawnPoints=" << stats.drawnPoints
            << " prefetched=" << stats.prefetchedPages
            << " evicted=" << stats.evictedPages
            << " skippedDistance=" << stats.skippedByDistance
            << " skippedBytes=" << stats.skippedByByteBudget
            << " skippedPoints=" << stats.skippedByPointBudget
            << " requestedBytes=" << stats.requestedBytes
            << " uploadedBytes=" << stats.uploadedBytes
            << " attrMask=0x" << std::hex << stats.requestedAttributeMask << std::dec
            << " clipped=" << (stats.clipped ? "true" : "false")
            << " ready=" << (stats.readyForRender ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PointCloudRuntimePlan& plan)
    {
        std::ostringstream out;
        out << "runtime plan pages=" << plan.pages.size()
            << " commands=" << plan.commands.size()
            << " warnings=" << plan.warnings.size()
            << " " << ToDebugString(plan.stats)
            << " ooc=" << ToDebugString(plan.outOfCore);
        return out.str();
    }

    PointCloudRuntimeProbeResult BuildPointCloudRuntimeProbe()
    {
        PointCloudRuntimeProbeResult probe{};
        probe.cook = BuildPointCloudCookProbe().report;

        PointCloudRuntimeFrameDesc frame{};
        frame.cameraLocalMeters = {0.0f, 90.0f, 0.0f};
        frame.frameIndex = 42;
        frame.streamingByteBudget = 24ull * 1024ull * 1024ull;
        frame.uploadByteBudget = 10ull * 1024ull * 1024ull;
        frame.pointBudget = 900000;
        frame.maxVisibleDistanceMeters = 3500.0f;
        frame.material.colorMode = PointCloudColorMode::Classification;
        frame.material.excludeUnusedStreamedAttributes = true;

        PointCloudRuntimeResidencyPolicy policy{};
        policy.maxResidentBytes = 32ull * 1024ull * 1024ull;
        policy.maxResidentPages = 9;
        policy.prefetchPageCount = 3;
        policy.allowOutOfCoreSpill = true;

        probe.plan = BuildPointCloudRuntimePlan(probe.cook.manifest, frame, policy);

        const bool attrHasPosition = (probe.plan.stats.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Position)) != 0;
        const bool attrHasClassification = (probe.plan.stats.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Classification)) != 0;
        const bool attrExcludesRgb = (probe.plan.stats.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Rgb)) == 0;
        const bool commandsHaveDraw = std::any_of(probe.plan.commands.begin(), probe.plan.commands.end(), [](const PointCloudRuntimeCommand& command)
        {
            return command.type == PointCloudRuntimeCommandType::DrawPage;
        });
        const bool commandsHaveEvict = std::any_of(probe.plan.commands.begin(), probe.plan.commands.end(), [](const PointCloudRuntimeCommand& command)
        {
            return command.type == PointCloudRuntimeCommandType::EvictPage;
        });

        probe.ok = probe.cook.ok && probe.plan.stats.readyForRender && probe.plan.stats.clipped &&
            probe.plan.stats.drawnPages > 0 && probe.plan.stats.requestedPages > 0 &&
            probe.plan.stats.requestedBytes <= frame.streamingByteBudget &&
            probe.plan.stats.uploadedBytes <= frame.uploadByteBudget &&
            attrHasPosition && attrHasClassification && attrExcludesRgb && commandsHaveDraw && commandsHaveEvict;

        std::ostringstream out;
        out << (probe.ok ? "[ ok ] " : "[fail] ")
            << "pointcloud runtime streaming / residency foundation "
            << "pages=" << probe.plan.pages.size()
            << " commands=" << probe.plan.commands.size()
            << " drawn=" << probe.plan.stats.drawnPages
            << " requestedBytes=" << probe.plan.stats.requestedBytes
            << " uploadedBytes=" << probe.plan.stats.uploadedBytes
            << " attrMask=0x" << std::hex << probe.plan.stats.requestedAttributeMask << std::dec
            << " ready=" << (probe.plan.stats.readyForRender ? "true" : "false");
        probe.summary = out.str();
        return probe;
    }

    std::string BuildPointCloudRuntimeProbeSummary()
    {
        return BuildPointCloudRuntimeProbe().summary;
    }
}
