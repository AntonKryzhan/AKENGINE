# AK Engine v3.2 — Gravity Fields & Surface Movement Foundation

This layer removes the hardcoded assumptions that usually break engines when they move from flat levels to planets, space, wrapped maps, or gravity zones.

## Core rule

Engine systems must not assume:

```text
up = +Y
gravity = -Y
movement plane = XZ
terrain = flat heightfield only
```

Instead they must sample local gravity and local surface frame.

## Gravity field types

```text
Uniform
  Classic gravity, usually (0, -9.80665, 0).

Planet
  Gravity points toward a planet center. Supports inverse-square falloff.

Point
  Small planet / asteroid / attractor gravity.

SphericalZone
  Local gravity volume with falloff.

Zero
  Space / no gravity zones.
```

## Surface movement modes

```text
PlanarSurface
  Standard flat terrain movement.

PlanetSurface
  Movement in a tangent frame on a sphere:
    east / north / up / gravity

ToroidalSurface
  Pac-Man style wrap-around movement.

FreeSpace
  Zero-G or unconstrained movement.
```

## Why this is early-core work

If gameplay, physics, camera, AI and animation start with hardcoded global Y, later spherical planets become a full-engine rewrite. This module establishes the contract before the physics backend exists.

## Current implementation

```text
engine/gravity
  GravityFieldDesc
  GravitySample
  SurfaceAttachmentDesc
  SurfaceMovementInput
  SurfaceMovementResult

ak_gravityprobe.exe
  samples uniform / planet / point gravity
  moves on a spherical planet tangent-frame
  verifies toroidal wrapping movement
```

## Later integrations

```text
vNext physics
  character controller uses GravitySample.up
  rigid bodies use sampled acceleration
  physics islands rebase in local frames

vNext navigation
  nav agents move on SurfaceFrame instead of XZ plane

vNext animation
  feet and character root align to local up vector

vNext camera
  planet camera orbits in local surface frame
```
