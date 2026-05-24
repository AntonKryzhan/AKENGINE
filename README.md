# AK Engine

**AK Engine** is an experimental Windows-first C++ game engine focused on large worlds, editor tooling, deterministic runtime foundations, modern rendering architecture, physics/destruction research, and scalable engine infrastructure.

The project is not a Unity/Unreal/Godot clone.  
It is built as a low-level custom engine that borrows proven production ideas from modern engines while keeping the architecture explicit, modular, inspectable, and optimized for long-term control.

AK Engine is currently in active development.

---

## Vision

AK Engine is designed around several long-term goals:

- large-world support without floating-point jitter;
- planet-scale worlds;
- flexible world topology: planar terrain, spherical planets, toroidal/wrap worlds;
- editor-first workflow similar in spirit to Unity/Godot, but with custom architecture;
- Vulkan-first rendering direction;
- robust C++20/23 engine core;
- deterministic simulation foundations;
- custom ECS, asset database, virtual file system, streaming, and world partitioning;
- physics, CSG, destruction, terrain, splines, and advanced material/texture systems;
- future integration paths for modern techniques such as RenderGraph, neural texture compression, virtualized geometry, streaming worlds, and high-quality editor tooling.

The engine is intentionally being built from the foundations upward instead of hiding core decisions behind opaque middleware.

---

## Current Status

AK Engine is currently a **bootstrap-to-runtime foundation engine** with an expanding editor shell and many core systems already implemented as standalone modules.

The project already contains working foundations for:

- editor shell;
- scene hierarchy;
- inspector;
- asset browser;
- command registry;
- menu/toolbar system;
- editor command states;
- Scene View 2D/3D foundation;
- Scene View navigation and camera controls;
- transform gizmo foundation;
- editor UX/runtime probes;
- ECS components;
- scene serialization;
- asset database;
- package format;
- virtual file system;
- streaming;
- world partition;
- large world coordinates;
- terrain modes;
- spherical planet topology;
- toroidal wrap worlds;
- gravity fields;
- spline systems;
- CSG/destruction foundations;
- physics foundations;
- diagnostics/profiling foundations;
- memory allocators;
- job system;
- RHI/Vulkan preparation;
- RenderGraph/Vulkan frame graph foundations.

This is not yet a production-ready engine, but it is already far beyond a minimal toy renderer.  
The current focus is building stable engine contracts before pushing toward full Vulkan editor rendering and production-grade asset workflows.

---

## Core Architecture

AK Engine is split into small modules with clear responsibility boundaries.

Major subsystems include:

### Engine Core

- `ak_core`;
- result/error handling foundations;
- invariant checks;
- engine versioning;
- low-level utilities;
- platform-independent contracts.

### Math and Geometry

- vectors, quaternions, matrices;
- rays, planes, spheres, AABBs, frustums;
- robust transform sanitation;
- bounds and visibility foundations;
- future-ready geometry infrastructure for collision, picking, culling, and rendering.

### ECS and Scene

- entity/component foundations;
- typed components;
- transform, mesh, camera, light, bounds, and world-position components;
- scene serialization with versioned formats;
- editor/runtime scene operations.

### Large World Coordinates

AK Engine uses a large-world model based on:

- integer world cells;
- double-precision local positions;
- camera-relative rendering path;
- world origin policies;
- future physics island origins.

This is designed to avoid common large-world problems such as:

- float jitter;
- unstable far-from-origin transforms;
- precision loss in physics and rendering;
- bad camera precision;
- unstable world streaming.

### World Topology

AK Engine is not limited to a flat Unity-style world.

Supported topology foundations include:

- planar terrain;
- spherical planets;
- toroidal wrap worlds;
- planet geodetic coordinates;
- surface frames;
- spherical gravity;
- movement on curved surfaces.

This allows the engine to eventually support:

