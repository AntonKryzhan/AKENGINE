# AK Engine World Partition Foundation

AK Engine v2.8 introduces a deterministic world-partition layer for large maps.

## Problem

Large-world coordinates fix precision, but they do not solve residency. A huge world cannot be kept fully loaded, fully visible, fully simulated, or fully resident in memory.

Without a partition layer the engine would eventually hit these limits:

- every object/asset loads at startup;
- editor scans and draws too much at once;
- streaming has no spatial priority;
- AI, physics, navigation, bounds and renderer cannot reason about active world regions;
- unloading is ad-hoc and unsafe.

## v2.8 model

The partition layer uses stable cell identifiers:

```text
WorldPartitionCellId = x, z, lod
```

The current implementation tracks:

```text
Unloaded -> Queued -> Resident -> Active -> Evicting
```

A camera position is converted from `WorldPosition` into a partition cell. The system then builds a residency plan:

- cells inside `activeRadiusCells` become active;
- cells inside `preloadRadiusCells` are queued for streaming;
- cells outside `unloadRadiusCells` are evicted;
- per-update activation and eviction budgets prevent spikes.

## Why this matters

This layer is the bridge between:

```text
Large World Coordinates
Asset Database
VFS
StreamingSystem
ResourceManager
Renderer visibility
Physics islands
AI/navigation chunks
```

It prevents the future open-world failure mode where the engine has precise coordinates, but still tries to load/simulate/render the whole world.

## Current scope

v2.8 is intentionally CPU-only and deterministic. It does not yet stream real cell packages by itself. It produces the cell residency state and statistics that the streaming system can consume in later patches.

## Next steps

- attach cell asset lists from asset manifest metadata;
- route queued cells into `StreamingSystem` requests;
- add editor overlay for partition cells;
- add LOD rings;
- connect active cells to physics island creation;
- connect active cells to navigation tile activation;
- connect visible cells to renderer render-proxy generation.
