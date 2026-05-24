# AK Engine v5.8 — Physics Debug Draw / Runtime Visualization Foundation

This patch adds a renderer-agnostic debug draw command layer for physics and runtime simulation systems.

The goal is to avoid wiring debug visualization directly into future Vulkan, ImGui, editor, gameplay, or probe code. Systems emit deterministic draw commands into `DebugDrawList`; a renderer/editor backend can later consume the command stream.

## Covered systems

- physics bodies and colliders;
- broadphase pairs and bounds;
- contacts and contact manifolds;
- raycast and sphere-sweep queries;
- projectile trajectory, hit and events;
- fluid volumes and fluid forces;
- debris particles and velocity vectors;
- explosion radius, occlusion rays and impulse vectors;
- force-field volumes, samples and damage points.

## Command types

- `Line`;
- `Arrow`;
- `Point`;
- `Aabb`;
- `Obb`;
- `Sphere`;
- `Capsule`;
- `Text`.

## Architecture

```text
simulation systems
  -> Collect*DebugDraw(...)
  -> DebugDrawList commands
  -> future editor viewport / Vulkan overlay / capture tools
```

The command list is intentionally data-only. It does not own GPU buffers, ImGui widgets, Vulkan resources, editor state, or platform windows.

## Runtime safety

- non-finite commands are rejected;
- max command budget clips oversized debug streams;
- categories can be enabled/disabled by mask;
- commands carry lifetime and space policy for future render backends;
- physics/editor/gameplay can consume the same stream without calling private subsystem internals.

## Probe

`ak_debugdrawprobe` builds representative physics, projectile, fluid, debris, explosion, and force-field probe scenes, collects visualization commands, and validates command counts and finite data.
