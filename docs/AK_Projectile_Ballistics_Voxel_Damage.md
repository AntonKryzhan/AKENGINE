# AK Engine v4.2/v4.3 — Projectile Ballistics & Voxel Damage

The first projectile patch introduced the module. The v4.3 fidelity pass expands it to carry the complete useful logic from the supplied Unity ballistic and voxel-wall scripts as deterministic C++ engine code.

## What is now present

- projectile FSM;
- damage/weapon preset matrix;
- Mach drag table;
- full environment preset set;
- density and speed-of-sound update by altitude;
- seeded wind, gust and OU-like turbulence;
- midpoint integration;
- gravity, drag, Magnus and Coriolis forces;
- angular stabilization;
- thermal cooling and mass erosion;
- impact force, penetration, ricochet, embed/stuck;
- material deformation and accumulated damage;
- mass loss after penetration;
- thickness query for override, AABB, oriented box, sphere, physics collider and voxel grid;
- physics scene sweep integration;
- voxel DDA damage;
- unsupported voxel flood fill;
- debris candidate extraction;
- exposed-face count;
- RLE compression;
- trajectory samples/events and Catmull-Rom debug interpolation.

## Engine layering

```text
projectile ballistics
  -> physics sweep / hit
  -> thickness query
  -> surface material impact
  -> material damage state
  -> voxel damage / debris candidates
  -> trajectory events
```

Unity-specific gameplay code such as camera recoil, cursor handling, audio and weapon UI remains outside the engine core.
