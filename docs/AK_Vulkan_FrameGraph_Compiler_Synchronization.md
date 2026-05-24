# AK Engine v6.8 — Vulkan FrameGraph Compiler / Synchronization2 / Transient Aliasing Foundation

This patch adds the renderer-side frame graph compiler that turns declared render passes and resource accesses into an executable Vulkan-oriented plan.

The goal is to keep Vulkan explicit without making every renderer feature manually write barriers, queue ownership transitions, transient resource lifetimes, and submit batches.

## What this layer owns

- Render resource declarations for buffers, images, depth images and swapchain images.
- Per-pass resource access declarations.
- Synchronization2-style barrier planning.
- Image layout transition planning.
- Buffer hazard planning.
- Queue ownership transition detection.
- Transient resource lifetime calculation.
- Transient alias group packing.
- Per-queue submit batch planning.
- Secondary command buffer planning for draw-heavy graphics passes.
- Validation for missing resources, invalid layouts, read-before-write and alias hazards.

## Why it exists before real VkBuffer/VkPipeline draw

Vulkan performance collapses when every pass manually emits barriers or when resources are destroyed/transitioned without a global view of the frame. The graph compiler gives the RHI a stable input:

```text
Renderer declares intent
  -> resources
  -> passes
  -> reads/writes/access/layout
  -> compiler
  -> barriers + lifetimes + batches
  -> real VkCommandBuffer recording
```

The compiler is intentionally CPU-side and deterministic. It does not create Vulkan handles yet, but the generated data maps directly to:

```text
VkBufferMemoryBarrier2
VkImageMemoryBarrier2
VkDependencyInfo
vkCmdPipelineBarrier2
vkCmdBeginRendering
vkCmdExecuteCommands
vkQueueSubmit2
```

## Default graph

The default probe builds this frame:

```text
acquire_swapchain
upload_geometry_and_tables
depth_prepass
forward_primitive_lit
visibility_reduce_compute
tonemap_to_swapchain
present_swapchain
```

This deliberately includes graphics, transfer, compute and present queues so the compiler is forced to model cross-queue hazards early.

## Transient aliasing

Transient images are assigned lifetimes and packed into alias groups when their pass ranges do not overlap. This is the foundation for future transient image heaps used by RenderGraph.

## Next step

The next Vulkan patch can consume this compiled graph to create real Vulkan command buffer recording code around the existing primitive mesh draw path.
