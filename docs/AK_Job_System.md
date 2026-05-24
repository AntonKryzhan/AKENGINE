# AK Engine Job System Foundation

Version: AK Engine v2.0

## Goal

The job system exists so multithreading is not bolted on after the engine architecture is already sequential.
Systems should be designed around explicit work units, data ownership and synchronization points from the beginning.

## Problems fixed early

### Sequential update bottleneck

A renderer, asset pipeline, visibility system, animation system and physics preparation layer will all need CPU parallelism.
If every subsystem is written as a direct main-thread loop, parallelization later becomes a large refactor.

### Hidden synchronization

The engine needs a clear rule: jobs may run in parallel, but synchronization happens at known boundaries.
This avoids accidental waits in render, asset streaming, physics sync and editor update code.

### Frame spikes from serial work

Large arrays of entities, bounds, assets or transforms should be processed in batches through `ParallelFor`.
This reduces one-frame stalls and prepares the engine for task graph scheduling.

## Current implementation

The v2.0 layer provides:

- `JobSystemConfig`
- worker thread pool
- FIFO task queue
- `Submit`
- `ParallelFor`
- `WaitIdle`
- `JobSystemStats`
- `ak_jobprobe.exe`

This is intentionally small. It is not yet a full fiber scheduler or dependency graph.

## Current rules

1. Main thread owns editor UI and platform messages.
2. Jobs must not call Win32 UI code.
3. Jobs should operate on clearly partitioned ranges.
4. Job outputs must be written to separate slots or atomically merged.
5. Engine systems should avoid hidden heap allocation inside hot jobs.
6. `WaitIdle` is a synchronization boundary, not something to call in the middle of every subsystem.

## Future evolution

The current worker pool should evolve into:

- job counters
- task graph dependencies
- work stealing
- per-thread scratch allocators
- profiling zones per job
- main-thread-only command queue
- async asset IO jobs
- visibility/build-bounds jobs
- animation sampling jobs
- renderer prepare jobs

## Intended use in AK Engine

Near-term systems that should move to jobs:

- bounds rebuild and visibility classification
- asset scanning and import/cooking
- scene validation
- transform hierarchy rebuild
- animation sampling
- light clustering preparation
- texture/mesh CPU preprocessing

The goal is not to make every line multithreaded. The goal is to prevent the engine from being architecturally single-threaded.
