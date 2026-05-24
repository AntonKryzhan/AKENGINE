# AK Engine v0.9 — Basic Math / Geometry Core

Status: **implemented as engine foundation**

This stage adds the first reusable geometry layer that will be used by renderer, scene picking, physics broadphase, culling, navigation and asset import.

## Added modules

```text
engine/math/include/AK/Math/Numerics.hpp
engine/math/include/AK/Math/Geometry.hpp
engine/math/src/Numerics.cpp
engine/math/src/Geometry.cpp
tools/geomprobe/src/main.cpp
```

## Numerics

The numeric layer is deliberately small and conservative:

```text
Pi32 / DegToRad32 / RadToDeg32
IsFinite
NearlyEqual
ClampFloat / Saturate
SafeDivide
WrapAngleDegrees
Lerp
```

This is the first step toward a robust math baseline: no hidden division by zero, no silent NaN propagation in core geometry helpers, and explicit tolerances for approximate comparisons.

## Geometry primitives

```text
Vec2
Vec4
Quat
Mat4
Ray3
Plane3
Sphere3
AABB3
Frustum3
```

`Vec3` remains in `AK/Core/Types.hpp` for compatibility with existing `TransformComponent`. The new geometry functions operate on that same `Vec3` type to avoid breaking scene, ECS and editor code.

## Operations

```text
Vec3 add/sub/mul/div
Dot / Cross / Length / Normalize
Quat identity / axis-angle / multiply / rotate
Mat4 identity / translation / scale / rotation / multiply
AABB create / expand / union / center / extents / size / surface area
point containment
AABB intersection
Ray vs AABB
Ray vs Sphere
Sphere vs AABB
Frustum vs AABB
```

## Editor integration

The Inspector now shows preview AABB data for the selected entity:

```text
AABB min/max
AABB size
surface area
geometry probe summary
```

This is intentionally not a real mesh bound yet. It is a transform-derived preview bound that gives the editor and future systems a stable geometry API before asset mesh bounds are imported.

## Probe tool

`ak_geomprobe.exe` validates the basic geometry path:

```text
ray/AABB intersection
quaternion rotation
matrix transform
AABB surface area
```

Run:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_geomprobe.exe
```

Expected output includes:

```text
ray_aabb_hit=true
rotated_forward=1.000, 0.000, 0.000
surface_area=40.000
```

## Next use

The next systems should consume this layer instead of adding ad-hoc vector math:

```text
viewport picking refinement
scene object bounds
asset mesh bounds
camera frustum culling
physics broadphase AABB
BVH nodes
navigation debug volumes
renderer visibility tests
```
