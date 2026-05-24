# AK Engine v2.7 — Streaming Foundation

## Purpose

The streaming layer prevents runtime code from blocking directly on raw files, loose asset paths, or package offsets. Game/editor systems request assets by `AssetGuid` or logical path. The streaming system reads through the Virtual File System, respects a per-update budget, and creates generation-checked resource handles through `ResourceManager`.

## Problems fixed early

- Blocking reads in gameplay/editor hot paths.
- Unbounded asset loading spikes.
- Runtime code depending on whether data is loose or inside `.akpak`.
- Resource lifetime bugs during load/unload/hot-reload.
- Missing budget diagnostics for large open-world streaming.

## Current implementation

`StreamingSystem` is intentionally deterministic and small:

```text
RequestLoad -> queued request
Update      -> budgeted VFS read -> ResourceManager::Create
RequestUnload -> ResourceManager::RequestDestroy
```

This is not the final async IO backend yet. It is the contract that future async loading, decompression, residency and GPU upload queues will obey.

## Next steps

- Add async IO worker queue.
- Add decompression jobs.
- Add resource residency groups.
- Add priority from camera distance / world cells.
- Add streaming budgets per type: mesh, texture, audio, shader.
- Feed GPU upload queues with frame-in-flight fences.
