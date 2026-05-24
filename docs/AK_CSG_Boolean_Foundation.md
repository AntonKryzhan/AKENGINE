# AK Engine v3.6 — CSG Boolean / Destruction Foundation

## Purpose

This module adds the first controlled boolean-geometry layer for destructible worlds. It is intentionally built as a bounded, deterministic foundation instead of a fragile full triangle-triangle CSG implementation.

The first implementation targets two practical paths:

- fast primitive boolean operations through signed-distance evaluation;
- asset/model boolean preparation through voxelized closed meshes.

The output can be used immediately for collision proxy rebuilds and later for render remeshing, fracture surfaces, debris generation and GPU acceleration.

## Why this exists early

Destruction becomes expensive and unstable if it is added after physics, streaming and renderer code already assume static meshes. The engine needs a boolean layer before production physics because destructible geometry affects:

- collision broadphase;
- rigid-body proxies;
- bounds and visibility;
- asset cooking;
- streaming chunks;
- world partition cells;
- runtime save overlays.

## Supported foundation features

```text
Primitive SDF boolean:
  Box
  Sphere
  CylinderY
  CapsuleY

Mesh asset voxelization:
  odd-even raycast against closed triangle mesh
  intended for watertight assets
  deterministic low-resolution offline/runtime preprocessing

Boolean operations:
  Union
  Intersection
  Difference

Collision proxy extraction:
  per-voxel AABB debug mode
  greedy X-axis AABB runs for physics broadphase
```

## Runtime destruction contract

The current fast path is:

```text
source primitive / source mesh asset
        ↓
voxelize into bounded grid
        ↓
apply cutter primitive / tool grid
        ↓
boolean combine
        ↓
extract AABB collision proxies
        ↓
update BoundsComponent / future PhysicsProxy
```

This is deliberately safer than trying to do arbitrary triangle-triangle CSG during gameplay. Full render-quality remeshing is a future stage.

## Why voxel CSG first

Triangle mesh CSG can fail on common asset problems:

- non-manifold edges;
- holes;
- duplicate triangles;
- tiny sliver triangles;
- nearly-coplanar faces;
- inconsistent winding;
- floating-point precision loss.

Voxel CSG gives a robust first layer for destruction and collision, especially when the goal is gameplay physics rather than perfect CAD surfaces.

## Current limitations

```text
No final render mesh extraction yet.
No marching cubes / dual contouring yet.
No fracture material generation yet.
No convex decomposition yet.
Mesh voxelization expects closed watertight meshes.
Greedy collision merge currently collapses runs along X only.
```

These are intentional foundation limits. The next stages should be:

```text
v3.7 Physics Collision Proxy Foundation
v3.8 CSG Remesh / Marching Cubes Foundation
v3.9 Fracture Materials and Debris Foundation
v4.x GPU voxelization / compute path
```

## Engine rule

Runtime destruction must not mutate render meshes and physics meshes ad hoc. It should go through a bounded CSG/destruction job, update collision proxies, mark bounds dirty, and only then notify renderer/physics/streaming systems.
