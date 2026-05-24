# AK Engine v7.2 — PointCloud Runtime Streaming / Residency Foundation

This patch adds the runtime-side layer for cooked massive point-cloud data. The v7.1 cook manifest describes pages, LODs, payload sizes, attribute layout and deterministic hashes. v7.2 turns that manifest into a frame-local execution plan: which pages are relevant near the camera, which attribute streams the active material actually needs, which payloads must be read, which pages are uploaded to the future Vulkan storage-buffer path, which pages draw this frame, and which stale pages are evicted.

## Goals

- Keep AK Engine independent from udSDK while preserving an optional bridge path later.
- Convert cooked page manifests into deterministic runtime streaming plans.
- Support material-driven attribute streaming: position is always required, but RGB/intensity/classification/height/custom streams are requested only when the material needs them.
- Enforce streaming byte budget, upload byte budget, page budget and point budget.
- Model out-of-core residency with `VirtualChunkCache` and deterministic LRU spill behavior.
- Produce future renderer commands without requiring a real Vulkan backend yet.

## New API

```text
AK/PointCloud/PointCloudRuntime.hpp
```

Main structs:

```text
PointCloudRuntimeFrameDesc
PointCloudRuntimeResidencyPolicy
PointCloudRuntimePage
PointCloudRuntimeCommand
PointCloudRuntimeStats
PointCloudRuntimePlan
```

Main function:

```text
BuildPointCloudRuntimePlan(manifest, frame, policy)
```

Runtime command types:

```text
RequestPage
ReadPayload
UploadGpuBuffer
BindAttributeStreams
DrawPage
EvictPage
```

## Why this matters

The point-cloud path now has a complete early pipeline:

```text
source scan / DEM / LAS / LAZ / E57 / UDS-like
    -> v7.1 cook manifest
    -> v7.2 runtime page selection
    -> out-of-core payload residency
    -> future Vulkan storage-buffer upload
    -> future GPU-driven point rendering
```

This removes the main architectural trap of point-cloud rendering: loading the whole dataset and all attributes into RAM/VRAM. AK Engine can instead stream only visible pages and only the attributes needed by the active visualization material.

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_pointcloudruntimeprobe
.\build\windows-vs-debug\bin\Debug\ak_pointcloudruntimeprobe.exe
```

Expected behavior:

- Builds a synthetic cooked LAZ-like manifest.
- Requests classification material streams.
- Excludes RGB because classification mode does not need it.
- Selects runtime pages under byte/page/point budgets.
- Emits request/read/upload/bind/draw/evict commands.
- Reports ready-for-render when at least one page reaches GPU-resident draw state.