- normal game terrains;
- Earth-scale worlds;
- planets and moons;
- asteroid-style gravity;
- Pac-Man-style infinite wrap maps.

### Asset System

AK Engine contains foundations for:

- asset GUIDs;
- asset manifests;
- source-to-cooked asset mapping;
- package format;
- virtual file system;
- loose files and packaged assets;
- budgeted streaming;
- resource handles with generation counters.

The long-term goal is to avoid fragile path-based asset references and move toward stable GUID-based project data.

### Editor

The editor is currently a native Windows editor shell with an expanding Scene View and UI framework.

Implemented editor foundations include:

- hierarchy panel;
- inspector panel;
- asset browser;
- console/status overlays;
- menu and toolbar runtime;
- command palette foundations;
- command registry;
- shortcut/input action integration;
- undo/redo foundations;
- Scene View 2D/3D mode;
- transform gizmo foundation;
- camera navigation and picking foundations;
- editor diagnostics panels;
- editor session/preferences/workspace foundations.

The current editor is still not the final Vulkan/ImGui docking editor.  
It is an evolving bootstrap editor used to validate runtime architecture and interaction contracts.

### Rendering Direction

AK Engine is Vulkan-first by design.

Current rendering-related foundations include:

- renderer module;
- render probes;
- RHI foundations;
- Vulkan architecture probes;
- Vulkan upload/draw/framegraph/mesh foundations;
- reversed-Z depth policy;
- camera-relative render positioning;
- RenderGraph planning structures;
- future pipeline cache/shader system direction.

The long-term rendering roadmap includes:

- Vulkan surface/device/swapchain;
- real-time editor viewport;
- RenderGraph resource lifetime management;
- GPU synchronization discipline;
- shader pipeline with HLSL/Slang/SPIR-V direction;
- PBR material model;
- virtual texturing;
- neural texture compression integration path;
- virtualized geometry / Nanite-like research path;
- advanced culling and world-scale rendering.

### Physics and Destruction

AK Engine contains foundations for:

- physics scene contracts;
- rigid-body direction;
- collision descriptors;
- gravity-field integration;
- projectile/debris/damage/explosion systems;
- CSG boolean/destruction foundations;
- remesh foundations;
- softbody, ragdoll, vehicle, and fluid module foundations.

The physics system is currently CPU-first and designed to move toward:

- deterministic fixed-tick simulation;
- jobified broadphase/narrowphase;
- physics islands;
- local-origin simulation for large worlds;
- stable collision/event handling;
- destruction-to-physics bridge;
- gameplay-oriented deterministic physics before GPU acceleration.

### Diagnostics, Jobs, and Memory

The engine includes early infrastructure for:

- job system;
- worker pool;
- parallel-for;
- memory tracking;
- linear allocators;
- diagnostics hub;
- profiling scopes;
- counters;
- runtime probes.

The goal is to make performance and correctness visible early instead of adding diagnostics after the engine becomes too large.

---

## Feature Progress

| Area | Status |
|---|---|
| C++20/23 CMake project | Implemented |
| Windows-first bootstrap | Implemented |
| Editor shell | Implemented |
| Scene hierarchy | Implemented |
| Inspector foundation | Implemented |
| Asset browser foundation | Implemented |
| Command registry | Implemented |
| Menu/toolbar runtime | Implemented |
| Editor command state | Implemented |
| Scene View 2D/3D mode | Implemented |
| Scene View camera navigation | Implemented foundation |
| Transform gizmo | Implemented foundation |
| ECS typed components | Implemented foundation |
| Scene serialization | Implemented foundation |
| Large World Coordinates | Implemented foundation |
| Bounds/visibility | Implemented foundation |
| Asset database | Implemented foundation |
| Package format | Implemented foundation |
| Virtual file system | Implemented foundation |
| Streaming | Implemented foundation |
| World partition | Implemented foundation |
| Project settings | Implemented foundation |
| World topology | Implemented foundation |
| Planet/spherical surface movement | Implemented foundation |
| Gravity fields | Implemented foundation |
| Terrain modes | Implemented foundation |
| Splines | Implemented foundation |
| CSG/destruction | Implemented foundation |
| Physics modules | Implemented foundations |
| Job system | Implemented foundation |
| Memory system | Implemented foundation |
| Diagnostics/profiling | Implemented foundation |
| RHI/Vulkan preparation | Implemented foundation |
| Vulkan real viewport | Planned |
| ImGui/Vulkan editor | Planned |
| Material/shader graph | Planned |
| Full asset cooking pipeline | In progress / planned |
| Production renderer | Planned |
| Production physics solver | Planned |
| Production animation system | Planned |
| Scripting | Planned |
| Full game runtime/player pipeline | In progress / planned |

