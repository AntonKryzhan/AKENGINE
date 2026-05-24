# AK Engine v4.0 — Destruction Remesh Foundation

This stage adds the first generated surface-mesh path for CSG/destruction output.
The goal is not final production-quality CAD remeshing. The goal is to remove the next architectural limit: destruction results must no longer stop at voxel grids, proxy AABBs, and generated asset handles.

## Problem fixed early

Before this patch, the CSG/destruction bridge could generate:

```text
CSG voxel result
  -> collision proxies
  -> physics colliders
  -> bounds update
  -> generated mesh AssetGuid / ResourceHandle
  -> WorldPartition dirty cell
```

But the generated mesh resource did not have a concrete mesh extraction foundation. That would become a hard blocker for renderer integration, asset cooking, editor previews, and future remesh quality upgrades.

## Added module

```text
engine/remesh
```

The module owns generated surface extraction policy and output mesh metadata.

## Implemented path

```text
CsgVoxelGrid
  -> boundary solid voxel faces only
  -> quad surface faces
  -> deterministic vertices / triangles
  -> face normals
  -> closed-surface validation by lattice edge use counts
  -> GeneratedSurfaceMesh
```

The implemented algorithm is intentionally named:

```text
RemeshAlgorithm::VoxelFaceSurface
```

It is stable, deterministic, debuggable, and safe as a foundation mesh. It is not the final visual remesh for production destruction.

## Planned paths

The API already reserves explicit future algorithms:

```text
RemeshAlgorithm::MarchingCubesPlanned
RemeshAlgorithm::DualContouringPlanned
```

If either planned algorithm is requested today, the system falls back to `VoxelFaceSurface` and records a warning. This prevents silent behavior changes and keeps generated mesh contracts stable.

## Data contracts

### RemeshSettings

Controls:

```text
algorithm
normal mode
max vertices
max triangles
boundary-only extraction
closed-surface validation
```

### GeneratedSurfaceMesh

Stores:

```text
vertices: position + normal + uv
triangles: 3 uint32 indices
bounds: local generated mesh AABB
```

### RemeshStats

Tracks:

```text
solid voxels
exposed faces
interior faces skipped
vertices
triangles
estimated bytes
non-manifold edges
closed surface flag
mesh budget clipping
warnings
```

## Validation

The `ak_remeshprobe` tool creates a CSG difference, extracts a generated surface, and verifies:

```text
CSG result is valid
surface has exposed faces
vertex count == exposed faces * 4
triangle count == exposed faces * 2
mesh bounds are valid
surface lattice is closed
budget was not clipped
```

## Why this must exist before Vulkan

A Vulkan renderer should not receive ad-hoc generated geometry directly from CSG or physics. Generated geometry needs a stable intermediate representation first:

```text
CSG / destruction output
  -> remesh module
  -> generated mesh asset/resource
  -> render proxy
  -> GPU upload / streaming later
```

This keeps render, physics, and destruction separated:

```text
physics uses collision proxies
renderer uses generated surface mesh
asset pipeline owns persistent/cooked form later
```

## Known limits

Current v4.0 remesh output is blocky voxel-surface geometry:

```text
no vertex welding in output buffer
no smoothing
no material boundary stitching
no UV unwrap
no LOD generation
no marching cubes
no dual contouring
```

These are deliberate limits of the foundation stage. The important part is that the engine now has a correct module boundary and deterministic generated mesh contract.
