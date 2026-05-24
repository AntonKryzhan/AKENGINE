#pragma once

#include <AK/PointCloud/PointCloudCook.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class PointCloudRuntimePageState : u32
    {
        Unloaded = 0,
        Requested = 1,
        PayloadResident = 2,
        GpuResident = 3,
        Evictable = 4,
        Failed = 5
    };

    enum class PointCloudRuntimeCommandType : u32
    {
        RequestPage = 0,
        ReadPayload = 1,
        UploadGpuBuffer = 2,
        BindAttributeStreams = 3,
        DrawPage = 4,
        EvictPage = 5
    };

    struct PointCloudRuntimeFrameDesc final
    {
        Vec3 cameraLocalMeters{};
        u64 frameIndex = 0;
        u64 streamingByteBudget = 64ull * 1024ull * 1024ull;
        u64 uploadByteBudget = 16ull * 1024ull * 1024ull;
        u32 pointBudget = 2000000;
        float maxVisibleDistanceMeters = 10000.0f;
        PointCloudMaterialPolicy material{};
    };

    struct PointCloudRuntimeResidencyPolicy final
    {
        u64 maxResidentBytes = 256ull * 1024ull * 1024ull;
        u32 maxResidentPages = 96;
        u32 prefetchPageCount = 8;
        bool deterministicOrder = true;
        bool evictOutsideFramePlan = true;
        bool allowOutOfCoreSpill = true;
        bool keepCoarserLodsResident = true;
    };

    struct PointCloudRuntimePage final
    {
        PointCloudCookPageRecord cooked{};
        PointCloudRuntimePageState state = PointCloudRuntimePageState::Unloaded;
        u64 requestedAttributeMask = 0;
        u64 requestedBytes = 0;
        u64 fullPayloadBytes = 0;
        u64 gpuBufferToken = 0;
        float distanceToCameraMeters = 0.0f;
        float priority = 0.0f;
        bool drawThisFrame = false;
        bool prefetch = false;
    };

    struct PointCloudRuntimeCommand final
    {
        PointCloudRuntimeCommandType type = PointCloudRuntimeCommandType::RequestPage;
        PointCloudPageId page{};
        u32 lod = 0;
        u64 bytes = 0;
        u64 attributeMask = 0;
        u64 gpuBufferToken = 0;
        std::string debugName;
    };

    struct PointCloudRuntimeStats final
    {
        u32 consideredPages = 0;
        u32 requestedPages = 0;
        u32 payloadResidentPages = 0;
        u32 gpuResidentPages = 0;
        u32 drawnPages = 0;
        u32 prefetchedPages = 0;
        u32 evictedPages = 0;
        u32 skippedByDistance = 0;
        u32 skippedByByteBudget = 0;
        u32 skippedByPointBudget = 0;
        u64 requestedBytes = 0;
        u64 uploadedBytes = 0;
        u64 residentBytes = 0;
        u64 drawnPoints = 0;
        u64 requestedAttributeMask = 0;
        bool clipped = false;
        bool readyForRender = false;
        std::string summary;
    };

    struct PointCloudRuntimePlan final
    {
        std::vector<PointCloudRuntimePage> pages;
        std::vector<PointCloudRuntimeCommand> commands;
        PointCloudRuntimeStats stats{};
        OutOfCoreStats outOfCore{};
        std::vector<std::string> warnings;
        std::string summary;
    };

    struct PointCloudRuntimeProbeResult final
    {
        bool ok = false;
        PointCloudCookReport cook{};
        PointCloudRuntimePlan plan{};
        std::string summary;
    };

    const char* ToString(PointCloudRuntimePageState state);
    const char* ToString(PointCloudRuntimeCommandType type);

    u32 PointCloudRuntimeStreamCount();
    u64 PointCloudRuntimeStreamMaskForAttributes(const PointCloudAttributeLayout& layout, u64 attributeMask);
    u32 PointCloudRuntimeBytesPerPoint(const PointCloudAttributeLayout& layout, u64 attributeMask);
    u64 PointCloudRuntimeEstimatePageBytes(const PointCloudCookPageRecord& page, const PointCloudAttributeLayout& layout, u64 attributeMask);
    u64 PointCloudRuntimeGpuToken(const PointCloudCookPageRecord& page, u64 attributeMask);

    PointCloudRuntimePlan BuildPointCloudRuntimePlan(
        const PointCloudCookManifest& manifest,
        const PointCloudRuntimeFrameDesc& frame,
        const PointCloudRuntimeResidencyPolicy& policy = {});

    std::string ToDebugString(const PointCloudRuntimePage& page);
    std::string ToDebugString(const PointCloudRuntimeCommand& command);
    std::string ToDebugString(const PointCloudRuntimeStats& stats);
    std::string ToDebugString(const PointCloudRuntimePlan& plan);

    PointCloudRuntimeProbeResult BuildPointCloudRuntimeProbe();
    std::string BuildPointCloudRuntimeProbeSummary();
}
