# AK Engine v6.4 — Render Mesh Optimization / meshing-main Adaptation

This patch imports the useful optimization philosophy from `meshing-main` into AK Engine without copying its OpenGL/voxel-only architecture.

## What was studied

The source project is a compact voxel renderer using greedy meshing. The useful engineering ideas are:

- avoid scanning empty space by using min/max height ranges;
- avoid reprocessing faces by using directional visited masks;
- merge contiguous faces into larger runs;
- allocate temporary meshing data from reusable storage;
- rebuild only dirty chunks;
- render front-to-back according to camera direction;
- upload one generated mesh buffer per chunk;
- use a compact vertex layout and derive some data in shader.

## What AK Engine changes

AK Engine is not a cube-only world renderer. The adaptation therefore targets general polygon meshes:

- generic `PrimitiveMesh` input, not voxel-only chunks;
- combined Vulkan-friendly vertex/index buffer plan;
- deterministic quantized vertex welding;
- 32-byte packed render vertex contract instead of 48-byte full-float vertices;
- LOD index ranges for future streaming/visibility budgets;
- meshlet ranges for future GPU culling, indirect drawing and mesh shading;
- front-to-back draw command ordering independent of OpenGL state;
- dirty render sections prepared for partial rebuilds and partial Vulkan buffer updates.

## Vulkan advantage over the OpenGL source

The OpenGL project naturally uploads and draws one chunk-style buffer through VAO/VBO state. AK Engine should instead move toward:

```text
many mesh sections
  -> one combined vertex buffer
  -> one combined index buffer
  -> draw command ranges
  -> meshlet ranges
  -> indirect draw / GPU culling later
```

This avoids excessive binding churn and prepares the renderer for RenderGraph, streaming, GPU-driven culling and virtualized geometry.

## Current scope

This patch is still CPU-side renderer foundation. It does not yet upload the optimized buffer to Vulkan GPU memory. It creates and validates the contracts that the next Vulkan draw patch should consume.

## Next steps

- allocate real Vulkan vertex/index buffers;
- staging upload into device-local memory;
- descriptor/push-constant camera data;
- `vkCmdDrawIndexed` over optimized draw commands;
- material-sorted and front-to-back draw buckets;
- GPU culling over meshlets;
- QEM-based polygon LOD generation.
