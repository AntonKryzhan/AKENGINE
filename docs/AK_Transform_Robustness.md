# AK Engine v1.4 — Transform Robustness

This stage hardens the transform layer before renderer, physics, scene graph, and prefabs become dependent on it.

## Problems fixed early

### NaN / Inf transform poisoning
A single invalid float in position, rotation, or scale can spread into matrices, bounds, culling, picking, physics proxies, and GPU buffers. The transform math layer now has explicit validation and sanitization helpers.

### Zero scale and near-zero scale
Zero scale creates singular matrices. That breaks inverse transforms, normal matrices, physics shapes, ray picking, bounds updates, and future gizmos. Scale is clamped to a safe minimum magnitude while preserving sign for mirrored transforms.

### Unbounded Euler angle growth
If Euler angles grow forever, precision gets worse and scene files become noisy. Editor-facing Euler rotations are wrapped to the `[-180, 180]` range. Runtime math converts them to normalized quaternions before matrix generation.

### Matrix drift
The authoritative state is TRS, not an accumulated matrix. Matrices are generated from sanitized position, normalized quaternion rotation, and sanitized scale. This avoids long-term drift from repeatedly multiplying matrices into themselves.

### Dirty propagation foundation
`TransformComponent` now carries `dirtyFlags` and `revision`. This prepares the engine for exact invalidation of world matrices, bounds, render proxies, and physics proxies instead of recomputing everything every frame.

## Added API

```text
AK::EulerTransform
AK::TransformSanitizePolicy
AK::TransformSanitizeResult
AK::SanitizeEulerTransform
AK::QuatFromEulerXYZDegrees
AK::Mat4FromTRS
AK::Mat4FromEulerTransform
AK::TransformAABB
AK::IsTransformUsable
```

## Editor behavior

The editor sanitizes transforms after scene load, undo/redo restore, viewport drag, keyboard transform edits, and world-position sync. Inspector now displays transform revision, dirty flags, matrix validity, usability, and the transform probe summary.

## Rule

Authoritative transform state remains:

```text
position + rotation + scale
```

Derived data is treated as cache:

```text
local matrix
world matrix
bounds
render proxy
physics proxy
```

Derived data must be regenerated from the authoritative TRS when dirty flags are set.
