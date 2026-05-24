# AK Engine v5.7 — Force Fields / Volumes / Area Effects Foundation

This patch adds a shared force-field orchestration layer for physics gameplay volumes.

The goal is to avoid duplicating volume logic across gravity modifiers, wind zones, aftershock fields, local attractors, damage-over-time hazards and trigger-like gameplay areas.

## Added module

```text
engine/forcefield
```

## Supported field kinds

- directional acceleration
- directional force
- point attractor
- point repulsor
- vortex
- linear drag
- wind
- damage-over-time

## Supported volumes

- global
- sphere
- box
- half-space

## Integration

`ApplyForceFieldsToPhysicsScene()` samples enabled fields against `PhysicsScene` bodies/colliders and can:

- apply forces through the physics API;
- wake dynamic bodies;
- respect physics layer masks;
- queue `DamageEvent` records into `DamageSystem`;
- produce deterministic samples and stats for debug/probes.

This is intentionally an orchestration layer. It does not replace `engine/gravity`, `engine/fluid` or `engine/explosion`; it provides a common volume/effect contract that those systems can use or mirror later.

## Probe

```text
ak_forcefieldprobe
```

The probe validates gravity-like modifiers, local attractors, wind corridors, vortex afterflow and damage zones in one deterministic scene.
