# AK Engine v3.9 — CSG Destruction Bridge Foundation

Status: v3.9 foundation.

This patch connects the existing CSG boolean output to the rest of the engine instead of leaving voxel/proxy data isolated inside `ak_csg`.

## Problem closed

CSG/destruction must not become a separate toy system. A destructive edit has to invalidate and regenerate all dependent runtime views:

```text
CSG boolean result
    -> generated geometry identity
    -> collision proxy set
    -> physics colliders
    -> entity bounds
    -> world partition dirty cell
    -> diagnostics counters/events
```

Without this bridge the engine would later need a painful rewrite when physics, streaming, bounds, editor selection and renderer all start depending on different versions of the same destroyed object.

## Added module

```text
engine/destruction
```

Main types:

```text
DestructibleComponent
DestructionMaterial
FractureSettings
CsgDestructionRequest
DestructionResult
CollisionProxySet
PhysicsProxyDesc
GeneratedGeometryDesc
DirtyWorldCell
DestructionEvent
DestructionRegistry
```

## Pipeline

`ExecuteCsgDestruction()` performs the CSG operation and prepares deterministic engine-facing output:

```text
source primitive / source mesh asset
        ↓
CSG boolean
        ↓
trimmed CollisionProxySet
        ↓
PhysicsProxyDesc
        ↓
generated:/destruction/... logical mesh path + AssetGuid
        ↓
WorldPartitionCellId dirty mark
```

`ApplyDestructionResult()` applies the result to optional engine contexts:

```text
World              -> MeshComponent mesh path + BoundsComponent rebuild
PhysicsScene       -> static body + ProxyAABB colliders
ResourceManager    -> generated mesh ResourceHandle
WorldPartition     -> generated asset ref in dirty cell
DiagnosticsHub     -> destruction counters and event
```

## Guardrails

- invalid entity requests are rejected;
- fracture resolution is clamped to a safe range;
- proxy count is capped by `FractureSettings::maxCollisionProxies`;
- generated geometry gets a stable logical path and deterministic `AssetGuid`;
- ECS/physics/resource/partition/diagnostics contexts are optional;
- stale or missing ECS entity prevents ECS mutation but does not corrupt other systems.

## Probe

```text
ak_destructionprobe
```

Expected behavior:

```text
Destruction bridge probe: ok ...
physics_bodies=1
physics_colliders>0
resource_alive=1
partition_refs=1
diag_events=1
```

## Next steps

Recommended next patch:

```text
v4.0 Physics/ECS Runtime Sync
```

Scope:

```text
PhysicsBodyComponent
ColliderComponent
PhysicsSceneBuilder from ECS
fixed tick transform writeback
physics island origin policy
contact event queue
```

After that the engine can safely move toward:

```text
v4.1 Collision Shapes Expansion
v4.2 Destruction Remesh Foundation
v4.3 Material / Surface System
v4.4 RHI Foundation
```
