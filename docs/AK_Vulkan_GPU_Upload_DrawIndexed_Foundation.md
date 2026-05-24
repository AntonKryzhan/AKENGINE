# AK Engine v6.5 — Vulkan GPU Upload / DrawIndexed Foundation

This step connects the optimized polygon mesh stream from v6.4 to the next Vulkan backend stage.
It does not copy the OpenGL/voxel implementation style from `meshing-main`; it keeps the useful ideas:
combined geometry buffers, dirty sections, compact packed vertices, draw ranges, LOD ranges and meshlet ranges.

## Added foundation

- device-local combined vertex buffer plan;
- device-local combined index buffer plan;
- CPU staging upload ring plan;
- meshlet metadata storage buffer plan;
- indirect draw command buffer plan;
- per-frame uniform ring plan;
- object/material storage table plan;
- descriptor layout plan;
- shader cache contract for `assets/shaders/ak_primitive_mesh.slang`;
- indexed draw record generation from optimized render geometry;
- camera-relative/polygon-oriented upload contract.

## Why this matters

The renderer now has a stable contract for the first real Vulkan `VkBuffer` stage:

```text
OptimizedRenderGeometry
  -> PackedRenderVertex32 stream
  -> combined vertex/index buffers
  -> staging copy plan
  -> descriptor/uniform plan
  -> vkCmdBindVertexBuffers / vkCmdBindIndexBuffer / vkCmdDrawIndexed plan
```

This is the correct bridge between CPU-side mesh optimization and the real Vulkan graphics pipeline.
The next patch can replace this plan with actual `VkBuffer` creation, staging memory, buffer copies and command recording.

## Deliberate non-goals

- no OpenGL-style VAO/VBO abstraction;
- no cube-only vertex format;
- no per-object buffer allocation;
- no immediate-mode draw path;
- no shader permutation explosion;
- no GPU readback in the frame.
