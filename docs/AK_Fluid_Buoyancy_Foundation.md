# AK Engine v5.3 — Fluid / Buoyancy / Environment Interaction Foundation

This patch adds a first engine-native fluid interaction layer for gameplay physics, vehicles, characters, projectiles, debris and soft bodies.

It is not a full CFD solver. The goal is to provide deterministic, cheap and physically meaningful fluid volumes that can push rigid bodies through buoyancy, drag and flow without coupling the whole physics core to water-specific logic.

## Added module

```text
engine/fluid
```

## Main contracts

- `FluidMediumDesc`
- `FluidVolume`
- `FluidSample`
- `FluidColliderInteraction`
- `FluidBodyComponent`
- `FluidSystemStats`

## Supported fluid volumes

```text
Plane   — infinite water/ocean surface foundation
Box     — bounded pool/river/trigger-like volume
Sphere  — localized field, lava bubble, gas/magic volume foundation
```

## Supported media presets

```text
FreshWater
SeaWater
Oil
Mud
Lava
Gas
Custom
```

Each medium defines density, viscosity, drag, buoyancy scale, flow velocity and optional damage-on-contact.

## Physics integration

The system reads `PhysicsScene` bodies and colliders, estimates displaced volume/submerged fraction and applies forces through the physics API:

```text
PhysicsScene + FluidVolume[]
  -> collider submerged fraction
  -> displaced volume
  -> buoyancy force
  -> velocity-relative drag
  -> optional flow/current force
  -> ApplyForce(body)
  -> StepPhysics(scene)
```

This keeps water behavior engine-native while leaving the rigid-body solver generic.

## Current approximation

Submerged fraction is estimated from collider world bounds and shape volume. That is intentional for v5.3 because it gives stable behavior for box/sphere/capsule/proxy colliders without requiring triangle-level fluid clipping.

Later passes can replace this with exact waterline clipping for convex hulls, voxel/SDF volume integration and wave sampling without changing the public contracts.

## Probe

```text
ak_fluidprobe
```

The probe creates dynamic rigid bodies inside sea water and lava volumes, applies buoyancy/drag, steps physics and prints interaction stats.

## Next work

- Character swim controller.
- Vehicle floating/hydroplaning layer.
- Projectile underwater drag/cavitation bridge.
- Soft-body cloth wetness/drag coupling.
- Wave field sampling.
- Voxel/SDF exact displaced volume.
