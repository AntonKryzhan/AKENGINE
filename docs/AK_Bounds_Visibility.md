# AK Engine v1.5 — Bounds and Visibility Foundation

This layer removes an early architectural limit: renderable and selectable objects can no longer exist without explicit spatial bounds.

## Why this exists

Large scenes fail if every system scans every object every frame. Rendering, ray picking, physics broadphase, LOD, streaming, occlusion and editor selection all need stable bounds.

Without a bounds layer, future Vulkan rendering would quickly hit these problems:

- no frustum culling;
- no broadphase for ray picking and physics;
- no object visibility budget;
- no safe world partition activation;
- no LOD selection basis;
- no way to know scene spatial extent;
- hidden NaN/invalid bounds poisoning renderer and physics.

## Components

`BoundsComponent` stores:

- local AABB;
- world AABB;
- world bounding sphere;
- dirty flags;
- revision;
- visibility/culling state.

`TransformComponent` remains the authoritative TRS state. Matrices and bounds are derived data.

## Dirty propagation

When a transform changes, `TransformDirty_Bounds` is set. The visibility layer rebuilds world bounds and clears that dirty flag.

This prevents two bad modes:

- recalculating every bound manually everywhere;
- rendering/picking against stale bounds after movement or scaling.

## Future integration

This module is the base for:

- renderer frustum culling;
- editor ray picking;
- physics broadphase;
- world partition streaming;
- LOD and impostors;
- occlusion culling;
- GPU-driven culling.
