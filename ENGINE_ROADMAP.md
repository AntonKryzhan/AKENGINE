# AK Engine Roadmap

This roadmap is a local planning file for Codex-driven patches. It is not a marketing roadmap. Prefer small, testable, architecture-safe patches that move one layer forward without breaking older scenes/tools.

## Current engine direction

AK Engine is being built as a custom Windows-first C++ engine with:

- Unity-like editor ergonomics without copying Unity internals;
- Vulkan-first renderer and future Slang/HLSL/SPIR-V shader pipeline;
- explicit RenderGraph/RHI/resource lifetime contracts;
- Large World Coordinates for planet-scale worlds;
- world topology policies: planar terrain, spherical planets, toroidal wrap maps;
- gravity fields instead of hardcoded `-Y` gravity;
- deterministic fixed-tick simulation foundation;
- native physics/destruction foundations;
- asset database/package/VFS/streaming/world-partition foundations;
- editor runtime models that can later move from GDI shell to Vulkan/ImGui docking.

## Already established foundations

### Core and project infrastructure

- v0.1 bootstrap project layout: `engine/`, `editor/`, `runtime/`, `tools/`, `assets/`, `projects/`, `third_party/`.
- Core/platform/ECS/scene/assets/render/editor/player/asset compiler targets.
- CMake presets for Visual Studio 2022 and Ninja/MSVC debug builds.
- Engine version is currently `10.8.0` in `CMakeLists.txt`.

### Editor shell and workflow foundations

- Editor shell with hierarchy, inspector, viewport, asset browser, console.
- GDI backbuffer anti-flicker path.
- Viewport selection, grid, drag, zoom, pan, focus, duplicate, rename, save/load.
- Undo/redo history.
- Layout persistence, autosave, backup, session recovery.
- Preferences, theme/skin, Unicode/DPI/text support.
- Docking/runtime panel model, virtualized lists, command palette, popups/context workflows.
- Menu/toolbar runtime, command state, tooltips/modal command state.
- Activity notification / task center foundation.
- Transform gizmo runtime with Translate/Rotate/Scale modes and World/Local transform space.
- Scene View 2D/3D runtime foundation with orthographic/perspective camera contract and GDI preview bridge.
- Scene View camera navigation, picking hardening, overlay surface, and orientation widget runtime foundation.
- Inspector component editing expansion for Transform, World Position, Camera, Light, Mesh, Bounds, and Destructible runtime property descriptors.

### Math, geometry, world, time

- Math core: vectors, quaternions, matrices, rays, planes, spheres, AABB, frustum.
- Chernoff experimental math layer.
- Robust transform foundation: authoritative TRS, sanitized Euler transform, dirty flags, revisions.
- Bounds/visibility foundation: local/world AABB, scene bounds, frustum visibility.
- Large World Coordinates: world cell + local double position.
- Time/tick foundation: fixed tick, double real time, `uint64` frame/tick counters.
- Engine invariants: generational entity IDs, GUIDs, handles, results, normalized paths, atomic save.

### Assets, storage, streaming

- Asset database with source path -> `AssetGuid` -> cooked target.
- AKPAK package foundation.
- Virtual File System over loose files and packages.
- Budgeted streaming foundation.
- World Partition foundation.
- Project settings for units, coordinate conventions, runtime policy.
- Serialization/schema foundation for versioned formats.

### Topology, terrain, gravity, surfaces

- World topology policies: planar terrain, spherical planet, toroidal wrap.
- Gravity fields: uniform, planet, point, spherical zone, zero gravity.
- Surface movement policies: planar, planet, toroidal, free space.
- Terrain modes: heightfield, planet cube-sphere, voxel, mesh, procedural, toroidal.
- Spline foundation for roads, rivers, rails, cables, camera rails, patrol paths, terrain deformation corridors.
- Surface material foundation.

### Destruction, CSG, physics-related systems

- CSG boolean/destruction foundation with primitive SDF boolean and mesh voxelization path.
- Destruction bridge: destructible components, materials, fracture settings, CSG request/result, collision proxy sets, events.
- Remesh foundation for voxel surface extraction from CSG grids.
- Damage/health/breakage foundation.
- Explosion/shockwave/blast foundation.
- Debris/fracture runtime foundation.
- Debug draw runtime visualization.
- Physics foundations: collision proxies, broadphase, contacts, fixed-step shell, materials/layers/events, solver/CCD/sleep/constraints, telemetry/determinism/replay, anti-tunneling/leakage guards, convex manifold accuracy.
- Projectile ballistics/voxel damage and native optimization/fidelity layers.
- Character controller, vehicle, ragdoll bridge, softbody/cloth XPBD, fluid/buoyancy, force fields.

### Render and Vulkan foundations

- Render foundation: reversed-Z, camera-relative render position, RenderGraph plan/pass/resource descriptors.
- RHI/Vulkan runtime architecture and surface/device/swapchain clear frame.
- Vulkan frame graph compiler/synchronization and execution/command recording.
- Vulkan shader toolchain/draw submission.
- Vulkan GPU upload/draw-indexed foundation.
- Vulkan graphics pipeline primitive mesh.
- Render mesh optimization / meshing-main adaptation.
- Neural Texture Compression policy/foundation for future NVIDIA RTXNTC-style integration.
- Point-cloud import/cook, runtime streaming, out-of-core foundations.

## Near-term patch queue

Pick the highest-value item that is still missing or under-tested in the current working tree.

### v10.6 completed — Scene View camera navigation and picking hardening

