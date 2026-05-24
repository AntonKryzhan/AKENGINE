#pragma once

#include <AK/Core/Guid.hpp>
#include <AK/Core/Types.hpp>
#include <AK/World/WorldCoordinates.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AK
{
    enum class WorldCellResidency
    {
        Unloaded,
        Queued,
        Resident,
        Active,
        Evicting
    };

    enum class WorldCellPriority
    {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    struct WorldPartitionConfig final
    {
        double cellSizeMeters = 1024.0;
        i32 activeRadiusCells = 1;
        i32 preloadRadiusCells = 2;
        i32 unloadRadiusCells = 4;
        u32 maxCellsActivatedPerUpdate = 8;
        u32 maxCellsEvictedPerUpdate = 8;
    };

    struct WorldPartitionCellId final
    {
        i64 x = 0;
        i64 z = 0;
        i32 lod = 0;
    };

    struct WorldPartitionCellIdHash final
    {
        std::size_t operator()(WorldPartitionCellId value) const noexcept;
    };

    struct WorldPartitionAssetRef final
    {
        AssetGuid guid{};
        std::string logicalPath;
        u64 estimatedBytes = 0;
    };

    struct WorldPartitionCell final
    {
        WorldPartitionCellId id{};
        WorldCellResidency residency = WorldCellResidency::Unloaded;
        WorldCellPriority priority = WorldCellPriority::Low;
        std::vector<WorldPartitionAssetRef> assets;
        u64 estimatedBytes = 0;
        u64 lastTouchedFrame = 0;
        std::string debugName;
    };

    struct WorldPartitionUpdateResult final
    {
        std::vector<WorldPartitionCellId> queuedLoads;
        std::vector<WorldPartitionCellId> activated;
        std::vector<WorldPartitionCellId> evicted;
        u32 active = 0;
        u32 resident = 0;
        u32 queued = 0;
        u32 unloaded = 0;
        u32 evicting = 0;
        u64 estimatedResidentBytes = 0;
        bool budgetLimited = false;
        std::string summary;
    };

    struct WorldPartitionStats final
    {
        u32 cells = 0;
        u32 active = 0;
        u32 resident = 0;
        u32 queued = 0;
        u32 unloaded = 0;
        u32 evicting = 0;
        u32 assetRefs = 0;
        u64 estimatedResidentBytes = 0;
        WorldPartitionCellId cameraCell{};
        std::string summary;
    };

    struct WorldPartitionProbeResult final
    {
        bool ok = false;
        WorldPartitionConfig config{};
        WorldPosition camera{};
        WorldPartitionUpdateResult firstUpdate{};
        WorldPartitionUpdateResult farUpdate{};
        WorldPartitionStats stats{};
        std::string summary;
    };

    class WorldPartition final
    {
    public:
        explicit WorldPartition(WorldPartitionConfig config = {});

        void Clear();
        WorldPartitionCell& EnsureCell(WorldPartitionCellId id);
        bool AddAssetRef(WorldPartitionCellId id, WorldPartitionAssetRef asset);
        bool SetResidency(WorldPartitionCellId id, WorldCellResidency residency);

        WorldPartitionUpdateResult Update(WorldPosition camera, u64 frameIndex);
        [[nodiscard]] WorldPartitionStats Stats(WorldPosition camera) const;
        [[nodiscard]] const WorldPartitionCell* FindCell(WorldPartitionCellId id) const;
        [[nodiscard]] const std::vector<WorldPartitionCell>& Cells() const;
        [[nodiscard]] const WorldPartitionConfig& Config() const;

    private:
        [[nodiscard]] bool IsWithinRadius(WorldPartitionCellId cell, WorldPartitionCellId center, i32 radius) const;
        [[nodiscard]] WorldCellPriority ComputePriority(WorldPartitionCellId cell, WorldPartitionCellId center) const;
        [[nodiscard]] u64 ComputeResidentBytes() const;
        void RefreshCellSummary(WorldPartitionCell& cell) const;
        void RefreshUpdateSummary(WorldPartitionUpdateResult& result) const;

        WorldPartitionConfig mConfig{};
        std::vector<WorldPartitionCell> mCells;
        std::unordered_map<WorldPartitionCellId, usize, WorldPartitionCellIdHash> mCellIndex;
    };

    bool operator==(WorldPartitionCellId a, WorldPartitionCellId b);
    bool operator!=(WorldPartitionCellId a, WorldPartitionCellId b);

    WorldPartitionCellId MakeWorldPartitionCellId(WorldPosition position, const WorldPartitionConfig& config = {});
    WorldPosition MakeWorldPartitionCellCenter(WorldPartitionCellId id, const WorldPartitionConfig& config = {});
    i64 ManhattanDistance(WorldPartitionCellId a, WorldPartitionCellId b);
    u64 EstimateCellAssetBytes(const WorldPartitionCell& cell);

    std::string_view ToString(WorldCellResidency residency);
    std::string_view ToString(WorldCellPriority priority);
    std::string ToDebugString(WorldPartitionCellId id);
    std::string ToDebugString(const WorldPartitionCell& cell);
    std::string ToDebugString(const WorldPartitionStats& stats);
    std::string ToDebugString(const WorldPartitionUpdateResult& result);

    WorldPartitionProbeResult BuildWorldPartitionProbe();
    std::string BuildWorldPartitionProbeSummary();
}
