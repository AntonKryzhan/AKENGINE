# AK Engine v1.7 Render Foundation

This stage adds renderer-level invariants before a real Vulkan/DX12 backend exists.
The goal is to remove architectural traps early, while the current editor still uses the temporary Win32/GDI shell.

## Fixed architectural risks

### 1. Standard depth precision collapse

A normal depth buffer loses useful precision when `far / near` becomes large. Large worlds make this visible as:

- z-fighting;
- unstable decals;
- shadow acne/peter-panning getting worse with distance;
- distant surfaces flickering even if world coordinates are precise.

AK Engine now has an explicit `DepthPolicy` and default policy:

```text
reversed-Z
floating point depth
near = 0.05 m
infinite far-plane ready
```

The future Vulkan projection layer must use this policy instead of ad-hoc projection matrices.

### 2. Absolute world coordinates on GPU

Large world coordinates must never be uploaded to GPU as raw absolute positions. The renderer foundation now exposes camera-relative conversion:

```text
CPU world:  int64 cell + double local
GPU input:  float(cameraRelativePosition)
```

This protects vertex transforms, culling, picking overlays and debug drawing from float jitter.

### 3. Untracked render pass lifetime

Manual render pass sequencing later becomes unmaintainable in Vulkan/DX12 because resource states and barriers are explicit.

AK Engine now has a small `RenderGraphPlan` skeleton:

```text
resource declarations
pass declarations
read/write lists
transient/imported resource flags
basic validation
estimated barrier count
```

This is not yet the production RenderGraph. It is the contract that future Vulkan code must grow into.

## Default graph probe

The first graph contains:

```text
resources:
  swapchain
  scene_color
  scene_depth
  ui_color

passes:
  depth_prepass
  main_color
  editor_ui
  present
```

## Probe

Run:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_renderprobe.exe
```

Expected result:

```text
[ ok ] reversed-Z ...
[ ok ] deviceZ near=1 far≈0
[ ok ] render graph passes=4 resources=4 ... warnings=0
```

## Rule going forward

Renderer code must not create arbitrary pass order or raw GPU lifetime ownership outside the graph/resource systems.
Vulkan integration should implement these contracts, not bypass them.