---

## Comparison With Other Engines

AK Engine is not trying to replace Unity, Unreal Engine, or Godot today.  
Those engines are mature production ecosystems with years of tooling, rendering, importers, exporters, platforms, plugins, and community support.

AK Engine is different because it is being built as a custom, research-heavy, low-level engine with direct control over the architecture.

| Engine | Strengths | AK Engine Difference |
|---|---|---|
| Unity | Mature editor, huge ecosystem, fast iteration, C# workflow | AK Engine focuses on custom C++ architecture, large-world foundations, explicit engine internals, and low-level rendering/physics control |
| Unreal Engine | AAA renderer, Nanite/Lumen, advanced editor, production tools | AK Engine is smaller and experimental, but aims for transparent systems, custom architecture, and research-driven large-world/runtime foundations |
| Godot | Open-source, lightweight, flexible, fast to modify | AK Engine is lower-level, Windows/Vulkan-first, C++-centric, and focused on custom engine research rather than general-purpose accessibility |
| Custom in-house engines | Maximum control, project-specific optimization | AK Engine follows this philosophy: controlled architecture, no black-box dependence, explicit systems, engine code built around the project’s long-term goals |

AK Engine’s advantage is not maturity.  
Its advantage is **control**.

The project is designed so that every major system can be inspected, modified, specialized, optimized, and eventually pushed toward very specific game requirements.

---

## Design Principles

AK Engine follows several strict principles:

- no blind copying of existing engines;
- no hidden architecture magic;
- explicit ownership and lifetime management;
- generational handles instead of unsafe stale references;
- GUID-based assets instead of fragile path references;
- fixed simulation tick separated from render frames;
- large-world coordinates from the beginning;
- camera-relative rendering;
- reversed-Z rendering policy;
- versioned file formats;
- atomic saves;
- stable project settings;
- diagnostics and profiling from early development;
- avoid unnecessary refactoring during focused patches;
- every new system should have a clear runtime/editor contract.

---

## Development Philosophy

AK Engine is developed in controlled patches.

Each patch should:

- change only what is necessary;
- preserve existing APIs unless the task explicitly requires a change;
- avoid unrelated refactoring;
- keep compatibility with existing project formats;
- pass the quality gate;
- keep the project buildable;
- avoid committing generated folders, build outputs, logs, cache files, or patch archives.

The current development workflow supports both:

- manual high-quality patching through browser-based review;
- local Codex-assisted micro-patches;
- Git checkpointing;
- CMake/Visual Studio quality gates.

---

## Repository Layout

Typical structure:

```text
AKENGINE/
  assets/          Project assets and samples
  docs/            Engine documentation
  editor/          Editor executable and editor-side integration
  engine/          Engine modules
  projects/        Example projects
  runtime/         Runtime/player code
  scripts/         Automation and quality-gate scripts
  tasks/           Codex/local automation task definitions
  tools/           Engine tools and probes
  CMakeLists.txt   Root CMake project
  CMakePresets.json
  AGENTS.md
  ENGINE_ROADMAP.md
  ENGINE_STATE.md
  QUALITY_GATE.md
```

