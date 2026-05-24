# AK Engine v5.6 — Explosion / Shockwave / Blast Propagation Foundation

This patch adds the first engine-native explosion runtime layer. The goal is not a visual effect system, but a deterministic physics/damage bridge for blast events.

## Scope

The new `engine/explosion` module connects:

```text
ExplosionDesc
  -> radius / energy / falloff / occlusion
  -> affected physics bodies
  -> radial impulse
  -> DamageSystem explosion events
  -> DebrisSystem fragmentation emission
  -> PhysicsScene wake-up and fixed-step response
```

## Key concepts

- `ExplosionDesc` stores center, radius, energy, heat, impulse scale, damage scale and policy flags.
- `ExplosionFalloffKind` supports linear, smoothstep and inverse-square-style falloff.
- `ExplosionOcclusionPolicy::RaycastFirstHit` attenuates blast damage/impulse when another collider blocks line-of-sight.
- `ExplosionTarget` is gathered from `PhysicsScene` bodies and their collider bounds.
- `ExplosionHit` is the per-target result: distance, attenuation, visibility, impulse and generated damage event.
- `ExplosionResult` stores aggregate runtime statistics for diagnostics and tests.

## Integration points

- Physics: dynamic bodies receive radial velocity impulse and wake from sleep.
- Damage: blast hits can queue `DamageKind::Explosion` events through `DamageSystem`.
- Debris: fragmentation can spawn deterministic seeded debris through `DebrisSystem`.
- Queries: occlusion uses the physics raycast path instead of separate geometry code.

## Current limitations

This is a runtime foundation, not a final blast simulation. It does not yet model full fluid shock CFD, pressure-wave reflections, material-dependent blast shielding, shaped charges, explosive fragmentation ballistics or building-scale progressive collapse. Those should be layered on top of this API.

## Probe

`ak_explosionprobe` builds a small physics scene with dynamic targets, a static occluder, damageable targets and debris emission. It verifies:

- affected body detection;
- raycast occlusion attenuation;
- impulse application;
- damage queueing and processing;
- debris spawning;
- post-blast physics step validity.
