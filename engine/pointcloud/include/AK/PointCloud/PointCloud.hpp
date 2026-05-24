#pragma once

#include <AK/Core/Guid.hpp>
#include <AK/Core/Types.hpp>
#include <AK/GeoData/GeoData.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/OutOfCore/OutOfCore.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace AK
{
    enum class PointCloudAttributeSemantic : u32
    {
        Position = 0,
        Rgb = 1,
        Intensity = 2,
        Classification = 3,
        Height = 4,
        Normal = 5,
        ReturnIndex = 6,
        ScanAngle = 7,
        Custom0 = 8,
        Custom1 = 9
    };

    enum class PointCloudAttributeFormat : u32
    {
        UInt8 = 0,
        UInt16 = 1,
        UInt32 = 2,
        Float32 = 3,
        Float64 = 4
    };

    enum class PointCloudPageState : u32
    {
        Unloaded = 0,
        Queued = 1,
        Resident = 2,
        Evictable = 3
    };

    enum class PointCloudColorMode : u32
    {
        Rgb = 0,
        Intensity = 1,
        Classification = 2,
        Height = 3,
        Solid = 4,
        Custom = 5
    };

    struct PointCloudPageId final
    {
        i64 x = 0;
        i64 y = 0;
        i64 z = 0;
        u32 lod = 0;
        u32 ordinal = 0;
    };

    struct PointCloudAttributeDesc final
    {
        PointCloudAttributeSemantic semantic = PointCloudAttributeSemantic::Position;
        PointCloudAttributeFormat format = PointCloudAttributeFormat::Float32;
        u32 components = 3;
        std::string name;
        bool streamable = true;
        bool requiredByDefault = false;
    };

    struct PointCloudStreamingPolicy final
    {
        u64 targetPagePayloadBytes = 4ull * 1024ull * 1024ull;
        u64 maxResidentBytes = 512ull * 1024ull * 1024ull;
        u32 maxResidentPages = 128;
        u32 prefetchRings = 1;
        bool attributeStreaming = true;
        bool allowOutOfCoreSpill = true;
        bool deterministicPageOrder = true;
    };

    struct PointCloudRenderPolicy final
    {
        u32 maxPointsPerFrame = 5000000;
        float pointSizePixels = 1.0f;
        float targetScreenErrorPixels = 1.5f;
        bool cameraRelative = true;
        bool gpuDrivenCulling = true;
        bool useVulkanStorageBuffers = true;
        bool allowComputeExpansion = true;
    };

    struct PointCloudMaterialPolicy final
    {
        PointCloudColorMode colorMode = PointCloudColorMode::Rgb;
        PointCloudAttributeSemantic customColorSemantic = PointCloudAttributeSemantic::Custom0;
        u64 requiredAttributeMask = 0;
        bool excludeUnusedStreamedAttributes = true;
        bool classificationPalette = true;
        bool intensityRamp = true;
    };

    struct PointCloudAssetDesc final
    {
        AssetGuid guid{};
        std::filesystem::path sourcePath;
        std::string debugName;
        u64 pointCount = 0;
        u32 chunkTargetPoints = 250000;
        AABB3 localBounds{};
        GeoReference geo{};
        std::vector<PointCloudAttributeDesc> attributes;
        PointCloudStreamingPolicy streaming{};
        PointCloudRenderPolicy render{};
        PointCloudMaterialPolicy material{};
        bool georeferenced = false;
        bool cooked = false;
    };

    struct PointCloudChunkDesc final
    {
        PointCloudPageId page{};
        u64 pointOffset = 0;
        u32 pointCount = 0;
        u64 payloadBytes = 0;
        u64 selectedPayloadBytes = 0;
        u64 attributeMask = 0;
        AABB3 bounds{};
        PointCloudPageState state = PointCloudPageState::Unloaded;
    };

    struct PointCloudValidationReport final
    {
        bool ok = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct PointCloudStreamingPlan final
    {
        std::vector<PointCloudChunkDesc> chunks;
        u64 requestedAttributeMask = 0;
        u64 requestedPayloadBytes = 0;
        u64 fullPayloadBytes = 0;
        u64 selectedPointCount = 0;
        u32 skippedByBudget = 0;
        bool clippedByBudget = false;
        std::string summary;
    };

    struct PointCloudProbeResult final
    {
        bool ok = false;
        PointCloudAssetDesc asset{};
        std::vector<PointCloudChunkDesc> chunks;
        PointCloudStreamingPlan streamingPlan{};
        GeoDataProbeResult geo{};
        OutOfCoreStats outOfCore{};
        PointCloudValidationReport validation{};
        std::string summary;
    };

    const char* ToString(PointCloudAttributeSemantic semantic);
    const char* ToString(PointCloudAttributeFormat format);
    const char* ToString(PointCloudPageState state);
    const char* ToString(PointCloudColorMode mode);

    u32 BytesPerComponent(PointCloudAttributeFormat format);
    u32 BytesPerAttribute(const PointCloudAttributeDesc& attribute);
    u64 AttributeBit(PointCloudAttributeSemantic semantic);
    u64 AttributeMaskForColorMode(PointCloudColorMode mode, PointCloudAttributeSemantic customSemantic = PointCloudAttributeSemantic::Custom0);
    u64 BuildPointCloudAttributeMask(const std::vector<PointCloudAttributeDesc>& attributes, bool onlyRequired);
    u64 BuildPointCloudAttributeRequest(const PointCloudAssetDesc& asset, const PointCloudMaterialPolicy& material);
    u32 BytesPerPoint(const PointCloudAssetDesc& asset, u64 attributeMask);
    u64 EstimatePointCloudBytes(const PointCloudAssetDesc& asset, u64 attributeMask);

    PointCloudValidationReport ValidatePointCloudAsset(const PointCloudAssetDesc& asset);
    std::vector<PointCloudChunkDesc> BuildPointCloudChunks(const PointCloudAssetDesc& asset);
    PointCloudStreamingPlan BuildPointCloudStreamingPlan(const PointCloudAssetDesc& asset, Vec3 cameraLocalMeters, u64 streamingBudgetBytes);

    std::string ToDebugString(PointCloudPageId id);
    std::string ToDebugString(const PointCloudAttributeDesc& attribute);
    std::string ToDebugString(const PointCloudValidationReport& report);
    std::string ToDebugString(const PointCloudChunkDesc& chunk);
    std::string ToDebugString(const PointCloudStreamingPlan& plan);
    std::string ToDebugString(const PointCloudAssetDesc& asset);

    PointCloudProbeResult BuildPointCloudProbe();
    std::string BuildPointCloudProbeSummary();
}
