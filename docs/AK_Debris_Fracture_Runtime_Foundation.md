# AK Engine v5.4 — Debris / Particles / Fracture Runtime Foundation

This patch adds the first engine-native runtime layer for physical debris produced by CSG destruction, projectile voxel damage, terrain cuts, fluid splashes, and future fracture systems.

The goal is not a renderer particle system. It is a physics-facing debris foundation that can later feed renderer instances, audio, decals, gameplay damage, and persistent destruction.

## Added module

```text
engine/debris
```

## Main responsibilities

- spawn impact-cone debris from physical hits;
- spawn debris from projectile voxel damage candidates;
- deterministic seeded emission;
- per-particle mass from size and material density;
- lifetime and sleeping policy;
- gravity integration;
- scene collision via `PhysicsScene` sphere sweep;
- fluid interaction through `engine/fluid` sampling;
- conversion of large debris into temporary `PhysicsScene` proxy bodies;
- stats and probe coverage.

## Important contracts

Debris is intentionally not stored as Unity-like `GameObject` particles. Each particle is a compact engine-side record:

```text
DebrisParticle
  id
  state
  kind/material
  position / previousPosition
  velocity / angularVelocity
  bounds radius / halfExtents
  mass / inverse mass
  lifetime / sleep state
  source body id
```

Runtime spawning uses `DebrisEmitterDesc`, which can represent:

```text
ImpactCone
Explosion
VoxelCandidates
```

## Integration points

```text
Projectile voxel damage
  -> VoxelDebrisCandidate[]
  -> SpawnDebrisFromVoxelDamage()
  -> DebrisSystem particles
  -> optional PhysicsScene proxy bodies
```

```text
Fluid volumes
  -> SampleFluidAtPoint()
  -> buoyancy + drag on debris particles
```

```text
PhysicsScene
  -> SweepSpherePhysicsScene()
  -> bounce/friction/sleep
```

## Current limitations

This is a CPU foundation. It does not yet implement GPU particles, fracture mesh rendering, instanced renderer submission, debris pooling in the resource manager, or persistent save/load of debris fields.

Next useful steps:

- renderer instance stream for debris;
- persistent debris chunks for world partition cells;
- jobified particle batches;
- GPU/CPU LOD policy;
- fracture surface material tagging;
- debris audio/decals event routing.
