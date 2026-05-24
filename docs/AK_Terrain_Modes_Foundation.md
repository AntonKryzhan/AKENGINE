# AK Engine v3.3 — Terrain Modes Foundation

This patch adds the first terrain policy layer. The engine must not treat terrain as one flat heightmap forever. AK Engine now has an explicit place for terrain mode decisions before renderer, physics, navigation, streaming and editor tools grow around wrong assumptions.

## Supported terrain modes

- `Heightfield` — classic chunked terrain: heightmap, splatmap, grass, trees, collision heightfield.
- `PlanetCubeSphere` — spherical planet terrain using six cube-sphere faces and quadtree-like LOD tiles.
- `Voxel` — editable volume terrain for caves, destruction and digging.
- `Mesh` — authored terrain sections imported from DCC tools.
- `Procedural` — deterministic height generation from seed and generation version.
- `Toroidal` — Pac-Man style wrap-around terrain where samples repeat across world edges.

## Why this matters

A game engine should not hardcode `terrain = flat XZ heightmap`. Planet-scale worlds, spherical gravity, wrap-around maps, destructible caves and authored terrain meshes require different storage, sampling, collision, navigation and streaming policies.

The new module is still intentionally small, but it defines the contracts:

- terrain configs are mode-specific;
- terrain samples return height, normal, slope and hole state;
- terrain patches have stable ids, LOD, residency state and estimated bounds;
- planet terrain uses geodetic coordinates and cube-sphere faces;
- toroidal terrain wraps sample coordinates through topology rules;
- procedural terrain is deterministic from a seed.

## Future integration

Next steps should connect this foundation to:

- World Partition cell selection;
- async streaming and resource residency;
- collision heightfields / voxel collision / mesh collision;
- navigation tile baking;
- renderer terrain patches;
- biome/climate rules;
- editor terrain mode selector.