Local/generated directories are intentionally ignored:

```text
build/
logs/
.akcache/
PATCHES/
screen/
*.zip
```

---

## Windows Setup Guide

AK Engine is currently Windows-first.

Recommended location:

```text
D:\AKENGINE
```

### Required Tools

Install:

1. **Git**
2. **CMake 3.25+**
3. **Visual Studio 2022 Community** or **Visual Studio 2022 Build Tools**
4. Visual Studio workload:
   - `Desktop development with C++`
5. Windows SDK
6. Optional later:
   - Vulkan SDK
   - Python
   - Node.js / Codex CLI for local automation

---

## Clone the Repository

```powershell
Set-Location D:\
git clone https://github.com/AntonKryzhan/AKENGINE.git
Set-Location D:\AKENGINE
```

If you already have the repository:

```powershell
Set-Location D:\AKENGINE
git pull
```

---

## Configure the Project

```powershell
Set-Location D:\AKENGINE
cmake --preset windows-vs-debug
```

---

## Build the Project

```powershell
cmake --build --preset windows-vs-debug
```

Expected output directory:

```text
D:\AKENGINE\build\windows-vs-debug\bin\Debug\
```

---

## Run the Editor

```powershell
.\build\windows-vs-debug\bin\Debug\ak_editor.exe
```

---

## Run Important Validation Tools

```powershell
.\build\windows-vs-debug\bin\Debug\ak_editorsceneviewprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editortransformgizmoprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_commandprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editormenutoolbarprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editorcommandstateprobe.exe
```

Or run the project quality gate:

```powershell
.\scripts\quality-gate.ps1
```

---

## Recommended Git Workflow

Before making changes:

```powershell
Set-Location D:\AKENGINE
git status
```

Create a branch:

```powershell
git checkout -b feature/my-change
```

After changes:

```powershell
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug
.\scripts\quality-gate.ps1
```

Commit:

```powershell
git add .
git commit -m "Describe the change"
```

Push:

```powershell
git push
```

---

## Local Codex Automation

The repository contains optional local automation files:

```text
AGENTS.md
ENGINE_ROADMAP.md
ENGINE_STATE.md
QUALITY_GATE.md
tasks/
scripts/
```

Recommended low-cost workflow:

```powershell
.\scripts\auto-codex-loop.ps1 -MaxPatches 1 -TaskFile "tasks/ONE_FILE_PATCH.md" -TimeoutMinutes 20
```

For a batch of tiny patches followed by architecture sync:

```powershell
.\scripts\codex-5tiny-1sync.ps1
```

Automation rules:

- keep patches small;
- do not create zip archives;
- do not create new probes unless explicitly required;
- do not touch generated folders;
- validate with existing quality gates;
- commit only passing changes.

---

## Roadmap

Near-term direction:

- continue improving Scene View and editor UX;
- strengthen inspector component editing;
- improve transform tools and selection workflow;
- stabilize editor command/state integration;
- continue Vulkan/RHI foundation;
- move toward real Vulkan editor viewport;
- improve asset pipeline and material handling;
- connect editor workflows with runtime scene data;
- improve physics/destruction bridges;
- expand terrain/world partition workflows;
- keep large-world, planet, and streaming requirements central.

Long-term direction:

- Vulkan renderer;
- ImGui/Vulkan editor UI;
- RenderGraph-driven frame pipeline;
- shader pipeline;
- material graph;
- virtual texturing;
- neural texture compression integration path;
- virtualized geometry research;
- deterministic physics foundation;
- large-world streaming;
- planet-scale runtime;
- production-grade asset pipeline;
- scripting/runtime gameplay layer.

---

## Project Status Disclaimer

AK Engine is an active experimental engine project.

It is not production-ready yet.  
APIs, formats, editor behavior, and runtime systems may change as the architecture evolves.

The current goal is to build a strong foundation first, then gradually turn those foundations into production systems.
