# AK Engine v4.3 — Projectile Fidelity Integration

This patch replaces the earlier projectile foundation with a fidelity pass based on the supplied Unity ballistic and voxel-wall scripts. The goal is not to keep Unity's `MonoBehaviour`/`Rigidbody` architecture, but to preserve the ballistic logic as deterministic C++ engine code connected to AK systems.

## Ported from the original ballistic script

- Full projectile FSM: `Flying`, `Ricocheted`, `Penetrated`, `Embedded`, `Stuck`, `Fragmented`, `Melted`, `Destroyed`.
- Full `DamageType` and `WeaponType` preset matrix from the original `BulletPreset` logic.
- Projectile mass from radius, length and density.
- G7-like Mach drag table with interpolation.
- Full environment preset list: open field, forest, urban, indoor, mountains, desert, storm, high altitude, jungle, arctic, city high-rise, indoor hall, cave, coastal, snowstorm, shallow/deep underwater, space and space station.
- ISA-style air density and speed-of-sound update with altitude.
- Deterministic OU-style turbulence plus smooth gust noise, replacing Unity `Random`/`PerlinNoise` with seeded engine code.
- Mean wind, turbulence velocity and final wind vector.
- Magnus force.
- Coriolis force.
- Midpoint ballistic integration.
- Angular stabilization toward velocity direction.
- Thermal cooling through radiation and convection.
- Mass erosion and latent heat energy loss.
- Temperature-dependent yield-strength helper.
- Ricochet / penetration / embed / stuck impact solver.
- Impact force calculation.
- Material deformation and accumulated damage state.
- 2% mass loss on penetration.
- Trajectory samples and trajectory events.
- Catmull-Rom trajectory interpolation for debug/editor visualization.

## Ported from the voxel wall scripts

- 3D-DDA tunnel traversal through a voxel grid.
- Spherical cut at each visited DDA cell.
- Flood-fill removal of unsupported voxels.
- Debris candidate extraction.
- Exposed-face counting.
- RLE voxel occupancy compression and decompression.

## New AK Engine integrations

```text
ProjectileBody
  -> ProjectileEnvironment / gravity / wind / turbulence
  -> PhysicsScene swept collision query
  -> ProjectileThicknessQuery
  -> SurfaceMaterial -> ProjectileImpactMaterial
  -> impact result / material damage / mass loss
  -> CSG voxel damage tunnel
  -> debris candidates
  -> trajectory events
```

The projectile module now includes physics-scene sweep support for sphere and AABB-like physics colliders. Impact thickness is resolved through a formal query instead of being passed as a raw number only. Supported thickness paths are override, AABB projection, oriented box projection, sphere chord, physics collider and voxel-grid scan.

## Important deviation from the Unity script

The Unity code computes the impact plane angle with `Abs(Angle(direction, normal) - 90)`, which makes a frontal hit report as `90°` when the hit normal points against projectile travel. In this C++ port the solver uses physically stable obliquity: `0°` is frontal, `90°` is grazing. This preserves the stated intent in the Unity comment and avoids zero normal impulse on direct hits.

## Probe

`ak_projectileprobe` verifies:

- full preset path;
- storm environment with wind/turbulence;
- ballistic step with drag, Magnus, Coriolis, thermal and erosion hooks;
- physics-scene hit;
- thickness query;
- surface material impact;
- penetration with mass loss;
- voxel DDA damage;
- debris candidate extraction;
- RLE roundtrip;
- trajectory event generation.
