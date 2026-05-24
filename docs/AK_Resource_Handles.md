# AK Engine Resource Handles

AK Engine v1.6 adds the first resource lifetime layer. The goal is to remove a future class of use-after-free bugs before Vulkan/DX12 resources appear.

## Problem

A renderer cannot destroy GPU resources immediately when gameplay/editor code drops a reference. The GPU may still read an image, buffer, acceleration structure, descriptor, or pipeline from older frames in flight.

Typical failures:

- a material still references an unloaded texture;
- a render proxy points to a destroyed mesh buffer;
- asset hot reload replaces a resource while the previous frame is still on GPU;
- a stale handle accidentally resolves to a newly-created resource;
- a resource is destroyed before fences say the GPU is finished.

## Current v1.6 solution

ResourceManager stores resources behind generation handles:

```text
ResourceHandle = index + generation
```

A handle is valid only while its slot generation matches. When a resource is finally released, the slot generation increments. Old handles stop resolving.

Resource deletion is deferred:

```text
RequestDestroy(handle, frame)
  -> mark pending
  -> release after frame + deferredFrameLag
```

This is not a full renderer resource system yet. It is the lifetime invariant that the renderer, streaming system, hot reload, and asset pipeline will use later.

## Runtime contract

- CPU code should use `IsAlive(handle)` for normal resource references.
- Low-level renderer/backend may use `IsResident(handle)` while a pending resource is still protected for frames in flight.
- Releasing a resource does not immediately recycle the slot.
- Recycled slots always receive a new generation.

## Future integration

Next layers will connect this to:

- AssetGuid -> ResourceHandle cache;
- mesh/material/texture runtime resources;
- deferred GPU destruction queues;
- frame fences/timeline semaphores;
- asset hot reload;
- streaming residency budgets.
