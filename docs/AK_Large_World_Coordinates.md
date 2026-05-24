# AK Engine Large World Coordinates

Status: v1.1 foundation.

This layer prevents the classic large-world floating point jitter: objects far from `(0,0,0)` no longer need to be rendered, simulated, or edited directly in huge `float32` coordinates.

## Problem

A 32-bit float has about 7 significant decimal digits. The farther an object is from origin, the larger the gap between adjacent representable values becomes. At world-scale distances this causes:

- camera jitter;
- discrete object movement;
- unstable ray casts and picking;
- shadow and z-buffer noise;
- physics contact jitter;
- non-deterministic replay/network sync edge cases.

## AK approach

AK Engine keeps an authoritative large-world position separately from the local transform:

```text
WorldPosition = WorldCell<int64> + local<double>
RenderPosition = WorldPosition - CameraWorldPosition -> float Vec3
```

The renderer must receive camera-relative floats, not absolute million-meter positions.

## Added types

```text
WorldCell
WorldPosition
WorldOrigin
LargeWorldConfig
CameraRelativePosition
WorldPositionComponent
```

## Similar precision traps handled or documented

### Float movement jitter

Fixed by cell+double world positions and camera-relative conversion.

### Z-buffer precision loss

A huge far/near ratio destroys depth precision. Later Vulkan work should use reversed-Z, sane near/far defaults, per-camera depth ranges, and possibly logarithmic depth only for special cases.

### Physics far from origin

Rigid-body and character simulation must run inside local physics islands near their own origin. Global coordinates should be converted at synchronization boundaries only.

### Time precision drift

The existing stopwatch already uses a monotonic double-second value. Long-running simulation should later use integer ticks plus double frame delta, not a single ever-growing float time.

### Accumulated transform drift

Future transform systems should avoid endlessly multiplying matrices into themselves. Store TRS authoritatively and rebuild matrices when needed.

## Current limitation

v1.1 does not replace the editor viewport with a full large-world editor. It adds the safe coordinate layer, scene serialization, ECS component storage, and a probe tool. Full render/physics rebasing will be introduced when Vulkan, physics and streaming modules appear.
