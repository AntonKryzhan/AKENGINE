# AK Engine v3.7 — Physics Collision Proxy Foundation

## Purpose

This stage adds the first runtime physics foundation that can consume collision proxies produced by CSG/destruction code. It is intentionally narrow: deterministic bodies, primitive colliders, CSG AABB proxy import, broadphase pair generation, contact generation and a fixed-step integration contract.

The goal is not a complete production physics engine yet. The goal is to make destruction, terrain, streaming and ECS agree on one safe collision-proxy path before adding joints, character controllers, convex hulls, islands or threaded solvers.

## Supported foundation features

```text
Body kinds:
  Static
  Kinematic
  Dynamic

Collider kinds:
  Sphere
  Box
  ProxyAABB

Contact paths:
  sphere-sphere
  box/proxy AABB vs box/proxy AABB
  sphere vs box/proxy AABB

Runtime path:
  CSG boolean result
        ↓
  CsgCollisionProxy list
        ↓
  Physics ProxyAABB colliders
        ↓
  broadphase pair list
        ↓
  contact manifold foundation
        ↓
  simple positional/impulse resolution
```

## Why this follows CSG

CSG/destruction changes collision topology. If the physics system cannot consume bounded proxy output directly, runtime destruction will either mutate render meshes unsafely or rebuild expensive triangle collision during gameplay.

The current contract is:

```text
Destruction jobs output conservative AABB proxies.
Physics imports them as ProxyAABB colliders.
The broadphase treats them exactly like normal collision boxes.
A later narrowphase can replace selected proxy groups with convex hulls.
```

## Fixed-step rule

The physics step clamps large frame deltas and subdivides them into bounded substeps. This prevents a single slow frame from producing unbounded integration and collision correction.

```text
fixedDeltaSeconds = 1 / 60 by default
maxDeltaSeconds   = 0.1 by default
maxSubsteps       = 8 by default
```

## Current limitations

```text
No island solver yet.
No persistent contact cache yet.
No friction cone solver yet.
No swept collision / CCD yet.
No rotational inertia yet.
No convex hull / GJK-EPA narrowphase yet.
No ECS TransformComponent sync yet.
Broadphase is deterministic O(n²), intended only as the foundation contract.
```

These limits are intentional. A bad physics architecture is expensive to replace later; this stage defines the contracts before optimizing.

## Next stages

```text
v3.8 Physics ECS Sync + Scene Serialization
v3.9 Broadphase Acceleration Grid / Sweep-and-Prune
v4.0 GJK/EPA Convex Narrowphase
v4.1 Sequential Impulse Constraint Solver
v4.2 Character Controller Foundation
```

## Probe

Run:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_physicsprobe.exe
```

Expected result:

```text
[ ok ] physics foundation bodies=3 colliders=N csg_proxies=N pairs=N contacts=N step_contacts=N
physics bodies=3 colliders=N dynamic=1 proxies=N pairs=N contacts=N resolved=N substeps=N seconds=...
```

## Engine rule

Runtime physics must not attach collision directly to render mesh mutation. Physics should consume authored primitive colliders, terrain colliders, or bounded destruction/CSG proxy output, then later upgrade selected proxy clusters to convex or mesh acceleration structures through explicit cooking jobs.
