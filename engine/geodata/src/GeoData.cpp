#include <AK/GeoData/GeoData.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace AK
{
    namespace
    {
        bool IsPositiveFinite(double value)
        {
            return std::isfinite(value) && value > 0.0;
        }

        void Merge(GeoValidationReport& into, const GeoValidationReport& from)
        {
            into.ok = into.ok && from.ok;
            into.warnings.insert(into.warnings.end(), from.warnings.begin(), from.warnings.end());
            into.errors.insert(into.errors.end(), from.errors.begin(), from.errors.end());
        }
    }

    const char* ToString(GeoCoordinateSystem system)
    {
        switch (system)
        {
            case GeoCoordinateSystem::LocalMeters:
                return "local-meters";
            case GeoCoordinateSystem::Wgs84Geographic:
                return "wgs84-geographic";
            case GeoCoordinateSystem::ProjectedMeters:
                return "projected-meters";
            case GeoCoordinateSystem::PlanetSurface:
                return "planet-surface";
            default:
                return "unknown";
        }
    }

    const char* ToString(GeoHeightSource source)
    {
        switch (source)
        {
            case GeoHeightSource::None:
                return "none";
            case GeoHeightSource::DemGrid:
                return "dem-grid";
            case GeoHeightSource::PointCloud:
                return "point-cloud";
            case GeoHeightSource::Mesh:
                return "mesh";
            default:
                return "unknown";
        }
    }

    const char* ToString(GeoRasterFormat format)
    {
        switch (format)
        {
            case GeoRasterFormat::R8:
                return "r8";
            case GeoRasterFormat::R16:
                return "r16";
            case GeoRasterFormat::R32Float:
                return "r32-float";
            case GeoRasterFormat::RGB8:
                return "rgb8";
            case GeoRasterFormat::RGBA8:
                return "rgba8";
            case GeoRasterFormat::Unknown:
            default:
                return "unknown";
        }
    }

    GeoReference MakeLocalMetersGeoReference()
    {
        GeoReference reference{};
        reference.coordinateSystem = GeoCoordinateSystem::LocalMeters;
        reference.metersPerUnit = 1.0;
        return reference;
    }

    GeoReference MakeWgs84GeoReference(EpsgCode epsg)
    {
        GeoReference reference{};
        reference.coordinateSystem = GeoCoordinateSystem::Wgs84Geographic;
        reference.epsg = epsg;
        reference.metersPerUnit = 1.0;
        return reference;
    }

    GeoReference MakePlanetGeoReference(const PlanetSurfaceConfig& planet)
    {
        GeoReference reference{};
        reference.coordinateSystem = GeoCoordinateSystem::PlanetSurface;
        reference.epsg = {4979};
        reference.planet = planet;
        reference.metersPerUnit = 1.0;
        return reference;
    }

    u32 BytesPerPixel(GeoRasterFormat format)
    {
        switch (format)
        {
            case GeoRasterFormat::R8:
                return 1;
            case GeoRasterFormat::R16:
                return 2;
            case GeoRasterFormat::R32Float:
                return 4;
            case GeoRasterFormat::RGB8:
                return 3;
            case GeoRasterFormat::RGBA8:
                return 4;
            case GeoRasterFormat::Unknown:
            default:
                return 0;
        }
    }

    u64 EstimateDemBytes(const DemGridDesc& dem)
    {
        return static_cast<u64>(dem.width) * static_cast<u64>(dem.height) * sizeof(float);
    }

    u64 EstimateOrthophotoBytes(const OrthophotoDesc& orthophoto)
    {
        return static_cast<u64>(orthophoto.width) * static_cast<u64>(orthophoto.height) * BytesPerPixel(orthophoto.format);
    }

    AABB3 BuildDemLocalBounds(const DemGridDesc& dem)
    {
        const float maxX = static_cast<float>(static_cast<double>(dem.width > 0 ? dem.width - 1 : 0) * dem.sampleSpacingXMeters);
        const float maxZ = static_cast<float>(static_cast<double>(dem.height > 0 ? dem.height - 1 : 0) * dem.sampleSpacingZMeters);
        return MakeAABB3({0.0f, static_cast<float>(dem.minHeightMeters), 0.0f}, {maxX, static_cast<float>(dem.maxHeightMeters), maxZ});
    }

    GeoValidationReport ValidateGeoReference(const GeoReference& reference)
    {
        GeoValidationReport report{};
        if (!IsPositiveFinite(reference.metersPerUnit))
        {
            report.ok = false;
            report.errors.push_back("GeoReference metersPerUnit must be positive and finite");
        }
        if (reference.coordinateSystem == GeoCoordinateSystem::Wgs84Geographic && !reference.epsg.IsValid())
        {
            report.warnings.push_back("WGS84 georeference has no EPSG code; default EPSG:4326 is expected");
        }
        if (reference.coordinateSystem == GeoCoordinateSystem::PlanetSurface && !IsPositiveFinite(reference.planet.radiusMeters))
        {
            report.ok = false;
            report.errors.push_back("Planet georeference radius must be positive and finite");
        }
        return report;
    }

    GeoValidationReport ValidateDemGrid(const DemGridDesc& dem)
    {
        GeoValidationReport report = ValidateGeoReference(dem.geo);
        if (dem.width == 0 || dem.height == 0)
        {
            report.ok = false;
            report.errors.push_back("DEM grid dimensions must be non-zero");
        }
        if (!IsPositiveFinite(dem.sampleSpacingXMeters) || !IsPositiveFinite(dem.sampleSpacingZMeters))
        {
            report.ok = false;
            report.errors.push_back("DEM sample spacing must be positive and finite");
        }
        if (dem.minHeightMeters > dem.maxHeightMeters)
        {
            report.ok = false;
            report.errors.push_back("DEM min height must not exceed max height");
        }
        return report;
    }

    GeoValidationReport ValidateOrthophoto(const OrthophotoDesc& orthophoto)
    {
        GeoValidationReport report = ValidateGeoReference(orthophoto.geo);
        if (orthophoto.width == 0 || orthophoto.height == 0)
        {
            report.ok = false;
            report.errors.push_back("orthophoto dimensions must be non-zero");
        }
        if (BytesPerPixel(orthophoto.format) == 0)
        {
            report.ok = false;
            report.errors.push_back("orthophoto format must be known");
        }
        if (!IsPositiveFinite(orthophoto.metersPerPixelX) || !IsPositiveFinite(orthophoto.metersPerPixelY))
        {
            report.ok = false;
            report.errors.push_back("orthophoto meters per pixel must be positive and finite");
        }
        return report;
    }

    GeoImportEstimate EstimateGeoImport(const DemGridDesc& dem, const OrthophotoDesc& orthophoto, const GeoImportPolicy& policy)
    {
        GeoImportEstimate estimate{};
        estimate.demBytes = EstimateDemBytes(dem);
        estimate.orthophotoBytes = EstimateOrthophotoBytes(orthophoto);
        estimate.localBounds = BuildDemLocalBounds(dem);

        const double widthMeters = static_cast<double>(std::max<u32>(dem.width, 1)) * dem.sampleSpacingXMeters;
        const double depthMeters = static_cast<double>(std::max<u32>(dem.height, 1)) * dem.sampleSpacingZMeters;
        const double tileEdge = std::max(1.0, policy.maxTileEdgeMeters);
        const u32 tilesX = static_cast<u32>(std::max(1.0, std::ceil(widthMeters / tileEdge)));
        const u32 tilesZ = static_cast<u32>(std::max(1.0, std::ceil(depthMeters / tileEdge)));
        estimate.terrainTiles = tilesX * tilesZ;

        const u64 totalSamples = static_cast<u64>(dem.width) * static_cast<u64>(dem.height);
        const u64 samplesPerChunk = static_cast<u64>(std::max<u32>(1, policy.maxTileResolution)) * static_cast<u64>(std::max<u32>(1, policy.maxTileResolution));
        estimate.pointCloudFallbackChunks = static_cast<u32>(std::max<u64>(1, (totalSamples + samplesPerChunk - 1) / samplesPerChunk));
        estimate.summary = ToDebugString(estimate);
        return estimate;
    }

    std::string ToDebugString(const GeoReference& reference)
    {
        std::ostringstream out;
        out << "geo=" << ToString(reference.coordinateSystem)
            << " epsg=" << reference.epsg.value
            << " metersPerUnit=" << reference.metersPerUnit
            << " yUp=" << (reference.yUp ? "true" : "false")
            << " rhs=" << (reference.rightHanded ? "true" : "false")
            << " cameraRelative=" << (reference.cameraRelativeRuntime ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const DemGridDesc& dem)
    {
        std::ostringstream out;
        out << "dem " << dem.width << "x" << dem.height
            << " spacing=" << dem.sampleSpacingXMeters << "x" << dem.sampleSpacingZMeters
            << " height=" << dem.minHeightMeters << ".." << dem.maxHeightMeters
            << " bytes=" << EstimateDemBytes(dem);
        return out.str();
    }

    std::string ToDebugString(const OrthophotoDesc& orthophoto)
    {
        std::ostringstream out;
        out << "orthophoto " << orthophoto.width << "x" << orthophoto.height
            << " format=" << ToString(orthophoto.format)
            << " mpp=" << orthophoto.metersPerPixelX << "x" << orthophoto.metersPerPixelY
            << " bytes=" << EstimateOrthophotoBytes(orthophoto);
        return out.str();
    }

    std::string ToDebugString(const GeoValidationReport& report)
    {
        std::ostringstream out;
        out << "geo validation " << (report.ok ? "ok" : "failed")
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

    std::string ToDebugString(const GeoImportEstimate& estimate)
    {
        std::ostringstream out;
        out << "geo import demBytes=" << estimate.demBytes
            << " orthophotoBytes=" << estimate.orthophotoBytes
            << " tiles=" << estimate.terrainTiles
            << " pcFallbackChunks=" << estimate.pointCloudFallbackChunks
            << " bounds=" << ToDebugString(estimate.localBounds.min, 1) << ".." << ToDebugString(estimate.localBounds.max, 1);
        return out.str();
    }

    GeoDataProbeResult BuildGeoDataProbe()
    {
        GeoDataProbeResult probe{};
        probe.reference = MakePlanetGeoReference();
        probe.dem.width = 4096;
        probe.dem.height = 2048;
        probe.dem.sampleSpacingXMeters = 2.0;
        probe.dem.sampleSpacingZMeters = 2.0;
        probe.dem.minHeightMeters = -120.0;
        probe.dem.maxHeightMeters = 2180.0;
        probe.dem.geo = probe.reference;

        probe.orthophoto.width = 4096;
        probe.orthophoto.height = 2048;
        probe.orthophoto.format = GeoRasterFormat::RGBA8;
        probe.orthophoto.metersPerPixelX = 2.0;
        probe.orthophoto.metersPerPixelY = 2.0;
        probe.orthophoto.geo = probe.reference;

        probe.validation = ValidateDemGrid(probe.dem);
        Merge(probe.validation, ValidateOrthophoto(probe.orthophoto));
        probe.estimate = EstimateGeoImport(probe.dem, probe.orthophoto);
        probe.ok = probe.validation.ok && probe.estimate.demBytes > 0 && probe.estimate.orthophotoBytes > 0 && probe.estimate.terrainTiles > 0;

        std::ostringstream out;
        out << (probe.ok ? "[ ok ] " : "[fail] ")
            << "geodata DEM / orthophoto import foundation "
            << ToDebugString(probe.estimate);
        probe.summary = out.str();
        return probe;
    }

    std::string BuildGeoDataProbeSummary()
    {
        return BuildGeoDataProbe().summary;
    }
}
