#pragma once

#include <AK/Core/Types.hpp>
#include <AK/WorldTopology/WorldTopology.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class SplineKind : u32
    {
        Polyline,
        BezierCubic,
        CatmullRom
    };

    enum class SplineUsage : u32
    {
        Generic,
        Road,
        River,
        Railway,
        Cable,
        CameraRail,
        PatrolPath,
        TerrainDeformation
    };

    struct SplinePoint
    {
        DVec3 positionMeters{};
        DVec3 tangentHandleMeters{};
        double rollRadians = 0.0;
        double widthMeters = 1.0;
    };

    struct CubicBezierSegment
    {
        DVec3 p0{};
        DVec3 p1{};
        DVec3 p2{};
        DVec3 p3{};
    };

    struct CatmullRomSegment
    {
        DVec3 p0{};
        DVec3 p1{};
        DVec3 p2{};
        DVec3 p3{};
        double tension = 0.5;
    };

    struct SplinePath
    {
        SplineKind kind = SplineKind::CatmullRom;
        SplineUsage usage = SplineUsage::Generic;
        std::vector<SplinePoint> points;
        bool closed = false;
        double defaultWidthMeters = 1.0;
        u32 samplesPerSegment = 16;
    };

    struct SplineFrame
    {
        DVec3 positionMeters{};
        DVec3 tangent{};
        DVec3 normal{};
        DVec3 binormal{};
        double distanceMeters = 0.0;
        double parameter = 0.0;
        bool finite = true;
    };

    struct SplineClosestPoint
    {
        SplineFrame frame{};
        double distanceToSplineMeters = 0.0;
        u32 segmentIndex = 0;
        bool found = false;
    };

    struct SplineValidationReport
    {
        bool ok = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct SplineProbeResult
    {
        bool ok = false;
        std::string summary;
        SplineValidationReport validation{};
        SplinePath road{};
        double lengthMeters = 0.0;
        SplineFrame midFrame{};
        SplineClosestPoint closest{};
        DVec3 advancedPosition{};
    };

    const char* ToString(SplineKind kind);
    const char* ToString(SplineUsage usage);

    SplinePath MakeRoadSplinePath();
    SplinePath MakeCameraRailSplinePath();
    SplineValidationReport ValidateSplinePath(const SplinePath& path);

    DVec3 EvaluateCubicBezier(const CubicBezierSegment& segment, double t);
    DVec3 EvaluateCubicBezierTangent(const CubicBezierSegment& segment, double t);
    DVec3 EvaluateCatmullRom(const CatmullRomSegment& segment, double t);
    DVec3 EvaluateCatmullRomTangent(const CatmullRomSegment& segment, double t);

    double ApproximateSplineLength(const SplinePath& path, u32 samplesPerSegment = 16);
    SplineFrame SampleSplineFrame(const SplinePath& path, double normalizedParameter, DVec3 preferredUp = {0.0, 1.0, 0.0});
    SplineClosestPoint FindClosestPointOnSpline(const SplinePath& path, DVec3 pointMeters, u32 samplesPerSegment = 24);
    DVec3 AdvanceAlongSpline(const SplinePath& path, double startParameter, double distanceMeters, u32 samplesPerSegment = 24);

    std::string ToDebugString(const SplineValidationReport& report);
    std::string ToDebugString(const SplineFrame& frame, int precision = 3);
    std::string ToDebugString(const SplineClosestPoint& closest, int precision = 3);
    std::string BuildSplineProbeSummary();
    SplineProbeResult BuildSplineProbe();
}