Goal: make the new 2D/3D Scene View foundation feel like a real editor control layer before deeper renderer work.

Possible scope:

- precise Scene View camera controller state for pan/orbit/fly/focus/reset;
- deterministic screen-to-ray and ray-to-plane contracts for both 2D and 3D modes;
- command entries for Focus Selected, Frame All, Reset View, View Top/Front/Right;
- probe coverage for projection, focus distance, pitch clamps, near/far, zoom bounds, and selection ray stability;
- no Vulkan dependency required.

### v10.7 completed — Scene View overlay and axis/orientation widget foundation

Goal: make 2D/3D mode and transform space visible and controllable in the viewport.

Implemented scope:

- orientation gizmo model: X/Y/Z axes, top/front/right camera snaps;
- overlay model for grid mode, camera mode, snap state, selected entity, active tool;
- hit-test contracts for overlay buttons independent of GDI/Vulkan backend;
- command integration and probe.

### v10.8 completed — Inspector component editing expansion

Goal: move Inspector closer to useful component editing.

Implemented scope:

- typed property model for Transform, WorldPosition, Camera, Light, Mesh, Bounds, and Destructible where already present;
- min/max/step metadata for numeric fields;
- property edit validation coverage for camera range clamping;
- proxy rebuild request coverage for mesh material changes;
- strengthened `ak_editorpropertyprobe` and panel diagnostics without adding new targets.

### v10.9 candidate — Asset Browser thumbnails and import status model

Goal: make Asset Browser move beyond a simple file list.

Possible scope:

- asset item view model with icon kind, cook status, GUID, source path, cooked path;
- filters/search/sort state;
- thumbnail placeholder descriptors for texture/mesh/material/scene;
- drag source contract for scene drops;
- no real GPU thumbnail rendering yet.

### v11.0 candidate — Editor/runtime Vulkan viewport bridge hardening

Goal: prepare replacement of GDI Scene View preview with Vulkan-backed viewport without destroying editor runtime models.

Possible scope:

- viewport render target lifecycle contract;
- editor frame descriptor -> RHI/render bridge;
- resize, DPI, input capture, focus, frame-in-flight safety;
- clear/pass debug rendering through RenderGraph where possible;
- keep GDI fallback if Windows path is not ready.

## Mid-term systems

### Editor workflows

- Full Add Component search and component creation pipeline.
- Drag/drop from Asset Browser into Scene View and Inspector fields.
- Prefab/scene reference graph and broken-reference repair UX.
- Multi-select transform and inspector editing.
- Scene hierarchy search, filters, parent/child relationship graph.
- Gizmo visual polish after Vulkan/ImGui bridge: proper depth behavior, handles, axis rings, screen-space scale.

### ECS and scene model

- Relationship graph in ECS: parent/child, prefab/source links, dependencies.
- Stable scene reference model using persistent GUIDs.
- Archetype/chunk ECS or typed sparse-set upgrade path after current API is stable.
- Component events, dirty propagation, deterministic iteration order.
- Multi-scene/world support for editor + play mode.

### Assets and packages

- Deterministic asset import pipeline with importer versioning.
- Material and shader asset formats.
- Asset dependencies, reverse dependency graph, recook invalidation.
- Streaming entities/prefabs, not just raw assets.
- Package compression/encryption/signing policy later.

### Renderer/RHI

- RenderGraph execution for real passes, transient resources, barriers, and debug visualization.
- Shader cache/permutation registry and fallback shader policy.
- Material system and PBR foundation.
- Mesh/material draw submission, instancing, culling, sort keys.
- Shadow/depth prepass, G-buffer or forward+/clustered path decision.
- Virtual texturing / sparse virtual textures.
- Nanite-like virtual geometry research path: clusters, hierarchy, culling, streaming, fallback mesh LOD.

### Physics and simulation

- Production rigid body components and collider descriptors.
- Broadphase and narrowphase hardening for stack jitter, pressure/squeeze tunneling, rotational CCD, thin triangle leakage.
- Persistent manifolds, warm starting, split impulse, friction cone, restitution threshold.
- Jobified deterministic physics pipeline across broadphase/narrowphase/island solving/event merge.
- Character controller polish: multi-sweep, slope hysteresis, step validation, ground normal filtering.
- Vehicle tire curves, suspension contact cache, anti-roll, substepping.
- XPBD cloth/softbody self-collision and jobified solver batches.

### Worlds and streaming

- Hierarchical LOD / HLOD.
- Occlusion culling for world-scale scenes.
- Deterministic save system for huge worlds.
- Runtime procedural generation pipeline with saved deltas.
- World Partition editor visualization and cell debugging.
- Planet terrain clipmaps/cube-sphere streaming.

### Tooling and quality

- CI profile once repository moves to GitHub.
- Sanitizer/static analysis profile where toolchain permits.
- Crash diagnostics and minidumps.
- Determinism hash baselines for probes.
- Performance telemetry baselines for hot probes.

## Patch selection rules

When the task says “next patch”:

1. Prefer an item from the near-term queue unless a broken build/test requires a repair patch first.
2. If a module was just added, harden its tests, commands, state integration, and documentation before jumping to a far subsystem.
3. Do not add a large renderer/physics feature if the editor/runtime state model needed by that feature is still missing.
4. Avoid mixing unrelated systems. One patch can touch multiple modules only when they are part of one vertical feature.
5. The patch should leave the tree in a state where the operator can inspect `git diff`, build, run probes, and continue safely.
