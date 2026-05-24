# AK Engine Memory Foundation

AK Engine v2.1 adds the first explicit memory layer. The goal is not to replace every allocation immediately, but to establish the contracts before the renderer, asset cooker, streaming and ECS hot paths become large.

## Problems fixed early

### Heap allocation spikes

Realtime systems should not allocate unpredictably in hot loops. A random `new`, `delete`, `std::string` growth or container resize inside frame code can produce visible hitches.

The first rule is:

```text
hot frame path -> arena/linear/pool allocation
editor/tools diagnostics -> tracking allocation allowed
```

### Memory leaks hidden by editor lifetime

Editors often run for hours. A small leak per import, hot reload or scene edit becomes a practical stability bug. `TrackingAllocator` records active allocations, peak usage and leak records.

### Fragmentation

General-purpose heap allocators are flexible but not ideal for thousands of transient objects. `LinearAllocator` gives a deterministic per-frame scratch arena that can be reset in O(1).

### Future GPU lifetime bugs

The memory layer complements v1.6 resource handles. CPU memory ownership and GPU resource lifetime must be explicit. Future Vulkan/DX12 buffers will use generation handles plus deferred release queues.

## Added primitives

```text
MemoryTag
MemoryStats
TrackingAllocator
LinearAllocator
LinearAllocator::Marker
MemoryProbeResult
```

## Intended usage

```text
Frame scratch      -> LinearAllocator reset once per frame
Asset import       -> Arena/linear allocator per import job
ECS chunks         -> future fixed-size chunk allocator
Render staging     -> future ring buffer allocator
Diagnostics/tools  -> TrackingAllocator
```

## Probe

Run:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_memoryprobe.exe
```

Expected result contains:

```text
Memory probe: ok
current=0
active=0
```

That means the tracking allocator released all allocations and the linear arena returned to zero after reset.
