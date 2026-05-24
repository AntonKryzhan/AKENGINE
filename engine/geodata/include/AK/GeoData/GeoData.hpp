#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/WorldTopology/WorldTopology.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class GeoCoordinateSystem : u32
    {
        LocalMeters = 0,
        Wgs84Geographic = 1,
        ProjectedMeters = 2,
        PlanetSurface = 3
    };

    enum class GeoHeightSource : u32
    {
        None = 0,
        DemGrid = 1,
        PointCloud = 2,
        Mesh = 3
    };

    enum class GeoRasterFormat : u32
    {
        Unknown = 0,
        R8 = 1,
        R16 = 2,
        R32Float = 3,
        RGB8 = 4,
        RGBA8 = 5
    };

    struct EpsgCode final
    {
        u32 value = 0;

        constexpr bool IsValid() const
        {
            return value != 0;
        }
    };

    struct GeoReference final
    {
        GeoCoordinateSystem coordinateSystem = GeoCoordinateSystem::LocalMeters;
        EpsgCode epsg{};
        double originLatitudeRadians = 0.0;
        double originLongitudeRadians = 0.0;
        double originHeightMeters = 0.0;
        double metersPerUnit = 1.0;
        bool yUp = true;
        bool rightHanded = true;
        bool cameraRelativeRuntime = true;
        PlanetSurfaceConfig planet{};
    };

    struct DemGridDesc final
    {
        u32 width = 0;
        u32 height = 0;
        double sampleSpacingXMeters = 1.0;
        double sampleSpacingZMeters = 1.0;
        double minHeightMeters = 0.0;
        double maxHeightMeters = 0.0;
        GeoHeightSource source = GeoHeightSource::DemGrid;
        GeoReference geo{};
        bool rowMajor = true;
        bool hasNoData = false;
        double noDataValue = -32768.0;
    };

    struct OrthophotoDesc final
    {
        u32 width = 0;
        u32 height = 0;
        GeoRasterFormat format = GeoRasterFormat::RGBA8;
        double metersPerPixelX = 1.0;
        double metersPerPixelY = 1.0;
        GeoReference geo{};
        bool srgb = true;
        bool streamable = true;
    };

    struct GeoImportPolicy final
    {
        bool preserveSourceProjection = true;
        bool convertToEngineMeters = true;
        bool buildPlanetProjection = true;
        bool generateTerrainTiles = true;
        bool generatePointCloudFallback = true;
        u32 maxTileResolution = 2048;
        double maxTileEdgeMeters = 4096.0;
    };

    struct GeoValidationReport final
    {
        bool ok = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct GeoImportEstimate final
    {
        u64 demBytes = 0;
        u64 orthophotoBytes = 0;
        u32 terrainTiles = 0;
        u32 pointCloudFallbackChunks = 0;
        AABB3 localBounds{};
        std::string summary;
    };

    struct GeoDataProbeResult final
    {
        bool ok = false;
        GeoReference reference{};
        DemGridDesc dem{};
        OrthophotoDesc orthophoto{};
        GeoImportEstimate estimate{};
        GeoValidationReport validation{};
        std::string summary;
    };

    const char* ToString(GeoCoordinateSystem system);
    const char* ToString(GeoHeightSource source);
    const char* ToString(GeoRasterFormat format);

    GeoReference MakeLocalMetersGeoReference();
    GeoReference MakeWgs84GeoReference(EpsgCode epsg = {4326});
    GeoReference MakePlanetGeoReference(const PlanetSurfaceConfig& planet = {});

    u32 BytesPerPixel(GeoRasterFormat format);
    u64 EstimateDemBytes(const DemGridDesc& dem);
    u64 EstimateOrthophotoBytes(const OrthophotoDesc& orthophoto);
    AABB3 BuildDemLocalBounds(const DemGridDesc& dem);
    GeoValidationReport ValidateGeoReference(const GeoReference& reference);
    GeoValidationReport ValidateDemGrid(const DemGridDesc& dem);
    GeoValidationReport ValidateOrthophoto(const OrthophotoDesc& orthophoto);
    GeoImportEstimate EstimateGeoImport(const DemGridDesc& dem, const OrthophotoDesc& orthophoto, const GeoImportPolicy& policy = {});

    std::string ToDebugString(const GeoReference& reference);
    std::string ToDebugString(const DemGridDesc& dem);
    std::string ToDebugString(const OrthophotoDesc& orthophoto);
    std::string ToDebugString(const GeoValidationReport& report);
    std::string ToDebugString(const GeoImportEstimate& estimate);
    GeoDataProbeResult BuildGeoDataProbe();
    std::string BuildGeoDataProbeSummary();
}
