#include <AK/PointCloud/PointCloudCook.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

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

        u64 HashString(u64 hash, const std::string& text)
        {
            for (char c : text)
            {
                hash ^= static_cast<u8>(c);
                hash *= FnvPrime;
            }
            return hash;
        }

        u64 AlignUp(u64 value, u64 alignment)
        {
            if (alignment <= 1)
            {
                return value;
            }
            return ((value + alignment - 1) / alignment) * alignment;
        }

        std::string LowerExtension(const std::filesystem::path& path)
        {
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return ext;
        }

        PointCloudCookStream StreamForSemantic(PointCloudAttributeSemantic semantic)
        {
            switch (semantic)
            {
                case PointCloudAttributeSemantic::Position:
                case PointCloudAttributeSemantic::Normal:
                    return PointCloudCookStream::Core;
                case PointCloudAttributeSemantic::Rgb:
                    return PointCloudCookStream::Color;
                case PointCloudAttributeSemantic::Intensity:
                case PointCloudAttributeSemantic::Classification:
                case PointCloudAttributeSemantic::ReturnIndex:
                    return PointCloudCookStream::Classification;
                case PointCloudAttributeSemantic::Height:
                case PointCloudAttributeSemantic::ScanAngle:
                case PointCloudAttributeSemantic::Custom0:
                case PointCloudAttributeSemantic::Custom1:
                default:
                    return PointCloudCookStream::Auxiliary;
            }
        }

        u32 StreamIndex(PointCloudCookStream stream)
        {
            return static_cast<u32>(stream);
        }


        float LerpCookFloat(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        AABB3 CookChunkBounds(AABB3 bounds, u32 chunkIndex, u32 chunkCount, u32 lod)
        {
            const u32 safeChunkCount = std::max<u32>(1, chunkCount);
            const float t0 = static_cast<float>(chunkIndex) / static_cast<float>(safeChunkCount);
            const float t1 = static_cast<float>(chunkIndex + 1) / static_cast<float>(safeChunkCount);
            const float lodPad = static_cast<float>(lod) * 0.25f;
            Vec3 minValue = {
                LerpCookFloat(bounds.min.x, bounds.max.x, t0) - lodPad,
                bounds.min.y - lodPad,
                bounds.min.z - lodPad
            };
            Vec3 maxValue = {
                LerpCookFloat(bounds.min.x, bounds.max.x, t1) + lodPad,
                bounds.max.y + lodPad,
                bounds.max.z + lodPad
            };
            return MakeAABB3(minValue, maxValue);
        }

        u64 PowU64(u64 base, u32 exponent)
        {
            u64 value = 1;
            for (u32 i = 0; i < exponent; ++i)
            {
                value *= base;
            }
            return value;
        }

        void Merge(PointCloudCookValidationReport& into, const PointCloudValidationReport& from)
        {
            into.ok = into.ok && from.ok;
            into.warnings.insert(into.warnings.end(), from.warnings.begin(), from.warnings.end());
            into.errors.insert(into.errors.end(), from.errors.begin(), from.errors.end());
        }

        void Merge(PointCloudCookValidationReport& into, const GeoValidationReport& from)
        {
            into.ok = into.ok && from.ok;
            into.warnings.insert(into.warnings.end(), from.warnings.begin(), from.warnings.end());
            into.errors.insert(into.errors.end(), from.errors.begin(), from.errors.end());
        }
    }

    const char* ToString(PointCloudSourceFormat format)
    {
        switch (format)
        {
            case PointCloudSourceFormat::XyzAscii:
                return "xyz-ascii";
            case PointCloudSourceFormat::Ply:
                return "ply";
            case PointCloudSourceFormat::Las:
                return "las";
            case PointCloudSourceFormat::Laz:
                return "laz";
            case PointCloudSourceFormat::E57:
                return "e57";
            case PointCloudSourceFormat::Dem:
                return "dem";
            case PointCloudSourceFormat::UdStream:
                return "ud-stream";
            case PointCloudSourceFormat::CustomCallback:
                return "custom-callback";
            case PointCloudSourceFormat::Unknown:
            default:
                return "unknown";
        }
    }

    const char* ToString(PointCloudCookStream stream)
    {
        switch (stream)
        {
            case PointCloudCookStream::Core:
                return "core";
            case PointCloudCookStream::Color:
                return "color";
            case PointCloudCookStream::Classification:
                return "classification";
            case PointCloudCookStream::Auxiliary:
                return "auxiliary";
            default:
                return "unknown";
        }
    }

    const char* ToString(PointCloudCompressionMode mode)
    {
        switch (mode)
        {
            case PointCloudCompressionMode::None:
                return "none";
            case PointCloudCompressionMode::QuantizedDelta:
                return "quantized-delta";
            case PointCloudCompressionMode::GDeflateReady:
                return "gdeflate-ready";
            case PointCloudCompressionMode::External:
                return "external";
            default:
                return "unknown";
        }
    }

    PointCloudSourceFormat DetectPointCloudSourceFormat(const std::filesystem::path& path)
    {
        const std::string ext = LowerExtension(path);
        if (ext == ".xyz" || ext == ".pts" || ext == ".csv")
        {
            return PointCloudSourceFormat::XyzAscii;
        }
        if (ext == ".ply")
        {
            return PointCloudSourceFormat::Ply;
        }
        if (ext == ".las")
        {
            return PointCloudSourceFormat::Las;
        }
        if (ext == ".laz")
        {
            return PointCloudSourceFormat::Laz;
        }
        if (ext == ".e57")
        {
            return PointCloudSourceFormat::E57;
        }
        if (ext == ".tif" || ext == ".tiff" || ext == ".dem" || ext == ".asc")
        {
            return PointCloudSourceFormat::Dem;
        }
        if (ext == ".uds" || ext == ".udstream")
        {
            return PointCloudSourceFormat::UdStream;
        }
        return PointCloudSourceFormat::Unknown;
    }

    PointCloudSourceDesc BuildPointCloudSourceDescFromAsset(const PointCloudAssetDesc& asset)
    {
        PointCloudSourceDesc source{};
        source.sourcePath = asset.sourcePath;
        source.format = DetectPointCloudSourceFormat(asset.sourcePath);
        source.estimatedPointCount = asset.pointCount;
        source.localBounds = asset.localBounds;
        source.geo = asset.geo;
        source.georeferenced = asset.georeferenced;
        source.streamedReader = true;

        for (const PointCloudAttributeDesc& attribute : asset.attributes)
        {
            source.hasRgb = source.hasRgb || attribute.semantic == PointCloudAttributeSemantic::Rgb;
            source.hasIntensity = source.hasIntensity || attribute.semantic == PointCloudAttributeSemantic::Intensity;
            source.hasClassification = source.hasClassification || attribute.semantic == PointCloudAttributeSemantic::Classification;
            source.hasReturnIndex = source.hasReturnIndex || attribute.semantic == PointCloudAttributeSemantic::ReturnIndex;
            source.hasNormals = source.hasNormals || attribute.semantic == PointCloudAttributeSemantic::Normal;
        }
        return source;
    }

    PointCloudAttributeLayout BuildPointCloudAttributeLayout(const PointCloudAssetDesc& asset)
    {
        PointCloudAttributeLayout layout{};
        layout.elements.reserve(asset.attributes.size());

        for (const PointCloudAttributeDesc& attribute : asset.attributes)
        {
            const PointCloudCookStream stream = StreamForSemantic(attribute.semantic);
            const u32 streamIndex = StreamIndex(stream);
            PointCloudAttributeLayoutElement element{};
            element.semantic = attribute.semantic;
            element.format = attribute.format;
            element.stream = stream;
            element.components = attribute.components;
            element.byteOffset = layout.streamStrides[streamIndex];
            element.byteSize = BytesPerAttribute(attribute);
            element.sourceName = attribute.name;
            element.normalized = attribute.format == PointCloudAttributeFormat::UInt8 || attribute.format == PointCloudAttributeFormat::UInt16;
            element.required = attribute.requiredByDefault || attribute.semantic == PointCloudAttributeSemantic::Position;

            layout.streamStrides[streamIndex] += element.byteSize;
            layout.bytesPerPoint += element.byteSize;
            layout.packedAttributeMask |= AttributeBit(attribute.semantic);
            layout.positionFirst = layout.positionFirst || (layout.elements.empty() && attribute.semantic == PointCloudAttributeSemantic::Position);
            layout.elements.push_back(element);
        }

        layout.streamCount = 0;
        for (u32 stride : layout.streamStrides)
        {
            if (stride > 0)
            {
                ++layout.streamCount;
            }
        }
        layout.gpuStorageBufferReady = layout.positionFirst && layout.streamStrides[0] >= 12 && layout.streamCount > 0 && layout.bytesPerPoint > 0;
        return layout;
    }

    PointCloudCookValidationReport ValidatePointCloudCookInputs(const PointCloudSourceDesc& source, const PointCloudAssetDesc& asset, const PointCloudCookPolicy& policy)
    {
        PointCloudCookValidationReport report{};
        Merge(report, ValidatePointCloudAsset(asset));

        if (source.format == PointCloudSourceFormat::Unknown)
        {
            report.ok = false;
            report.errors.push_back("point cloud source format is unknown");
        }
        if (source.estimatedPointCount == 0)
        {
            report.ok = false;
            report.errors.push_back("point cloud source must expose a non-zero estimated point count");
        }
        if (!IsValid(source.localBounds))
        {
            report.ok = false;
            report.errors.push_back("point cloud source bounds are invalid");
        }
        if (source.georeferenced)
        {
            Merge(report, ValidateGeoReference(source.geo));
        }
        if (policy.chunkTargetPoints == 0)
        {
            report.ok = false;
            report.errors.push_back("point cloud cook chunk target must be non-zero");
        }
        if (policy.lodLevels == 0)
        {
            report.ok = false;
            report.errors.push_back("point cloud cook must produce at least one LOD level");
        }
        if (policy.lodLevels > 16)
        {
            report.ok = false;
            report.errors.push_back("point cloud cook LOD level count is too high");
        }
        if (policy.lodDownsampleFactor < 2 && policy.lodLevels > 1)
        {
            report.ok = false;
            report.errors.push_back("point cloud LOD downsample factor must be at least two when multiple LODs are requested");
        }

        const PointCloudAttributeLayout layout = BuildPointCloudAttributeLayout(asset);
        if (!layout.positionFirst)
        {
            report.warnings.push_back("position attribute is not first; GPU vertex/storage expansion may need an extra gather step");
        }
        if (!layout.gpuStorageBufferReady)
        {
            report.ok = false;
            report.errors.push_back("point cloud attribute layout is not GPU storage-buffer ready");
        }
        if (policy.maxChunkPayloadBytes > 0 && layout.bytesPerPoint > policy.maxChunkPayloadBytes)
        {
            report.ok = false;
            report.errors.push_back("single point is larger than the requested max chunk payload");
        }
        if (policy.allowExternalSdkBridge && source.format != PointCloudSourceFormat::UdStream)
        {
            report.warnings.push_back("external SDK bridge requested for a non-UD stream source; native cooker path will remain authoritative");
        }
        if (policy.compression == PointCloudCompressionMode::External && !policy.allowExternalSdkBridge)
        {
            report.ok = false;
            report.errors.push_back("external compression requires allowExternalSdkBridge=true");
        }
        return report;
    }

    u32 EffectiveCookChunkTargetPoints(const PointCloudAssetDesc& asset, const PointCloudCookPolicy& policy, const PointCloudAttributeLayout& layout)
    {
        if (policy.maxChunkPayloadBytes == 0 || layout.bytesPerPoint == 0)
        {
            return std::max<u32>(1, policy.chunkTargetPoints != 0 ? policy.chunkTargetPoints : asset.chunkTargetPoints);
        }

        const u64 maxPointsByPayload = std::max<u64>(1, policy.maxChunkPayloadBytes / static_cast<u64>(layout.bytesPerPoint));
        const u64 requested = policy.chunkTargetPoints != 0 ? policy.chunkTargetPoints : asset.chunkTargetPoints;
        return static_cast<u32>(std::max<u64>(1, std::min<u64>(requested, maxPointsByPayload)));
    }

    PointCloudCookManifest BuildPointCloudCookManifest(const PointCloudSourceDesc& source, const PointCloudAssetDesc& asset, const PointCloudCookPolicy& policy)
    {
        PointCloudCookManifest manifest{};
        manifest.guid = asset.guid;
        manifest.sourcePath = source.sourcePath;
        manifest.sourceFormat = source.format;
        manifest.sourcePointCount = source.estimatedPointCount;
        manifest.localBounds = source.localBounds;
        manifest.geo = source.geo;
        manifest.georeferenced = source.georeferenced;
        manifest.layout = BuildPointCloudAttributeLayout(asset);
        manifest.streamable = asset.streaming.maxResidentPages > 0;
        manifest.externalSdkOptional = policy.allowExternalSdkBridge;

        const u32 chunkTarget = EffectiveCookChunkTargetPoints(asset, policy, manifest.layout);
        const u32 requestedLods = policy.buildLodHierarchy ? std::max<u32>(1, policy.lodLevels) : 1;
        const u64 attributeMask = manifest.layout.packedAttributeMask;
        u64 fileOffset = 0;
        u32 globalOrdinal = 0;

        for (u32 lod = 0; lod < requestedLods; ++lod)
        {
            const u64 downsample = lod == 0 ? 1ull : PowU64(std::max<u32>(2, policy.lodDownsampleFactor), lod);
            const u64 lodPoints = std::max<u64>(1, (source.estimatedPointCount + downsample - 1) / downsample);
            const u32 chunkCount = static_cast<u32>((lodPoints + chunkTarget - 1) / chunkTarget);
            if (chunkCount == 0)
            {
                continue;
            }

            for (u32 chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
            {
                const u64 pointOffset = static_cast<u64>(chunkIndex) * static_cast<u64>(chunkTarget);
                const u64 remaining = lodPoints - pointOffset;
                const u32 pointCount = static_cast<u32>(std::min<u64>(remaining, chunkTarget));
                const u64 payloadBytes = static_cast<u64>(pointCount) * static_cast<u64>(manifest.layout.bytesPerPoint);

                PointCloudCookPageRecord page{};
                page.page.x = static_cast<i64>(chunkIndex);
                page.page.y = 0;
                page.page.z = 0;
                page.page.lod = lod;
                page.page.ordinal = globalOrdinal++;
                page.lod = lod;
                page.pointOffset = pointOffset;
                page.pointCount = pointCount;
                page.fullResolutionPointCount = std::min<u64>(source.estimatedPointCount, pointOffset * downsample + static_cast<u64>(pointCount) * downsample);
                page.bounds = CookChunkBounds(source.localBounds, chunkIndex, chunkCount, lod);
                page.fileOffsetBytes = AlignUp(fileOffset, 16);
                page.payloadBytes = payloadBytes;
                page.attributeMask = attributeMask;
                page.streamCount = manifest.layout.streamCount;
                page.payloadHash = policy.includePageHashes ? HashPointCloudCookPage(page) : 0;

                fileOffset = page.fileOffsetBytes + page.payloadBytes;
                manifest.totalPayloadBytes += page.payloadBytes;
                manifest.pages.push_back(page);
            }
        }

        manifest.lodLevels = requestedLods;
        manifest.rootHash = HashPointCloudCookManifest(manifest);
        return manifest;
    }

    PointCloudCookReport BuildPointCloudCookReport(const PointCloudSourceDesc& source, PointCloudAssetDesc asset, PointCloudCookPolicy policy)
    {
        PointCloudCookReport report{};
        report.source = source;
        report.policy = policy;

        if (policy.chunkTargetPoints != 0)
        {
            asset.chunkTargetPoints = policy.chunkTargetPoints;
        }
        asset.material = policy.defaultMaterial;
        asset.cooked = true;
        report.asset = asset;
        report.validation = ValidatePointCloudCookInputs(source, asset, policy);
        report.manifest = BuildPointCloudCookManifest(source, asset, policy);
        report.streamingPlan = BuildPointCloudStreamingPlan(asset, Center(asset.localBounds), asset.streaming.maxResidentBytes / 8ull);

        OutOfCorePolicy cachePolicy{};
        cachePolicy.chunkPayloadBytes = 512;
        cachePolicy.maxResidentChunks = 4;
        cachePolicy.spillFilePath = std::filesystem::temp_directory_path() / "ak_pointcloud_cook_probe.spill";
        VirtualChunkCache cache(cachePolicy);
        for (u32 i = 0; i < std::min<u32>(8, static_cast<u32>(report.manifest.pages.size())); ++i)
        {
            const PointCloudCookPageRecord& page = report.manifest.pages[i];
            std::vector<u8> bytes(cachePolicy.chunkPayloadBytes, static_cast<u8>((page.payloadHash >> 8u) & 0xffu));
            const OutOfCoreChunkId added = cache.AddChunk(bytes, static_cast<u64>(i + 1));
            (void)added;
        }
        if (!report.manifest.pages.empty())
        {
            const OutOfCoreReadResult readBack = cache.ReadChunk({1}, 100);
            (void)readBack;
        }
        report.outOfCore = cache.Stats();

        const bool hasMultipleLods = report.manifest.lodLevels >= 3 && report.manifest.pages.size() > 3;
        const bool hasStableHash = report.manifest.rootHash != 0;
        const bool layoutReady = report.manifest.layout.gpuStorageBufferReady && report.manifest.layout.streamCount >= 3;
        const bool budgeted = report.streamingPlan.clippedByBudget || !report.streamingPlan.chunks.empty();
        report.ok = report.validation.ok && hasMultipleLods && hasStableHash && layoutReady && budgeted && report.outOfCore.ok;

        std::ostringstream out;
        out << (report.ok ? "[ ok ] " : "[fail] ")
            << "pointcloud import/cook foundation "
            << "format=" << ToString(source.format)
            << " pages=" << report.manifest.pages.size()
            << " lods=" << report.manifest.lodLevels
            << " bytes=" << report.manifest.totalPayloadBytes
            << " stride=" << report.manifest.layout.bytesPerPoint
            << " streams=" << report.manifest.layout.streamCount
            << " rootHash=0x" << std::hex << report.manifest.rootHash << std::dec
            << " ooc=" << ToDebugString(report.outOfCore);
        report.summary = out.str();
        return report;
    }

    u64 HashPointCloudCookPage(const PointCloudCookPageRecord& page)
    {
        u64 hash = FnvOffset;
        hash = HashCombine(hash, static_cast<u64>(page.page.x));
        hash = HashCombine(hash, static_cast<u64>(page.page.y));
        hash = HashCombine(hash, static_cast<u64>(page.page.z));
        hash = HashCombine(hash, page.page.lod);
        hash = HashCombine(hash, page.page.ordinal);
        hash = HashCombine(hash, page.pointOffset);
        hash = HashCombine(hash, page.pointCount);
        hash = HashCombine(hash, page.fullResolutionPointCount);
        hash = HashCombine(hash, page.fileOffsetBytes);
        hash = HashCombine(hash, page.payloadBytes);
        hash = HashCombine(hash, page.attributeMask);
        hash = HashCombine(hash, page.streamCount);
        return hash;
    }

    u64 HashPointCloudCookManifest(const PointCloudCookManifest& manifest)
    {
        u64 hash = FnvOffset;
        hash = HashString(hash, manifest.magic);
        hash = HashCombine(hash, manifest.version);
        hash = HashCombine(hash, manifest.guid.high);
        hash = HashCombine(hash, manifest.guid.low);
        hash = HashString(hash, manifest.sourcePath.generic_string());
        hash = HashCombine(hash, static_cast<u64>(manifest.sourceFormat));
        hash = HashCombine(hash, manifest.sourcePointCount);
        hash = HashCombine(hash, manifest.lodLevels);
        hash = HashCombine(hash, manifest.totalPayloadBytes);
        hash = HashCombine(hash, manifest.layout.bytesPerPoint);
        hash = HashCombine(hash, manifest.layout.streamCount);
        hash = HashCombine(hash, manifest.layout.packedAttributeMask);
        for (const PointCloudCookPageRecord& page : manifest.pages)
        {
            hash = HashCombine(hash, page.payloadHash);
        }
        return hash;
    }

    std::string ToDebugString(const PointCloudSourceDesc& source)
    {
        std::ostringstream out;
        out << "source format=" << ToString(source.format)
            << " path='" << source.sourcePath.generic_string() << "'"
            << " points=" << source.estimatedPointCount
            << " streamed=" << (source.streamedReader ? "true" : "false")
            << " rgb=" << (source.hasRgb ? "true" : "false")
            << " intensity=" << (source.hasIntensity ? "true" : "false")
            << " class=" << (source.hasClassification ? "true" : "false")
            << " georef=" << (source.georeferenced ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PointCloudAttributeLayoutElement& element)
    {
        std::ostringstream out;
        out << ToString(element.semantic)
            << " stream=" << ToString(element.stream)
            << " format=" << ToString(element.format) << "x" << element.components
            << " offset=" << element.byteOffset
            << " size=" << element.byteSize
            << " normalized=" << (element.normalized ? "true" : "false")
            << " required=" << (element.required ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PointCloudAttributeLayout& layout)
    {
        std::ostringstream out;
        out << "layout streams=" << layout.streamCount
            << " bytesPerPoint=" << layout.bytesPerPoint
            << " mask=0x" << std::hex << layout.packedAttributeMask << std::dec
            << " positionFirst=" << (layout.positionFirst ? "true" : "false")
            << " gpuReady=" << (layout.gpuStorageBufferReady ? "true" : "false")
            << " strides=[" << layout.streamStrides[0] << "," << layout.streamStrides[1] << "," << layout.streamStrides[2] << "," << layout.streamStrides[3] << "]";
        return out.str();
    }

    std::string ToDebugString(const PointCloudCookPageRecord& page)
    {
        std::ostringstream out;
        out << ToDebugString(page.page)
            << " lod=" << page.lod
            << " points=" << page.pointCount
            << " fullPoints=" << page.fullResolutionPointCount
            << " fileOffset=" << page.fileOffsetBytes
            << " payload=" << page.payloadBytes
            << " streams=" << page.streamCount
            << " hash=0x" << std::hex << page.payloadHash << std::dec;
        return out.str();
    }

    std::string ToDebugString(const PointCloudCookManifest& manifest)
    {
        std::ostringstream out;
        out << manifest.magic << " v" << manifest.version
            << " format=" << ToString(manifest.sourceFormat)
            << " points=" << manifest.sourcePointCount
            << " pages=" << manifest.pages.size()
            << " lods=" << manifest.lodLevels
            << " payload=" << manifest.totalPayloadBytes
            << " rootHash=0x" << std::hex << manifest.rootHash << std::dec
            << " streamable=" << (manifest.streamable ? "true" : "false")
            << " externalSdkOptional=" << (manifest.externalSdkOptional ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PointCloudCookValidationReport& report)
    {
        std::ostringstream out;
        out << "cook validation " << (report.ok ? "ok" : "failed")
            << " warnings=" << report.warnings.size()
            << " errors=" << report.errors.size();
        for (const std::string& warning : report.warnings)
        {
            out << " warning='" << warning << "'";
        }
        for (const std::string& error : report.errors)
        {
            out << " error='" << error << "'";
        }
        return out.str();
    }

    std::string ToDebugString(const PointCloudCookReport& report)
    {
        std::ostringstream out;
        out << report.summary << " " << ToDebugString(report.manifest);
        return out.str();
    }

    PointCloudCookProbeResult BuildPointCloudCookProbe()
    {
        PointCloudCookProbeResult probe{};

        PointCloudAssetDesc asset{};
        asset.guid = BuildAssetGuidFromNormalizedPath("builtin:scan/city_block_cooked_laz");
        asset.sourcePath = "assets/scans/city_block_utm32.laz";
        asset.debugName = "city_block_utm32_lidar";
        asset.pointCount = 18200000ull;
        asset.chunkTargetPoints = 240000;
        asset.localBounds = MakeAABB3({-4096.0f, -64.0f, -4096.0f}, {4096.0f, 720.0f, 4096.0f});
        asset.geo = MakeWgs84GeoReference({4326});
        asset.geo.coordinateSystem = GeoCoordinateSystem::ProjectedMeters;
        asset.geo.epsg = {32632};
        asset.georeferenced = true;
        asset.streaming.maxResidentPages = 64;
        asset.streaming.maxResidentBytes = 96ull * 1024ull * 1024ull;
        asset.material.colorMode = PointCloudColorMode::Intensity;
        asset.material.excludeUnusedStreamedAttributes = true;
        asset.attributes = {
            {PointCloudAttributeSemantic::Position, PointCloudAttributeFormat::Float32, 3, "position", true, true},
            {PointCloudAttributeSemantic::Rgb, PointCloudAttributeFormat::UInt8, 3, "rgb", true, false},
            {PointCloudAttributeSemantic::Intensity, PointCloudAttributeFormat::UInt16, 1, "intensity", true, false},
            {PointCloudAttributeSemantic::Classification, PointCloudAttributeFormat::UInt8, 1, "classification", true, false},
            {PointCloudAttributeSemantic::ReturnIndex, PointCloudAttributeFormat::UInt8, 1, "return_index", true, false},
            {PointCloudAttributeSemantic::Height, PointCloudAttributeFormat::Float32, 1, "height", true, false},
            {PointCloudAttributeSemantic::ScanAngle, PointCloudAttributeFormat::Float32, 1, "scan_angle", true, false}
        };

        PointCloudSourceDesc source = BuildPointCloudSourceDescFromAsset(asset);
        PointCloudCookPolicy policy{};
        policy.chunkTargetPoints = 240000;
        policy.lodLevels = 5;
        policy.lodDownsampleFactor = 4;
        policy.maxChunkPayloadBytes = 3ull * 1024ull * 1024ull;
        policy.compression = PointCloudCompressionMode::QuantizedDelta;
        policy.defaultMaterial = asset.material;
        policy.quantizePositions = true;
        policy.buildLodHierarchy = true;
        policy.includePageHashes = true;
        policy.includeDebugBounds = true;
        policy.deterministicPageOrder = true;
        policy.allowExternalSdkBridge = false;

        probe.report = BuildPointCloudCookReport(source, asset, policy);
        const bool detectsLaz = DetectPointCloudSourceFormat(asset.sourcePath) == PointCloudSourceFormat::Laz;
        const bool hasEviction = probe.report.outOfCore.spilledChunks >= 4;
        const bool selectedAttributes = (probe.report.streamingPlan.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Intensity)) != 0 &&
            (probe.report.streamingPlan.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Rgb)) == 0;
        probe.ok = probe.report.ok && detectsLaz && hasEviction && selectedAttributes;

        std::ostringstream out;
        out << (probe.ok ? "[ ok ] " : "[fail] ")
            << "pointcloud cook probe "
            << "format=" << ToString(source.format)
            << " pages=" << probe.report.manifest.pages.size()
            << " lods=" << probe.report.manifest.lodLevels
            << " payload=" << probe.report.manifest.totalPayloadBytes
            << " streams=" << probe.report.manifest.layout.streamCount
            << " streamMask=0x" << std::hex << probe.report.streamingPlan.requestedAttributeMask << std::dec
            << " rootHash=0x" << std::hex << probe.report.manifest.rootHash << std::dec;
        probe.summary = out.str();
        return probe;
    }

    std::string BuildPointCloudCookProbeSummary()
    {
        return BuildPointCloudCookProbe().summary;
    }
}
