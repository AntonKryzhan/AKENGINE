# AK Engine v6.1 — Runtime Integration & Vulkan RHI Bootstrap

## Purpose

This patch moves AK Engine back to the main renderer line: runtime integration and a Vulkan-first backend foundation.

The goal is not to replace the existing Win32/GDI editor shell in one jump. The goal is to make Vulkan a first-class engine backend contract while preserving the current editor/player executables and probes.

## Added modules

- `engine/rhi`
- `engine/integration`
- `ak_rhiprobe`
- `ak_integrationprobe`

## Vulkan RHI foundation

`engine/rhi` defines the Vulkan bootstrap contract without requiring the Vulkan SDK headers at compile time.

It covers:

- backend selection: `RhiBackend::Vulkan`;
- native window surface descriptors;
- platform surface extension planning;
- loader probing through `vulkan-1.dll`, `libvulkan.so.1`, or `libvulkan.1.dylib`;
- validation/debug-utils policy;
- device extension requirements;
- queue family plan;
- frames-in-flight plan;
- swapchain policy;
- reversed-Z depth format policy;
- timeline semaphore / synchronization2 / dynamic rendering requirements.

The current renderer still keeps the GDI editor fallback path. On machines with the Vulkan runtime installed, the renderer can detect the loader and report the Vulkan bootstrap as available.

## Runtime frame integration

`engine/integration` defines the high-level frame order:

1. Input
2. Commands
3. Fixed tick clock
4. Physics and simulation
5. Gameplay events / damage
6. WorldPartition streaming
7. Asset residency
8. Visibility
9. Render extraction
10. RenderGraph build
11. Vulkan RHI submit
12. Present
13. Diagnostics and telemetry

This gives us one explicit place to keep system ordering stable while the renderer moves from stub/GDI to Vulkan.

## Why this order matters

- Simulation writes scene state before visibility.
- Streaming and residency are resolved before render extraction.
- RenderGraph is built before RHI submit.
- Present is only after GPU submit.
- Diagnostics is last, so it can read frame counters and warnings from all stages.

## What is intentionally not implemented yet

This patch does not create a real `VkInstance`, `VkSurfaceKHR`, `VkDevice`, or `VkSwapchainKHR` yet. That is the next patch. This patch prepares the ABI-safe boundary and runtime plan so the real Vulkan code can be added without changing editor/player architecture again.

## Next patches

Recommended sequence:

1. `v6.2 Vulkan Instance / Surface / Physical Device Selection`
2. `v6.3 Vulkan Logical Device / Queues / Allocator Foundation`
3. `v6.4 Vulkan Swapchain / Clear Frame`
4. `v6.5 Vulkan RenderGraph Execution`
5. `v6.6 Shader Pipeline: Slang/HLSL/SPIR-V cache`
6. `v6.7 ImGui Vulkan Editor Shell`
