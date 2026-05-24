# AK Engine Diagnostics Foundation

Version: 2.2.0

This layer gives the engine a small built-in diagnostics surface before the renderer, asset pipeline and simulation become complex.

## Problems fixed early

### Hidden frame spikes

Subsystems must expose timing zones instead of relying on visual guessing. The first implementation records named profile zones with call count, last time, average time and max time.

### Missing counters

Editor/runtime state should be visible as numeric counters: entity count, visible count, pending jobs, resident resources and similar values.

### Unbounded diagnostic spam

Diagnostic events are stored in a bounded ring buffer. When the buffer is full, old events are dropped and `droppedEvents` is incremented. This prevents a broken system from growing memory usage through diagnostics alone.

### Profiling added too late

The rule from this version forward: every heavy subsystem must be able to report timing/counter diagnostics before it becomes production-critical.

## Current API

```cpp
AK::DiagnosticsHub diagnostics;
diagnostics.BeginFrame(frameIndex);

{
    AK::ProfileScope scope(diagnostics, "Renderer.Prepare");
    // work
}

diagnostics.SetCounter("visible", visibleCount);
diagnostics.AddEvent(AK::DiagnosticSeverity::Warning, "Assets", "missing preview texture");
diagnostics.EndFrame();

AK::DiagnosticsSnapshot snapshot = diagnostics.Snapshot();
```

## Current editor integration

The editor records:

```text
Editor.SceneUpdate
Editor.RenderShell
Editor.Frame
entities
assets
visible
culled
resources.resident
jobs.pending
```

## Future extensions

```text
CPU thread timeline
Tracy/export bridge
GPU timestamp zones
crash dump metadata
assert history
per-subsystem budgets
frame hitch detector
log categories routed into diagnostics events
```
