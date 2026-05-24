# AK Engine v4.1 Surface Material System Foundation

## Goal

The v4.1 layer introduces a single material contract for physics, terrain, planets, destruction, gameplay, footsteps and future rendering. The engine should not grow separate incompatible material tables for physics contacts, destructible objects, terrain splats and renderer material metadata.

## New module

`engine/surface`

The module owns:

- `SurfaceMaterialKind`
- `SurfaceDomain`
- `SurfaceMaterialId`
- `SurfacePhysicsDesc`
- `SurfaceRenderDesc`
- `SurfaceMaterialDesc`
- `SurfaceMaterialRegistry`
- `SurfaceSample`
- `SurfaceContactMaterial`

## Fixed decisions

- Density is stored in kg/m^3.
- Friction is split into static and dynamic friction.
- Restitution is normalized to `[0, 1]`.
- Destruction resistance and hardness are explicit physical/gameplay fields.
- Terrain and planet surfaces resolve through the same `SurfaceSample` path.
- Fluid surfaces are flagged and do not masquerade as solid collision materials.
- Surface IDs are stable `AssetGuid`-backed IDs, not raw file paths.

## Default materials

The default registry contains:

- default
- concrete
- wood
- metal
- glass
- terrain soil
- grass
- rock
- sand
- ice
- water
- rubber

These presets are intentionally conservative and deterministic. They are not final art materials; they are a foundation for editor-authored `.aksurface` assets later.

## Terrain and planet bridge

`BuildSurfaceSampleFromTerrain()` maps `TerrainSample` into weighted surface layers:

- flat terrain -> grass
- steep terrain -> rock
- mid-slope terrain -> soil/rock blend
- low terrain -> sand
- holes -> water

Planet terrain uses the same sample path with `SurfaceDomain::Planet`, so gameplay and physics do not need a separate planet material system.

## Physics bridge

`ApplySurfaceToPhysicsBody()` writes friction and restitution into `PhysicsBody`. For dynamic bodies it can also derive mass from density and object volume.

Physics contact resolution now applies a Coulomb-style tangent friction impulse using the mixed body friction. This makes the already existing `PhysicsBody::friction` field active instead of just stored metadata.

## Future extension points

Planned follow-up layers:

1. `.aksurface` serialized assets.
2. Material editor UI.
3. Terrain splat map material indices.
4. Destruction fragment material inheritance.
5. Footstep/audio/VFX event mapping.
6. Renderer material binding and shader keyword policy.
7. Runtime material override for gameplay zones.

## Probe

`ak_surfaceprobe` validates:

- default registry creation;
- terrain surface sampling;
- planet surface sampling;
- rubber/concrete contact mixing;
- density-to-mass application;
- physics tangent friction reducing lateral velocity.
