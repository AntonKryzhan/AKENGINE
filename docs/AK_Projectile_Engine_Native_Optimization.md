# AK Engine v4.4 — Projectile Engine-Native Optimization Pass

This stage turns the v4.3 projectile fidelity layer into an AK-native physics/math subsystem rather than a Unity-style gameplay port.

## Goals

- Keep projectile authority in Large World Coordinates.
- Simulate physics locally around a physics-island origin to avoid float jitter.
- Sample AK gravity fields instead of a single hardcoded gravity vector.
- Use continuous projectile sweep/thickness queries as the default collision bridge.
- Add a data-oriented voxel occupancy workset for future jobified damage.
- Expose an ECS-style fixed-update shell for projectiles.

## Added contracts

### ProjectileWorldState

`ProjectileWorldState` stores:

- authoritative `WorldPosition`;
- previous `WorldPosition`;
- physics-island origin;
- double-precision velocity and angular velocity;
- cell size and rebase threshold state.

`ProjectileBody` remains the local fast simulation payload. `SyncProjectileBodyFromWorldState()` and `SyncProjectileWorldStateFromBody()` convert between the two.

### Gravity integration

`SampleProjectileGravity()` accepts AK `GravityFieldDesc` objects and falls back to `ProjectileEnvironment.gravityMetersPerSecondSquared` only when no fields are supplied. This prepares bullets, shells and debris for:

- planet gravity;
- point gravity;
- zero gravity;
- spherical zones;
- combined gravity fields.

### Native step

`StepProjectileNative()` performs:

1. optional physics-origin rebase;
2. gravity-field sampling;
3. body/world synchronization;
4. existing high-fidelity projectile step;
5. synchronization back to Large World Coordinates;
6. precision-risk reporting.

### Multi-hit continuous sweep

`SweepProjectileMultiHitAgainstPhysicsScene()` returns sorted hits along a segment and computes thickness for each hit. This is the foundation for multi-layer penetration, spaced armor and material stacks.

### Voxel bitset workset

`VoxelBitsetOccupancy` stores voxel occupancy as packed `u64` words. `VoxelDamageWorkset` records chunk counts, dirty chunks and memory usage so voxel damage can later be dispatched through the job system without reshaping the API.

### Projectile system shell

`ProjectileSystemFixedUpdate()` updates a vector of `ProjectileComponent` objects through the native step, collision sweep, impact solver and trajectory events. It is intentionally component-oriented but does not require the full ECS query layer yet.

## Probe

`ak_projectileprobe` validates:

- high-fidelity projectile step;
- AK gravity-field sampling;
- Large World Coordinate sync;
- multi-hit collision sweep;
- impact/penetration;
- voxel tunnel damage;
- RLE and bitset voxel roundtrip;
- projectile fixed-update system shell.
