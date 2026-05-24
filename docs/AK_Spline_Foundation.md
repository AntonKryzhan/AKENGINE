# AK Engine v3.5 — Spline Foundation

The spline layer removes another hardcoded-world assumption: roads, rivers,
railways, cables, camera rails, patrol paths and terrain deformation paths must
not be authored as ad-hoc arrays inside gameplay code.

## Added module

```text
engine/spline
```

## Main types

```text
SplineKind
  Polyline
  BezierCubic
  CatmullRom

SplineUsage
  Generic
  Road
  River
  Railway
  Cable
  CameraRail
  PatrolPath
  TerrainDeformation

SplinePath
SplinePoint
SplineFrame
SplineClosestPoint
SplineValidationReport
```

## Supported operations

```text
EvaluateCubicBezier
EvaluateCubicBezierTangent
EvaluateCatmullRom
EvaluateCatmullRomTangent
ApproximateSplineLength
SampleSplineFrame
FindClosestPointOnSpline
AdvanceAlongSpline
```

## Why this matters

Spline data is a foundation for large worlds:

```text
roads and rivers crossing world-partition cells
planet-surface roads projected to local tangent frames
railways and vehicle routes
camera rails and cinematic paths
AI patrol paths
terrain deformation corridors
cables, pipes and power lines
```

Later patches can bind spline anchors to world cells, planet geodetic
coordinates, terrain patches and streaming chunks. This avoids a common engine
mistake where roads and navigation paths are only flat XZ editor objects.
