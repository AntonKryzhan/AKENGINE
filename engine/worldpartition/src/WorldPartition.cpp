#include <AK/WorldPartition/WorldPartition.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        WorldPartitionConfig SanitizeConfig(WorldPartitionConfig config)
        {
            if (config.cellSizeMeters <= 0.0 || !std::isfinite(config.cellSizeMeters))
            {
                config.cellSizeMeters = DefaultWorldCellSizeMeters;
            }
            config.activeRadiusCells = std::max<i32>(0, config.activeRadiusCells);
            config.preloadRadiusCells = std::max(config.activeRadiusCells, config.preloadRadiusCells);
            config.unloadRadiusCells = std::max(config.preloadRadiusCells, config.unloadRadiusCells);
            config.maxCellsActivatedPerUpdate = std::max<u32>(1, config.maxCellsActivatedPerUpdate);
            config.maxCellsEvictedPerUpdate = std::max<u32>(1, config.maxCellsEvictedPerUpdate);
            return config;
        }
    }

    bool operator==(WorldPartitionCellId a, WorldPartitionCellId b)
    {
        return a.x == b.x && a.z == b.z && a.lod == b.lod;
    }

    bool operator!=(WorldPartitionCellId a, WorldPartitionCellId b)
    {
        return !(a == b);
    }


    std::size_t WorldPartitionCellIdHash::operator()(WorldPartitionCellId value) const noexcept
    {
        const std::uint64_t x = static_cast<std::uint64_t>(value.x);
        const std::uint64_t z = static_cast<std::uint64_t>(value.z);
        const std::uint64_t lod = static_cast<std::uint64_t>(static_cast<std::uint32_t>(value.lod));
        std::uint64_t h = x + 0x9e3779b97f4a7c15ull;
        h ^= z + 0x9e3779b97f4a7c15ull + (h << 6u) + (h >> 2u);
        h ^= lod + 0x9e3779b97f4a7c15ull + (h << 6u) + (h >> 2u);
        return std::hash<std::uint64_t>{}(h);
    }

    WorldPartition::WorldPartition(WorldPartitionConfig config)
        : mConfig(SanitizeConfig(config))
    {
    }

    void WorldPartition::Clear()
    {
        mCells.clear();
        mCellIndex.clear();
    }

    WorldPartitionCell& WorldPartition::EnsureCell(WorldPartitionCellId id)
    {
        const auto existing = mCellIndex.find(id);
        if (existing != mCellIndex.end())
        {
            return mCells[existing->second];
        }

        WorldPartitionCell cell{};
        cell.id = id;
        RefreshCellSummary(cell);
        const usize index = mCells.size();
        mCells.push_back(std::move(cell));
        mCellIndex[id] = index;
        return mCells.back();
    }

    bool WorldPartition::AddAssetRef(WorldPartitionCellId id, WorldPartitionAssetRef asset)
    {
        if (!asset.guid.IsValid() && asset.logicalPath.empty())
        {
            return false;
        }

        WorldPartitionCell& cell = EnsureCell(id);
        cell.estimatedBytes += asset.estimatedBytes;
        cell.assets.push_back(std::move(asset));
        RefreshCellSummary(cell);
        return true;
    }

    bool WorldPartition::SetResidency(WorldPartitionCellId id, WorldCellResidency residency)
    {
        WorldPartitionCell* cell = nullptr;
        if (const auto it = mCellIndex.find(id); it != mCellIndex.end())
        {
            cell = &mCells[it->second];
        }
        if (!cell)
        {
            return false;
        }

        cell->residency = residency;
        RefreshCellSummary(*cell);
        return true;
    }

    WorldPartitionUpdateResult WorldPartition::Update(WorldPosition camera, u64 frameIndex)
    {
        WorldPartitionUpdateResult result{};
        const WorldPartitionCellId center = MakeWorldPartitionCellId(camera, mConfig);

        for (i32 z = -mConfig.preloadRadiusCells; z <= mConfig.preloadRadiusCells; ++z)
        {
            for (i32 x = -mConfig.preloadRadiusCells; x <= mConfig.preloadRadiusCells; ++x)
            {
                const WorldPartitionCellId id{center.x + x, center.z + z, 0};
                WorldPartitionCell& cell = EnsureCell(id);
                cell.lastTouchedFrame = frameIndex;
                cell.priority = ComputePriority(id, center);

                if (IsWithinRadius(id, center, mConfig.activeRadiusCells))
                {
                    if (cell.residency != WorldCellResidency::Active)
                    {
                        if (result.activated.size() < mConfig.maxCellsActivatedPerUpdate)
                        {
                            cell.residency = WorldCellResidency::Active;
                            result.activated.push_back(id);
                        }
                        else
                        {
                            cell.residency = WorldCellResidency::Queued;
                            result.budgetLimited = true;
                            result.queuedLoads.push_back(id);
                        }
                    }
                }
                else if (cell.residency == WorldCellResidency::Unloaded)
                {
                    cell.residency = WorldCellResidency::Queued;
                    result.queuedLoads.push_back(id);
                }
                else if (cell.residency == WorldCellResidency::Evicting)
                {
                    cell.residency = WorldCellResidency::Queued;
                    result.queuedLoads.push_back(id);
                }

                RefreshCellSummary(cell);
            }
        }

        for (WorldPartitionCell& cell : mCells)
        {
            if (IsWithinRadius(cell.id, center, mConfig.unloadRadiusCells))
            {
                continue;
            }

            if (cell.residency == WorldCellResidency::Unloaded)
            {
                continue;
            }

            if (result.evicted.size() < mConfig.maxCellsEvictedPerUpdate)
            {
                cell.residency = WorldCellResidency::Unloaded;
                result.evicted.push_back(cell.id);
            }
            else
            {
                cell.residency = WorldCellResidency::Evicting;
                result.budgetLimited = true;
            }
            RefreshCellSummary(cell);
        }

        for (const WorldPartitionCell& cell : mCells)
        {
            switch (cell.residency)
            {
                case WorldCellResidency::Active:
                    ++result.active;
                    result.estimatedResidentBytes += cell.estimatedBytes;
                    break;
                case WorldCellResidency::Resident:
                    ++result.resident;
                    result.estimatedResidentBytes += cell.estimatedBytes;
                    break;
                case WorldCellResidency::Queued:
                    ++result.queued;
                    break;
                case WorldCellResidency::Evicting:
                    ++result.evicting;
                    break;
                case WorldCellResidency::Unloaded:
                    ++result.unloaded;
                    break;
            }
        }

        RefreshUpdateSummary(result);
        return result;
    }

    WorldPartitionStats WorldPartition::Stats(WorldPosition camera) const
    {
        WorldPartitionStats stats{};
        stats.cameraCell = MakeWorldPartitionCellId(camera, mConfig);
        stats.cells = static_cast<u32>(mCells.size());

        for (const WorldPartitionCell& cell : mCells)
        {
            stats.assetRefs += static_cast<u32>(cell.assets.size());
            switch (cell.residency)
            {
                case WorldCellResidency::Active:
                    ++stats.active;
                    stats.estimatedResidentBytes += cell.estimatedBytes;
                    break;
                case WorldCellResidency::Resident:
                    ++stats.resident;
                    stats.estimatedResidentBytes += cell.estimatedBytes;
                    break;
                case WorldCellResidency::Queued:
                    ++stats.queued;
                    break;
                case WorldCellResidency::Evicting:
                    ++stats.evicting;
                    break;
                case WorldCellResidency::Unloaded:
                    ++stats.unloaded;
                    break;
            }
        }

        stats.summary = ToDebugString(stats);
        return stats;
    }

    const WorldPartitionCell* WorldPartition::FindCell(WorldPartitionCellId id) const
    {
        const auto it = mCellIndex.find(id);
        return it == mCellIndex.end() ? nullptr : &mCells[it->second];
    }

    const std::vector<WorldPartitionCell>& WorldPartition::Cells() const
    {
        return mCells;
    }

    const WorldPartitionConfig& WorldPartition::Config() const
    {
        return mConfig;
    }

    bool WorldPartition::IsWithinRadius(WorldPartitionCellId cell, WorldPartitionCellId center, i32 radius) const
    {
        return std::llabs(cell.x - center.x) <= radius && std::llabs(cell.z - center.z) <= radius && cell.lod == center.lod;
    }

    WorldCellPriority WorldPartition::ComputePriority(WorldPartitionCellId cell, WorldPartitionCellId center) const
    {
        const i64 distance = ManhattanDistance(cell, center);
        if (distance == 0)
        {
            return WorldCellPriority::Critical;
        }
        if (distance <= mConfig.activeRadiusCells)
        {
            return WorldCellPriority::High;
        }
        if (distance <= mConfig.preloadRadiusCells)
        {
            return WorldCellPriority::Normal;
        }
        return WorldCellPriority::Low;
    }

    u64 WorldPartition::ComputeResidentBytes() const
    {
        u64 bytes = 0;
        for (const WorldPartitionCell& cell : mCells)
        {
            if (cell.residency == WorldCellResidency::Resident || cell.residency == WorldCellResidency::Active)
            {
                bytes += cell.estimatedBytes;
            }
        }
        return bytes;
    }

    void WorldPartition::RefreshCellSummary(WorldPartitionCell& cell) const
    {
        cell.estimatedBytes = EstimateCellAssetBytes(cell);
        cell.debugName = ToDebugString(cell);
    }

    void WorldPartition::RefreshUpdateSummary(WorldPartitionUpdateResult& result) const
    {
        result.summary = ToDebugString(result);
    }

    WorldPartitionCellId MakeWorldPartitionCellId(WorldPosition position, const WorldPartitionConfig& config)
    {
        const WorldPartitionConfig safeConfig = SanitizeConfig(config);
        const WorldPosition normalized = NormalizeWorldPosition(position, safeConfig.cellSizeMeters);
        return {normalized.cell.x, normalized.cell.z, 0};
    }

    WorldPosition MakeWorldPartitionCellCenter(WorldPartitionCellId id, const WorldPartitionConfig& config)
    {
        const WorldPartitionConfig safeConfig = SanitizeConfig(config);
        WorldPosition position{};
        position.cell.x = id.x;
        position.cell.y = 0;
        position.cell.z = id.z;
        position.localX = 0.0;
        position.localY = 0.0;
        position.localZ = 0.0;
        return NormalizeWorldPosition(position, safeConfig.cellSizeMeters);
    }

    i64 ManhattanDistance(WorldPartitionCellId a, WorldPartitionCellId b)
    {
        if (a.lod != b.lod)
        {
            return std::llabs(static_cast<i64>(a.lod) - static_cast<i64>(b.lod)) + std::llabs(a.x - b.x) + std::llabs(a.z - b.z);
        }
        return std::llabs(a.x - b.x) + std::llabs(a.z - b.z);
    }

    u64 EstimateCellAssetBytes(const WorldPartitionCell& cell)
    {
        u64 bytes = 0;
        for (const WorldPartitionAssetRef& asset : cell.assets)
        {
            bytes += asset.estimatedBytes;
        }
        return bytes;
    }

    std::string_view ToString(WorldCellResidency residency)
    {
        switch (residency)
        {
            case WorldCellResidency::Unloaded: return "unloaded";
            case WorldCellResidency::Queued: return "queued";
            case WorldCellResidency::Resident: return "resident";
            case WorldCellResidency::Active: return "active";
            case WorldCellResidency::Evicting: return "evicting";
        }
        return "unknown";
    }

    std::string_view ToString(WorldCellPriority priority)
    {
        switch (priority)
        {
            case WorldCellPriority::Low: return "low";
            case WorldCellPriority::Normal: return "normal";
            case WorldCellPriority::High: return "high";
            case WorldCellPriority::Critical: return "critical";
        }
        return "unknown";
    }

    std::string ToDebugString(WorldPartitionCellId id)
    {
        std::ostringstream out;
        out << "cell(" << id.x << "," << id.z << ",lod=" << id.lod << ")";
        return out.str();
    }

    std::string ToDebugString(const WorldPartitionCell& cell)
    {
        std::ostringstream out;
        out << ToDebugString(cell.id)
            << " " << ToString(cell.residency)
            << " priority=" << ToString(cell.priority)
            << " assets=" << cell.assets.size()
            << " bytes=" << cell.estimatedBytes
            << " touched=" << cell.lastTouchedFrame;
        return out.str();
    }

    std::string ToDebugString(const WorldPartitionStats& stats)
    {
        std::ostringstream out;
        out << "partition cells=" << stats.cells
            << " active=" << stats.active
            << " resident=" << stats.resident
            << " queued=" << stats.queued
            << " unloaded=" << stats.unloaded
            << " evicting=" << stats.evicting
            << " refs=" << stats.assetRefs
            << " bytes=" << stats.estimatedResidentBytes
            << " camera=" << ToDebugString(stats.cameraCell);
        return out.str();
    }

    std::string ToDebugString(const WorldPartitionUpdateResult& result)
    {
        std::ostringstream out;
        out << "partition update active=" << result.active
            << " resident=" << result.resident
            << " queued=" << result.queued
            << " unloaded=" << result.unloaded
            << " evicting=" << result.evicting
            << " load+=" << result.queuedLoads.size()
            << " active+=" << result.activated.size()
            << " evict+=" << result.evicted.size()
            << " bytes=" << result.estimatedResidentBytes
            << " limited=" << (result.budgetLimited ? "yes" : "no");
        return out.str();
    }

    WorldPartitionProbeResult BuildWorldPartitionProbe()
    {
        WorldPartitionProbeResult result{};
        result.config.cellSizeMeters = 512.0;
        result.config.activeRadiusCells = 1;
        result.config.preloadRadiusCells = 2;
        result.config.unloadRadiusCells = 3;
        result.config.maxCellsActivatedPerUpdate = 16;
        result.config.maxCellsEvictedPerUpdate = 16;

        WorldPartition partition(result.config);
        for (i64 z = -2; z <= 2; ++z)
        {
            for (i64 x = -2; x <= 2; ++x)
            {
                const WorldPartitionCellId id{x, z, 0};
                WorldPartitionAssetRef asset{};
                asset.guid = BuildAssetGuidFromNormalizedPath("world/cell_" + std::to_string(x) + "_" + std::to_string(z) + ".akcell");
                asset.logicalPath = "world/cell_" + std::to_string(x) + "_" + std::to_string(z) + ".akcell";
                asset.estimatedBytes = 1024u * static_cast<u64>(1 + std::llabs(x) + std::llabs(z));
                partition.AddAssetRef(id, std::move(asset));
            }
        }

        result.camera = MakeWorldPosition(0.0, 0.0, 0.0, result.config.cellSizeMeters);
        result.firstUpdate = partition.Update(result.camera, 1);
        const WorldPosition farCamera = MakeWorldPosition(4096.0, 0.0, 4096.0, result.config.cellSizeMeters);
        result.farUpdate = partition.Update(farCamera, 2);
        result.stats = partition.Stats(farCamera);
        result.ok = result.firstUpdate.active >= 9
            && !result.firstUpdate.queuedLoads.empty()
            && !result.farUpdate.evicted.empty()
            && result.stats.cells >= 25
            && result.stats.cameraCell.x == 8
            && result.stats.cameraCell.z == 8;
        result.summary = std::string("World partition probe: ") + (result.ok ? "ok " : "failed ") + result.stats.summary;
        return result;
    }

    std::string BuildWorldPartitionProbeSummary()
    {
        return BuildWorldPartitionProbe().summary;
    }
}
