#pragma once

#include <AK/PointCloud/PointCloud.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace AK
{
    enum class PointCloudSourceFormat : u32
    {
        Unknown = 0,
        XyzAscii = 1,
        Ply = 2,
        Las = 3,
        Laz = 4,
        E57 = 5,
        Dem = 6,
        UdStream = 7,
        CustomCallback = 8
    };

    enum class PointCloudCookStream : u32
    {
        Core = 0,
        Color = 1,
        Classification = 2,
        Auxiliary = 3
    };

    enum class PointCloudCompressionMode : u32
    {
        None = 0,
        QuantizedDelta = 1,
        GDeflateReady = 2,
        External = 3
    };

    struct PointCloudSourceDesc final
    {
        std::filesystem::path sourcePath;
        PointCloudSourceFormat format = PointCloudSourceFormat::Unknown;
        u64 estimatedPointCount = 0;
        AABB3 localBounds{};
        GeoReference geo{};
        bool georeferenced = false;
        bool streamedReader = true;
        bool hasRgb = false;
        bool hasIntensity = false;
        bool hasClassification = false;
        bool hasReturnIndex = false;
        bool hasNormals = false;
    };

    struct PointCloudAttributeLayoutElement final
    {
        PointCloudAttributeSemantic semantic = PointCloudAttributeSemantic::Position;
        PointCloudAttributeFormat format = PointCloudAttributeFormat::Float32;
        PointCloudCookStream stream = PointCloudCookStream::Core;
        u32 components = 0;
        u32 byteOffset = 0;
        u32 byteSize = 0;
        std::string sourceName;
        bool normalized = false;
        bool required = false;
    };

    struct PointCloudAttributeLayout final
    {
        std::vector<PointCloudAttributeLayoutElement> elements;
        std::array<u32, 4> streamStrides{};
        u32 streamCount = 0;
        u32 bytesPerPoint = 0;
        u64 packedAttributeMask = 0;
        bool positionFirst = false;
        bool gpuStorageBufferReady = false;
    };

    struct PointCloudCookPolicy final
    {
        u32 chunkTargetPoints = 250000;
        u32 lodLevels = 4;
        u32 lodDownsampleFactor = 4;
        u64 maxChunkPayloadBytes = 4ull * 1024ull * 1024ull;
        PointCloudCompressionMode compression = PointCloudCompressionMode::QuantizedDelta;
        PointCloudMaterialPolicy defaultMaterial{};
        bool quantizePositions = true;
        bool buildLodHierarchy = true;
        bool includePageHashes = true;
        bool includeDebugBounds = true;
        bool deterministicPageOrder = true;
        bool allowExternalSdkBridge = false;
    };

    struct PointCloudCookPageRecord final
    {
        PointCloudPageId page{};
        u32 lod = 0;
        u64 pointOffset = 0;
        u32 pointCount = 0;
        u64 fullResolutionPointCount = 0;
        AABB3 bounds{};
        u64 fileOffsetBytes = 0;
        u64 payloadBytes = 0;
        u64 attributeMask = 0;
        u64 payloadHash = 0;
        u32 streamCount = 0;
    };

    struct PointCloudCookManifest final
    {
        std::string magic = "AKPCOOK";
        u32 version = 1;
        AssetGuid guid{};
        std::filesystem::path sourcePath;
        PointCloudSourceFormat sourceFormat = PointCloudSourceFormat::Unknown;
        u64 sourcePointCount = 0;
        AABB3 localBounds{};
        GeoReference geo{};
        bool georeferenced = false;
        PointCloudAttributeLayout layout{};
        std::vector<PointCloudCookPageRecord> pages;
        u32 lodLevels = 0;
        u64 totalPayloadBytes = 0;
        u64 rootHash = 0;
        bool streamable = true;
        bool externalSdkOptional = false;
    };

    struct PointCloudCookValidationReport final
    {
        bool ok = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct PointCloudCookReport final
    {
        bool ok = false;
        PointCloudSourceDesc source{};
        PointCloudAssetDesc asset{};
        PointCloudCookPolicy policy{};
        PointCloudCookManifest manifest{};
        PointCloudStreamingPlan streamingPlan{};
        OutOfCoreStats outOfCore{};
        PointCloudCookValidationReport validation{};
        std::string summary;
    };

    struct PointCloudCookProbeResult final
    {
        bool ok = false;
        PointCloudCookReport report{};
        std::string summary;
    };

    const char* ToString(PointCloudSourceFormat format);
    const char* ToString(PointCloudCookStream stream);
    const char* ToString(PointCloudCompressionMode mode);

    PointCloudSourceFormat DetectPointCloudSourceFormat(const std::filesystem::path& path);
    PointCloudSourceDesc BuildPointCloudSourceDescFromAsset(const PointCloudAssetDesc& asset);
    PointCloudAttributeLayout BuildPointCloudAttributeLayout(const PointCloudAssetDesc& asset);
    PointCloudCookValidationReport ValidatePointCloudCookInputs(const PointCloudSourceDesc& source, const PointCloudAssetDesc& asset, const PointCloudCookPolicy& policy);
    PointCloudCookManifest BuildPointCloudCookManifest(const PointCloudSourceDesc& source, const PointCloudAssetDesc& asset, const PointCloudCookPolicy& policy);
    PointCloudCookReport BuildPointCloudCookReport(const PointCloudSourceDesc& source, PointCloudAssetDesc asset, PointCloudCookPolicy policy = {});

    u64 HashPointCloudCookPage(const PointCloudCookPageRecord& page);
    u64 HashPointCloudCookManifest(const PointCloudCookManifest& manifest);
    u32 EffectiveCookChunkTargetPoints(const PointCloudAssetDesc& asset, const PointCloudCookPolicy& policy, const PointCloudAttributeLayout& layout);

    std::string ToDebugString(const PointCloudSourceDesc& source);
    std::string ToDebugString(const PointCloudAttributeLayoutElement& element);
    std::string ToDebugString(const PointCloudAttributeLayout& layout);
    std::string ToDebugString(const PointCloudCookPageRecord& page);
    std::string ToDebugString(const PointCloudCookManifest& manifest);
    std::string ToDebugString(const PointCloudCookValidationReport& report);
    std::string ToDebugString(const PointCloudCookReport& report);

    PointCloudCookProbeResult BuildPointCloudCookProbe();
    std::string BuildPointCloudCookProbeSummary();
}
