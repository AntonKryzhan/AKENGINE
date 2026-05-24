#include <AK/PointCloud/PointCloud.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace AK
{
    namespace
    {
        double DistanceSquared(Vec3 a, Vec3 b)
        {
            const double dx = static_cast<double>(a.x) - static_cast<double>(b.x);
            const double dy = static_cast<double>(a.y) - static_cast<double>(b.y);
            const double dz = static_cast<double>(a.z) - static_cast<double>(b.z);
            return dx * dx + dy * dy + dz * dz;
        }

        bool HasAttribute(const PointCloudAssetDesc& asset, PointCloudAttributeSemantic semantic)
        {
            for (const PointCloudAttributeDesc& attribute : asset.attributes)
            {
                if (attribute.semantic == semantic)
                {
                    return true;
                }
            }
            return false;
        }

        float LerpFloat(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        void Merge(PointCloudValidationReport& into, const GeoValidationReport& from)
        {
            into.ok = into.ok && from.ok;
            into.warnings.insert(into.warnings.end(), from.warnings.begin(), from.warnings.end());
            into.errors.insert(into.errors.end(), from.errors.begin(), from.errors.end());
        }
    }

    const char* ToString(PointCloudAttributeSemantic semantic)
    {
        switch (semantic)
        {
            case PointCloudAttributeSemantic::Position:
                return "position";
            case PointCloudAttributeSemantic::Rgb:
                return "rgb";
            case PointCloudAttributeSemantic::Intensity:
                return "intensity";
            case PointCloudAttributeSemantic::Classification:
                return "classification";
            case PointCloudAttributeSemantic::Height:
                return "height";
            case PointCloudAttributeSemantic::Normal:
                return "normal";
            case PointCloudAttributeSemantic::ReturnIndex:
                return "return-index";
            case PointCloudAttributeSemantic::ScanAngle:
                return "scan-angle";
            case PointCloudAttributeSemantic::Custom0:
                return "custom0";
            case PointCloudAttributeSemantic::Custom1:
                return "custom1";
            default:
                return "unknown";
        }
    }

    const char* ToString(PointCloudAttributeFormat format)
    {
        switch (format)
        {
            case PointCloudAttributeFormat::UInt8:
                return "u8";
            case PointCloudAttributeFormat::UInt16:
                return "u16";
            case PointCloudAttributeFormat::UInt32:
                return "u32";
            case PointCloudAttributeFormat::Float32:
                return "f32";
            case PointCloudAttributeFormat::Float64:
                return "f64";
            default:
                return "unknown";
        }
    }

    const char* ToString(PointCloudPageState state)
    {
        switch (state)
        {
            case PointCloudPageState::Unloaded:
                return "unloaded";
            case PointCloudPageState::Queued:
                return "queued";
            case PointCloudPageState::Resident:
                return "resident";
            case PointCloudPageState::Evictable:
                return "evictable";
            default:
                return "unknown";
        }
    }

    const char* ToString(PointCloudColorMode mode)
    {
        switch (mode)
        {
            case PointCloudColorMode::Rgb:
                return "rgb";
            case PointCloudColorMode::Intensity:
                return "intensity";
            case PointCloudColorMode::Classification:
                return "classification";
            case PointCloudColorMode::Height:
                return "height";
            case PointCloudColorMode::Solid:
                return "solid";
            case PointCloudColorMode::Custom:
                return "custom";
            default:
                return "unknown";
        }
    }

    u32 BytesPerComponent(PointCloudAttributeFormat format)
    {
        switch (format)
        {
            case PointCloudAttributeFormat::UInt8:
                return 1;
            case PointCloudAttributeFormat::UInt16:
                return 2;
            case PointCloudAttributeFormat::UInt32:
            case PointCloudAttributeFormat::Float32:
                return 4;
            case PointCloudAttributeFormat::Float64:
                return 8;
            default:
                return 0;
        }
    }

    u32 BytesPerAttribute(const PointCloudAttributeDesc& attribute)
    {
        return BytesPerComponent(attribute.format) * attribute.components;
    }

    u64 AttributeBit(PointCloudAttributeSemantic semantic)
    {
        const u32 bit = static_cast<u32>(semantic);
        return bit < 63 ? (1ull << bit) : 0ull;
    }

    u64 AttributeMaskForColorMode(PointCloudColorMode mode, PointCloudAttributeSemantic customSemantic)
    {
        switch (mode)
        {
            case PointCloudColorMode::Rgb:
                return AttributeBit(PointCloudAttributeSemantic::Rgb);
            case PointCloudColorMode::Intensity:
                return AttributeBit(PointCloudAttributeSemantic::Intensity);
            case PointCloudColorMode::Classification:
                return AttributeBit(PointCloudAttributeSemantic::Classification);
            case PointCloudColorMode::Height:
                return AttributeBit(PointCloudAttributeSemantic::Height);
            case PointCloudColorMode::Custom:
                return AttributeBit(customSemantic);
            case PointCloudColorMode::Solid:
            default:
                return 0;
        }
    }

    u64 BuildPointCloudAttributeMask(const std::vector<PointCloudAttributeDesc>& attributes, bool onlyRequired)
    {
        u64 mask = 0;
        for (const PointCloudAttributeDesc& attribute : attributes)
        {
            if (!onlyRequired || attribute.requiredByDefault)
            {
                mask |= AttributeBit(attribute.semantic);
            }
        }
        return mask;
    }

    u64 BuildPointCloudAttributeRequest(const PointCloudAssetDesc& asset, const PointCloudMaterialPolicy& material)
    {
        const u64 materialMask = AttributeMaskForColorMode(material.colorMode, material.customColorSemantic);
        u64 requested = material.requiredAttributeMask | materialMask | AttributeBit(PointCloudAttributeSemantic::Position);
        if (!material.excludeUnusedStreamedAttributes)
        {
            requested |= BuildPointCloudAttributeMask(asset.attributes, false);
        }
        else
        {
            requested |= BuildPointCloudAttributeMask(asset.attributes, true);
        }
        return requested;
    }

    u32 BytesPerPoint(const PointCloudAssetDesc& asset, u64 attributeMask)
    {
        u32 bytes = 0;
        for (const PointCloudAttributeDesc& attribute : asset.attributes)
        {
            if ((attributeMask & AttributeBit(attribute.semantic)) != 0)
            {
                bytes += BytesPerAttribute(attribute);
            }
        }
        return bytes;
    }

    u64 EstimatePointCloudBytes(const PointCloudAssetDesc& asset, u64 attributeMask)
    {
        return asset.pointCount * static_cast<u64>(BytesPerPoint(asset, attributeMask));
    }

    PointCloudValidationReport ValidatePointCloudAsset(const PointCloudAssetDesc& asset)
    {
        PointCloudValidationReport report{};
        if (!asset.guid.IsValid())
        {
            report.warnings.push_back("point cloud asset GUID is not valid yet");
        }
        if (asset.pointCount == 0)
        {
            report.ok = false;
            report.errors.push_back("point cloud must contain at least one point");
        }
        if (asset.chunkTargetPoints == 0)
        {
            report.ok = false;
            report.errors.push_back("point cloud chunk target must be non-zero");
        }
        if (!IsValid(asset.localBounds))
        {
            report.ok = false;
            report.errors.push_back("point cloud local bounds are invalid");
        }
        if (asset.attributes.empty())
        {
            report.ok = false;
            report.errors.push_back("point cloud must declare at least one attribute");
        }
        if (!HasAttribute(asset, PointCloudAttributeSemantic::Position))
        {
            report.ok = false;
            report.errors.push_back("point cloud must contain a position attribute");
        }
        if (asset.streaming.attributeStreaming && asset.material.excludeUnusedStreamedAttributes)
        {
            report.warnings.push_back("unused streamed attributes may be excluded by material policy");
        }
        if (asset.georeferenced)
        {
            Merge(report, ValidateGeoReference(asset.geo));
        }
        if (BytesPerPoint(asset, BuildPointCloudAttributeMask(asset.attributes, false)) == 0)
        {
            report.ok = false;
            report.errors.push_back("point cloud has zero bytes per point");
        }
        return report;
    }

    std::vector<PointCloudChunkDesc> BuildPointCloudChunks(const PointCloudAssetDesc& asset)
    {
        std::vector<PointCloudChunkDesc> chunks;
        if (asset.pointCount == 0 || asset.chunkTargetPoints == 0)
        {
            return chunks;
        }

        const u64 fullMask = BuildPointCloudAttributeMask(asset.attributes, false);
        const u32 fullBytesPerPoint = BytesPerPoint(asset, fullMask);
        const u32 chunkCount = static_cast<u32>((asset.pointCount + asset.chunkTargetPoints - 1) / asset.chunkTargetPoints);
        chunks.reserve(chunkCount);

        const Vec3 minBounds = asset.localBounds.min;
        const Vec3 maxBounds = asset.localBounds.max;
        for (u32 i = 0; i < chunkCount; ++i)
        {
            const u64 offset = static_cast<u64>(i) * static_cast<u64>(asset.chunkTargetPoints);
            const u64 remaining = asset.pointCount - offset;
            const u32 points = static_cast<u32>(std::min<u64>(remaining, asset.chunkTargetPoints));
            const float t0 = static_cast<float>(i) / static_cast<float>(std::max<u32>(1, chunkCount));
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(std::max<u32>(1, chunkCount));

            PointCloudChunkDesc chunk{};
            chunk.page.x = static_cast<i64>(i);
            chunk.page.y = 0;
            chunk.page.z = 0;
            chunk.page.lod = 0;
            chunk.page.ordinal = i;
            chunk.pointOffset = offset;
            chunk.pointCount = points;
            chunk.attributeMask = fullMask;
            chunk.payloadBytes = static_cast<u64>(points) * static_cast<u64>(fullBytesPerPoint);
            chunk.selectedPayloadBytes = chunk.payloadBytes;
            chunk.bounds = MakeAABB3(
                {LerpFloat(minBounds.x, maxBounds.x, t0), minBounds.y, minBounds.z},
                {LerpFloat(minBounds.x, maxBounds.x, t1), maxBounds.y, maxBounds.z});
            chunks.push_back(chunk);
        }

        return chunks;
    }

    PointCloudStreamingPlan BuildPointCloudStreamingPlan(const PointCloudAssetDesc& asset, Vec3 cameraLocalMeters, u64 streamingBudgetBytes)
    {
        PointCloudStreamingPlan plan{};
        std::vector<PointCloudChunkDesc> chunks = BuildPointCloudChunks(asset);
        plan.requestedAttributeMask = BuildPointCloudAttributeRequest(asset, asset.material);
        const u32 requestedBytesPerPoint = BytesPerPoint(asset, plan.requestedAttributeMask);
        const u32 fullBytesPerPoint = BytesPerPoint(asset, BuildPointCloudAttributeMask(asset.attributes, false));

        std::sort(chunks.begin(), chunks.end(), [cameraLocalMeters](const PointCloudChunkDesc& a, const PointCloudChunkDesc& b)
        {
            const double da = DistanceSquared(Center(a.bounds), cameraLocalMeters);
            const double db = DistanceSquared(Center(b.bounds), cameraLocalMeters);
            if (da != db)
            {
                return da < db;
            }
            return a.page.ordinal < b.page.ordinal;
        });

        for (PointCloudChunkDesc chunk : chunks)
        {
            const u64 selectedBytes = static_cast<u64>(chunk.pointCount) * static_cast<u64>(requestedBytesPerPoint);
            const u64 fullBytes = static_cast<u64>(chunk.pointCount) * static_cast<u64>(fullBytesPerPoint);
            if (streamingBudgetBytes > 0 && plan.requestedPayloadBytes + selectedBytes > streamingBudgetBytes)
            {
                ++plan.skippedByBudget;
                plan.clippedByBudget = true;
                continue;
            }

            chunk.attributeMask = plan.requestedAttributeMask;
            chunk.selectedPayloadBytes = selectedBytes;
            chunk.payloadBytes = fullBytes;
            chunk.state = PointCloudPageState::Queued;
            plan.requestedPayloadBytes += selectedBytes;
            plan.fullPayloadBytes += fullBytes;
            plan.selectedPointCount += chunk.pointCount;
            plan.chunks.push_back(chunk);
        }

        plan.summary = ToDebugString(plan);
        return plan;
    }

    std::string ToDebugString(PointCloudPageId id)
    {
        std::ostringstream out;
        out << "page(" << id.x << "," << id.y << "," << id.z << ",lod=" << id.lod << ",ord=" << id.ordinal << ")";
        return out.str();
    }

    std::string ToDebugString(const PointCloudAttributeDesc& attribute)
    {
        std::ostringstream out;
        out << ToString(attribute.semantic)
            << ":" << ToString(attribute.format)
            << "x" << attribute.components
            << " bytes=" << BytesPerAttribute(attribute)
            << " streamable=" << (attribute.streamable ? "true" : "false")
            << " required=" << (attribute.requiredByDefault ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PointCloudValidationReport& report)
    {
        std::ostringstream out;
        out << "pointcloud validation " << (report.ok ? "ok" : "failed")
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

    std::string ToDebugString(const PointCloudChunkDesc& chunk)
    {
        std::ostringstream out;
        out << ToDebugString(chunk.page)
            << " points=" << chunk.pointCount
            << " offset=" << chunk.pointOffset
            << " fullBytes=" << chunk.payloadBytes
            << " selectedBytes=" << chunk.selectedPayloadBytes
            << " state=" << ToString(chunk.state)
            << " bounds=" << ToDebugString(chunk.bounds.min, 1) << ".." << ToDebugString(chunk.bounds.max, 1);
        return out.str();
    }

    std::string ToDebugString(const PointCloudStreamingPlan& plan)
    {
        std::ostringstream out;
        out << "stream chunks=" << plan.chunks.size()
            << " points=" << plan.selectedPointCount
            << " requestedBytes=" << plan.requestedPayloadBytes
            << " fullBytes=" << plan.fullPayloadBytes
            << " attrMask=0x" << std::hex << plan.requestedAttributeMask << std::dec
            << " skipped=" << plan.skippedByBudget
            << " clipped=" << (plan.clippedByBudget ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PointCloudAssetDesc& asset)
    {
        std::ostringstream out;
        out << "pointcloud '" << asset.debugName << "' points=" << asset.pointCount
            << " chunkTarget=" << asset.chunkTargetPoints
            << " attrs=" << asset.attributes.size()
            << " bytesAll=" << EstimatePointCloudBytes(asset, BuildPointCloudAttributeMask(asset.attributes, false))
            << " georef=" << (asset.georeferenced ? "true" : "false")
            << " render=" << (asset.render.useVulkanStorageBuffers ? "vulkan-storage" : "cpu")
            << " material=" << ToString(asset.material.colorMode);
        return out.str();
    }

    PointCloudProbeResult BuildPointCloudProbe()
    {
        PointCloudProbeResult probe{};
        probe.geo = BuildGeoDataProbe();

        probe.asset.guid = BuildAssetGuidFromNormalizedPath("builtin:scan/synthetic_lidar");
        probe.asset.sourcePath = "assets/scans/synthetic_lidar.laz";
        probe.asset.debugName = "synthetic_lidar_city_block";
        probe.asset.pointCount = 12500000ull;
        probe.asset.chunkTargetPoints = 500000;
        probe.asset.localBounds = MakeAABB3({-2048.0f, -32.0f, -2048.0f}, {2048.0f, 620.0f, 2048.0f});
        probe.asset.geo = probe.geo.reference;
        probe.asset.georeferenced = true;
        probe.asset.cooked = true;
        probe.asset.streaming.maxResidentPages = 3;
        probe.asset.streaming.maxResidentBytes = 64ull * 1024ull * 1024ull;
        probe.asset.material.colorMode = PointCloudColorMode::Classification;
        probe.asset.material.excludeUnusedStreamedAttributes = true;
        probe.asset.attributes = {
            {PointCloudAttributeSemantic::Position, PointCloudAttributeFormat::Float32, 3, "position", true, true},
            {PointCloudAttributeSemantic::Rgb, PointCloudAttributeFormat::UInt8, 3, "rgb", true, false},
            {PointCloudAttributeSemantic::Intensity, PointCloudAttributeFormat::UInt16, 1, "intensity", true, false},
            {PointCloudAttributeSemantic::Classification, PointCloudAttributeFormat::UInt8, 1, "classification", true, false},
            {PointCloudAttributeSemantic::Height, PointCloudAttributeFormat::Float32, 1, "height", true, false}
        };

        probe.validation = ValidatePointCloudAsset(probe.asset);
        probe.chunks = BuildPointCloudChunks(probe.asset);
        probe.streamingPlan = BuildPointCloudStreamingPlan(probe.asset, {0.0f, 128.0f, 0.0f}, 8ull * 1024ull * 1024ull);

        OutOfCorePolicy policy{};
        policy.chunkPayloadBytes = 256;
        policy.maxResidentChunks = 3;
        policy.spillFilePath = std::filesystem::temp_directory_path() / "ak_pointcloud_probe.spill";
        VirtualChunkCache cache(policy);
        std::vector<OutOfCoreChunkId> ids;
        for (u32 i = 0; i < 7; ++i)
        {
            std::vector<u8> bytes(256, static_cast<u8>(31 + i));
            ids.push_back(cache.AddChunk(bytes, i + 1));
        }
        const OutOfCoreReadResult readBack = cache.ReadChunk(ids.front(), 20);
        probe.outOfCore = cache.Stats();

        const bool maskExcludesHeight = (probe.streamingPlan.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Height)) == 0;
        const bool hasPositionAndClassification =
            (probe.streamingPlan.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Position)) != 0 &&
            (probe.streamingPlan.requestedAttributeMask & AttributeBit(PointCloudAttributeSemantic::Classification)) != 0;
        probe.ok = probe.geo.ok && probe.validation.ok && !probe.chunks.empty() && !probe.streamingPlan.chunks.empty() &&
            probe.streamingPlan.clippedByBudget && maskExcludesHeight && hasPositionAndClassification &&
            probe.outOfCore.residentChunks <= 3 && probe.outOfCore.spilledChunks >= 4 && readBack.ok && readBack.bytes.size() == 256;

        std::ostringstream out;
        out << (probe.ok ? "[ ok ] " : "[fail] ")
            << "massive pointcloud / scan streaming foundation "
            << "chunks=" << probe.chunks.size()
            << " selected=" << probe.streamingPlan.chunks.size()
            << " points=" << probe.streamingPlan.selectedPointCount
            << " requestedBytes=" << probe.streamingPlan.requestedPayloadBytes
            << " attrMask=0x" << std::hex << probe.streamingPlan.requestedAttributeMask << std::dec
            << " ooc=" << ToDebugString(probe.outOfCore);
        probe.summary = out.str();
        return probe;
    }

    std::string BuildPointCloudProbeSummary()
    {
        return BuildPointCloudProbe().summary;
    }
}
