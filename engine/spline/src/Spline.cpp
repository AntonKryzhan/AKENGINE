#include <AK/Spline/Spline.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr double MinStep = 1.0e-9;

        double Clamp01(double value)
        {
            if (!IsFinite(value))
            {
                return 0.0;
            }
            return std::max(0.0, std::min(1.0, value));
        }

        DVec3 LerpDVec3(DVec3 a, DVec3 b, double t)
        {
            return Add(a, Multiply(Subtract(b, a), t));
        }

        double Distance(DVec3 a, DVec3 b)
        {
            return Length(Subtract(a, b));
        }

        u32 SegmentCount(const SplinePath& path)
        {
            if (path.points.size() < 2)
            {
                return 0;
            }
            if (path.kind == SplineKind::BezierCubic)
            {
                return static_cast<u32>((path.points.size() - 1) / 3);
            }
            if (path.closed)
            {
                return static_cast<u32>(path.points.size());
            }
            return static_cast<u32>(path.points.size() - 1);
        }

        const SplinePoint& PointAt(const SplinePath& path, i64 index)
        {
            const i64 count = static_cast<i64>(path.points.size());
            if (path.closed && count > 0)
            {
                i64 wrapped = index % count;
                if (wrapped < 0)
                {
                    wrapped += count;
                }
                return path.points[static_cast<std::size_t>(wrapped)];
            }
            return path.points[static_cast<std::size_t>(std::clamp<i64>(index, 0, count - 1))];
        }

        CubicBezierSegment BuildBezierSegment(const SplinePath& path, u32 segmentIndex)
        {
            const std::size_t base = static_cast<std::size_t>(segmentIndex) * 3u;
            CubicBezierSegment segment{};
            segment.p0 = path.points[base + 0u].positionMeters;
            segment.p1 = path.points[base + 1u].positionMeters;
            segment.p2 = path.points[base + 2u].positionMeters;
            segment.p3 = path.points[base + 3u].positionMeters;
            return segment;
        }

        CatmullRomSegment BuildCatmullSegment(const SplinePath& path, u32 segmentIndex)
        {
            const i64 i = static_cast<i64>(segmentIndex);
            CatmullRomSegment segment{};
            segment.p0 = PointAt(path, i - 1).positionMeters;
            segment.p1 = PointAt(path, i).positionMeters;
            segment.p2 = PointAt(path, i + 1).positionMeters;
            segment.p3 = PointAt(path, i + 2).positionMeters;
            segment.tension = 0.5;
            return segment;
        }

        DVec3 EvaluateSegment(const SplinePath& path, u32 segmentIndex, double t)
        {
            t = Clamp01(t);
            if (path.kind == SplineKind::BezierCubic)
            {
                return EvaluateCubicBezier(BuildBezierSegment(path, segmentIndex), t);
            }
            if (path.kind == SplineKind::Polyline)
            {
                return LerpDVec3(PointAt(path, segmentIndex).positionMeters, PointAt(path, static_cast<i64>(segmentIndex) + 1).positionMeters, t);
            }
            return EvaluateCatmullRom(BuildCatmullSegment(path, segmentIndex), t);
        }

        DVec3 EvaluateSegmentTangent(const SplinePath& path, u32 segmentIndex, double t)
        {
            t = Clamp01(t);
            if (path.kind == SplineKind::BezierCubic)
            {
                return EvaluateCubicBezierTangent(BuildBezierSegment(path, segmentIndex), t);
            }
            if (path.kind == SplineKind::Polyline)
            {
                return Normalize(Subtract(PointAt(path, static_cast<i64>(segmentIndex) + 1).positionMeters, PointAt(path, segmentIndex).positionMeters), {1.0, 0.0, 0.0});
            }
            return EvaluateCatmullRomTangent(BuildCatmullSegment(path, segmentIndex), t);
        }

        SplineFrame BuildFrame(DVec3 position, DVec3 tangent, DVec3 preferredUp, double distance, double parameter)
        {
            SplineFrame frame{};
            frame.positionMeters = position;
            frame.tangent = Normalize(tangent, {1.0, 0.0, 0.0});
            DVec3 up = Normalize(preferredUp, {0.0, 1.0, 0.0});
            if (std::abs(Dot(frame.tangent, up)) > 0.96)
            {
                up = {0.0, 0.0, 1.0};
            }
            frame.binormal = Normalize(Cross(frame.tangent, up), {0.0, 0.0, 1.0});
            frame.normal = Normalize(Cross(frame.binormal, frame.tangent), {0.0, 1.0, 0.0});
            frame.distanceMeters = distance;
            frame.parameter = parameter;
            frame.finite = IsFinite(frame.positionMeters) && IsFinite(frame.tangent) && IsFinite(frame.normal) && IsFinite(frame.binormal);
            return frame;
        }
    }

    const char* ToString(SplineKind kind)
    {
        switch (kind)
        {
        case SplineKind::Polyline:
            return "Polyline";
        case SplineKind::BezierCubic:
            return "BezierCubic";
        case SplineKind::CatmullRom:
            return "CatmullRom";
        default:
            return "Unknown";
        }
    }

    const char* ToString(SplineUsage usage)
    {
        switch (usage)
        {
        case SplineUsage::Generic:
            return "Generic";
        case SplineUsage::Road:
            return "Road";
        case SplineUsage::River:
            return "River";
        case SplineUsage::Railway:
            return "Railway";
        case SplineUsage::Cable:
            return "Cable";
        case SplineUsage::CameraRail:
            return "CameraRail";
        case SplineUsage::PatrolPath:
            return "PatrolPath";
        case SplineUsage::TerrainDeformation:
            return "TerrainDeformation";
        default:
            return "Unknown";
        }
    }

    SplinePath MakeRoadSplinePath()
    {
        SplinePath path{};
        path.kind = SplineKind::CatmullRom;
        path.usage = SplineUsage::Road;
        path.closed = false;
        path.defaultWidthMeters = 8.0;
        path.samplesPerSegment = 24;
        path.points = {
            {{-120.0, 0.0, -40.0}, {}, 0.0, 8.0},
            {{-40.0, 2.0, 10.0}, {}, 0.0, 8.0},
            {{60.0, 1.0, 35.0}, {}, 0.0, 8.0},
            {{140.0, 0.0, -20.0}, {}, 0.0, 8.0}
        };
        return path;
    }

    SplinePath MakeCameraRailSplinePath()
    {
        SplinePath path{};
        path.kind = SplineKind::BezierCubic;
        path.usage = SplineUsage::CameraRail;
        path.closed = false;
        path.defaultWidthMeters = 1.0;
        path.samplesPerSegment = 32;
        path.points = {
            {{0.0, 4.0, -20.0}, {}, 0.0, 1.0},
            {{20.0, 7.0, -10.0}, {}, 0.0, 1.0},
            {{40.0, 7.0, 10.0}, {}, 0.0, 1.0},
            {{60.0, 4.0, 20.0}, {}, 0.0, 1.0}
        };
        return path;
    }

    SplineValidationReport ValidateSplinePath(const SplinePath& path)
    {
        SplineValidationReport report{};
        const auto error = [&report](const std::string& message)
        {
            report.ok = false;
            report.errors.push_back(message);
        };
        const auto warning = [&report](const std::string& message)
        {
            report.warnings.push_back(message);
        };

        if (path.points.size() < 2)
        {
            error("spline path needs at least two points");
        }
        if (path.kind == SplineKind::BezierCubic && path.points.size() >= 2 && ((path.points.size() - 1) % 3) != 0)
        {
            error("Bezier cubic path requires 3n+1 control points");
        }
        if (path.samplesPerSegment == 0)
        {
            error("samples per segment must be positive");
        }
        if (path.defaultWidthMeters <= 0.0)
        {
            error("default spline width must be positive");
        }

        for (std::size_t i = 0; i < path.points.size(); ++i)
        {
            if (!IsFinite(path.points[i].positionMeters))
            {
                error("spline point contains non-finite position");
                break;
            }
            if (path.points[i].widthMeters <= 0.0)
            {
                warning("spline point width is non-positive; default width should be used");
            }
        }

        if (SegmentCount(path) == 0 && path.points.size() >= 2)
        {
            error("spline has no usable segments");
        }
        if (path.closed && path.kind == SplineKind::BezierCubic)
        {
            warning("closed Bezier paths are not expanded in this foundation layer");
        }
        return report;
    }

    DVec3 EvaluateCubicBezier(const CubicBezierSegment& segment, double t)
    {
        t = Clamp01(t);
        const double u = 1.0 - t;
        const DVec3 a = Multiply(segment.p0, u * u * u);
        const DVec3 b = Multiply(segment.p1, 3.0 * u * u * t);
        const DVec3 c = Multiply(segment.p2, 3.0 * u * t * t);
        const DVec3 d = Multiply(segment.p3, t * t * t);
        return Add(Add(a, b), Add(c, d));
    }

    DVec3 EvaluateCubicBezierTangent(const CubicBezierSegment& segment, double t)
    {
        t = Clamp01(t);
        const double u = 1.0 - t;
        const DVec3 a = Multiply(Subtract(segment.p1, segment.p0), 3.0 * u * u);
        const DVec3 b = Multiply(Subtract(segment.p2, segment.p1), 6.0 * u * t);
        const DVec3 c = Multiply(Subtract(segment.p3, segment.p2), 3.0 * t * t);
        return Normalize(Add(Add(a, b), c), {1.0, 0.0, 0.0});
    }

    DVec3 EvaluateCatmullRom(const CatmullRomSegment& segment, double t)
    {
        t = Clamp01(t);
        const double t2 = t * t;
        const double t3 = t2 * t;
        const DVec3 a = Multiply(segment.p1, 2.0);
        const DVec3 b = Multiply(Subtract(segment.p2, segment.p0), t);
        const DVec3 c = Multiply(Add(Add(Multiply(segment.p0, 2.0), Multiply(segment.p2, 4.0)), Add(Multiply(segment.p1, -5.0), Multiply(segment.p3, -1.0))), t2);
        const DVec3 d = Multiply(Add(Add(Multiply(segment.p1, 3.0), Multiply(segment.p3, 1.0)), Add(Multiply(segment.p0, -1.0), Multiply(segment.p2, -3.0))), t3);
        return Multiply(Add(Add(a, b), Add(c, d)), segment.tension);
    }

    DVec3 EvaluateCatmullRomTangent(const CatmullRomSegment& segment, double t)
    {
        t = Clamp01(t);
        const double t2 = t * t;
        const DVec3 a = Subtract(segment.p2, segment.p0);
        const DVec3 b = Multiply(Add(Add(Multiply(segment.p0, 2.0), Multiply(segment.p2, 4.0)), Add(Multiply(segment.p1, -5.0), Multiply(segment.p3, -1.0))), 2.0 * t);
        const DVec3 c = Multiply(Add(Add(Multiply(segment.p1, 3.0), Multiply(segment.p3, 1.0)), Add(Multiply(segment.p0, -1.0), Multiply(segment.p2, -3.0))), 3.0 * t2);
        return Normalize(Multiply(Add(Add(a, b), c), segment.tension), {1.0, 0.0, 0.0});
    }

    double ApproximateSplineLength(const SplinePath& path, u32 samplesPerSegment)
    {
        if (!ValidateSplinePath(path).ok)
        {
            return 0.0;
        }
        const u32 segments = SegmentCount(path);
        const u32 samples = std::max(1u, samplesPerSegment);
        double length = 0.0;
        for (u32 segment = 0; segment < segments; ++segment)
        {
            DVec3 previous = EvaluateSegment(path, segment, 0.0);
            for (u32 sample = 1; sample <= samples; ++sample)
            {
                const double t = static_cast<double>(sample) / static_cast<double>(samples);
                const DVec3 current = EvaluateSegment(path, segment, t);
                length += Distance(previous, current);
                previous = current;
            }
        }
        return length;
    }

    SplineFrame SampleSplineFrame(const SplinePath& path, double normalizedParameter, DVec3 preferredUp)
    {
        const SplineValidationReport validation = ValidateSplinePath(path);
        if (!validation.ok)
        {
            SplineFrame frame{};
            frame.finite = false;
            return frame;
        }

        const u32 segments = SegmentCount(path);
        const double clampedParameter = Clamp01(normalizedParameter);
        const double scaled = clampedParameter * static_cast<double>(segments);
        const u32 segment = std::min(segments - 1u, static_cast<u32>(std::floor(scaled)));
        const double localT = Clamp01(scaled - static_cast<double>(segment));
        const DVec3 position = EvaluateSegment(path, segment, localT);
        const DVec3 tangent = EvaluateSegmentTangent(path, segment, localT);
        const double totalLength = ApproximateSplineLength(path, path.samplesPerSegment);
        return BuildFrame(position, tangent, preferredUp, totalLength * clampedParameter, clampedParameter);
    }

    SplineClosestPoint FindClosestPointOnSpline(const SplinePath& path, DVec3 pointMeters, u32 samplesPerSegment)
    {
        SplineClosestPoint closest{};
        const SplineValidationReport validation = ValidateSplinePath(path);
        if (!validation.ok || !IsFinite(pointMeters))
        {
            return closest;
        }

        const u32 segments = SegmentCount(path);
        const u32 samples = std::max(1u, samplesPerSegment);
        double bestDistance = std::numeric_limits<double>::max();
        u32 bestSegment = 0;
        double bestT = 0.0;
        DVec3 bestPosition{};
        DVec3 bestTangent{1.0, 0.0, 0.0};

        for (u32 segment = 0; segment < segments; ++segment)
        {
            for (u32 sample = 0; sample <= samples; ++sample)
            {
                const double t = static_cast<double>(sample) / static_cast<double>(samples);
                const DVec3 position = EvaluateSegment(path, segment, t);
                const double d = Distance(position, pointMeters);
                if (d < bestDistance)
                {
                    bestDistance = d;
                    bestSegment = segment;
                    bestT = t;
                    bestPosition = position;
                    bestTangent = EvaluateSegmentTangent(path, segment, t);
                }
            }
        }

        closest.segmentIndex = bestSegment;
        closest.distanceToSplineMeters = bestDistance;
        closest.found = bestDistance < std::numeric_limits<double>::max();
        closest.frame = BuildFrame(bestPosition, bestTangent, {0.0, 1.0, 0.0}, 0.0, (static_cast<double>(bestSegment) + bestT) / static_cast<double>(segments));
        return closest;
    }

    DVec3 AdvanceAlongSpline(const SplinePath& path, double startParameter, double distanceMeters, u32 samplesPerSegment)
    {
        const double totalLength = ApproximateSplineLength(path, samplesPerSegment);
        if (totalLength <= MinStep)
        {
            return SampleSplineFrame(path, startParameter).positionMeters;
        }

        double targetParameter = startParameter + distanceMeters / totalLength;
        if (path.closed)
        {
            targetParameter = std::fmod(targetParameter, 1.0);
            if (targetParameter < 0.0)
            {
                targetParameter += 1.0;
            }
        }
        else
        {
            targetParameter = Clamp01(targetParameter);
        }
        return SampleSplineFrame(path, targetParameter).positionMeters;
    }

    std::string ToDebugString(const SplineValidationReport& report)
    {
        std::ostringstream out;
        out << (report.ok ? "ok" : "error") << " warnings=" << report.warnings.size() << " errors=" << report.errors.size();
        if (!report.errors.empty())
        {
            out << " first=" << report.errors.front();
        }
        else if (!report.warnings.empty())
        {
            out << " first=" << report.warnings.front();
        }
        return out.str();
    }

    std::string ToDebugString(const SplineFrame& frame, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "frame pos=(" << frame.positionMeters.x << ", " << frame.positionMeters.y << ", " << frame.positionMeters.z << ")"
            << " tangent=(" << frame.tangent.x << ", " << frame.tangent.y << ", " << frame.tangent.z << ")"
            << " param=" << frame.parameter
            << " dist=" << frame.distanceMeters
            << " finite=" << (frame.finite ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const SplineClosestPoint& closest, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "closest found=" << (closest.found ? "yes" : "no")
            << " segment=" << closest.segmentIndex
            << " distance=" << closest.distanceToSplineMeters
            << " param=" << closest.frame.parameter;
        return out.str();
    }

    std::string BuildSplineProbeSummary()
    {
        const SplineProbeResult probe = BuildSplineProbe();
        return probe.summary;
    }

    SplineProbeResult BuildSplineProbe()
    {
        SplineProbeResult probe{};
        probe.road = MakeRoadSplinePath();
        probe.validation = ValidateSplinePath(probe.road);
        probe.lengthMeters = ApproximateSplineLength(probe.road, 32);
        probe.midFrame = SampleSplineFrame(probe.road, 0.5);
        probe.closest = FindClosestPointOnSpline(probe.road, {15.0, 0.0, 22.0}, 32);
        probe.advancedPosition = AdvanceAlongSpline(probe.road, 0.25, 50.0, 32);
        probe.ok = probe.validation.ok && probe.lengthMeters > 1.0 && probe.midFrame.finite && probe.closest.found && IsFinite(probe.advancedPosition);

        std::ostringstream out;
        out << "Spline probe: " << (probe.ok ? "ok" : "failed")
            << " kind=" << ToString(probe.road.kind)
            << " usage=" << ToString(probe.road.usage)
            << " points=" << probe.road.points.size()
            << " length=" << std::fixed << std::setprecision(2) << probe.lengthMeters
            << " closest=" << probe.closest.distanceToSplineMeters;
        probe.summary = out.str();
        return probe;
    }
}
