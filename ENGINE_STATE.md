# AK Engine State

Last updated by Codex patch: post-v10.8 architecture/state sync after latest five one-file editorui interaction/scroll hardening commits.

## Current repository baseline

- Root project: `AKEngine`
- Primary local path used by the operator: `D:\AKENGINE`
- Build system: CMake
- Primary compiler/generator: Visual Studio 17 2022 / MSVC
- Alternate local generator: Ninja with MSVC environment
- Language target: C++20/23 style, current CMake requires `cxx_std_20`
- Current project version in `CMakeLists.txt`: `10.8.0`
- Latest functional patch present in this archive: post-v10.8 one-file editorui interaction and scroll clipping hardening patches after `v10.8 - Inspector Component Editing Expansion`

## Primary build commands

```powershell
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug
```

Optional Ninja/MSVC path:

```powershell
cmake --preset windows-ninja-debug
cmake --build --preset windows-ninja-debug
```

Expected Visual Studio debug binaries:

```text
build/windows-vs-debug/bin/Debug/ak_editor.exe
build/windows-vs-debug/bin/Debug/ak_player.exe
build/windows-vs-debug/bin/Debug/ak_assetc.exe
```

## Important architectural decisions

- Windows-first, Vulkan-first later, C++/CMake/MSVC stack.
- Editor UX may be inspired by Unity, but implementation must stay custom and architecture-specific.
- World units: meters, seconds, kilograms.
- Angles: radians internally; degrees at UI/tool boundaries.
- World space: right-handed, Y-up.
- Large World Coordinates: world cell `int64` + local `double` position.
- Renderer precision policy: camera-relative rendering and reversed-Z.
- Simulation policy: fixed tick, `uint64` counters, double real time; do not use absolute float time.
- Entity/resource lifetime: generational handles; deferred release for resources.
- Persistent asset references: stable `AssetGuid`, not raw source paths.
- File formats: magic/version/schema/migration policy.
- Saves/writes: atomic write policy where possible.
- Future physics: deterministic CPU gameplay physics first; GPU acceleration later for massive particles/fluids/cloth/voxel/debris.

## Current high-level module map

```text
engine/core            — version, logging, result-style primitives, base contracts
engine/platform        — platform/window bridge
engine/math            — vector/quaternion/matrix/geometry math
engine/ecs             — entity/component foundation
engine/scene           — scene components and scene serialization path
engine/assets          — asset database contracts
engine/package         — AKPAK package foundation
engine/vfs             — loose/package virtual file access
engine/streaming       — budgeted asset streaming
engine/world           — Large World Coordinates
engine/worldtopology   — planar/planet/toroidal topology policies
engine/worldpartition  — streaming cell foundation
engine/settings        — units/coordinate/runtime policies
engine/serialization   — versioned schema/key-value document foundation
engine/resources       — generation handles and resource manager
engine/render          — render policy and RenderGraph descriptors
engine/rhi             — Vulkan/RHI runtime foundations
engine/editorui        — editor runtime models, commands, panels, Scene View, gizmos
engine/physics         — native physics foundations
engine/destruction     — CSG destruction bridge into physics/bounds/resources/world partition
engine/csg             — primitive/mesh CSG boolean foundation
engine/remesh          — voxel surface extraction foundation
engine/debugdraw       — debug visualization runtime
engine/projectile      — projectile/ballistics/voxel damage path
engine/vehicle         — vehicle physics foundation
engine/character       — character controller foundation
engine/ragdoll         — animation/physics bridge foundation
engine/softbody        — cloth/softbody XPBD foundation
engine/fluid           — fluid/buoyancy foundation
engine/forcefield      — force-field/area effects
engine/pointcloud      — point cloud import/runtime/out-of-core foundations
engine/ntc             — neural texture compression policy foundation
```

## Latest editor state

The editor currently has a GDI shell/preview bridge, not a final Vulkan/ImGui viewport. This is expected.

Recently completed editor runtime layers:

- v10.1 menu/toolbar runtime.
- v10.2 transform gizmo runtime foundation.
- v10.3 translate/rotate/scale transform tools.
- v10.4 world/local transform space.
- v10.5 Scene View 2D/3D runtime foundation.
- v10.6 Scene View camera navigation and picking hardening.
- v10.7 Scene View overlay and orientation widget runtime foundation.
- v10.8 Inspector component editing expansion.
- Post-v10.8 one-file hardening: Inspector property editing validation/live-preview/cancel correctness, command-palette property edit rejection handling, Asset Browser selection/inspector synchronization, and editor scroll clipping/range/thumb/wheel overflow robustness.

Current Scene View behavior direction:

- 2D mode: orthographic/top-down style editing path.
- 3D mode: perspective camera contract, focus-preserving orbit/pan/dolly, axis snaps, and GDI perspective-grid preview bridge.
- Scene View overlay state now exposes backend-neutral controls for 2D/3D mode, snap, transform tool/space, focus/frame/reset, and X/Y/Z orientation-axis hit-tests.
- Transform tools and spaces are represented in runtime/editorui state, not only hardcoded in the window code.

Current Inspector runtime direction:

- Inspector property descriptors now cover Transform, World Position, Camera, Light, Mesh, Bounds, and Destructible component groups in the backend-neutral panel model.
- Numeric properties can expose min/max ranges and step metadata for deterministic editor widgets.
- Property editing validation covers finite numeric values, camera FOV range clamping, undo-per-commit behavior, cancel restore, and proxy rebuild requests.
- Recent one-file fixes tightened integer validation order, reversed min/max range handling, integer supported-range rejection, and live-preview revision increments only when a property value actually changes.
- Recent cancel-edit hardening now restores inspector/frame revisions only when the original value actually changes the property.
- Recent Scene View camera sanitization now keeps the near plane below the maximum far-plane relationship and preserves equality at the minimum far/near separation.
- Recent Scene View navigation hardening now suppresses no-op focus/2D zoom change reporting, ignores 2D zoom input when the viewport is unusable, and keeps orthographic ray origins inside the camera coordinate policy.
- Recent Scene View focus/frame hardening now rejects non-finite and out-of-camera-range bounds/focus coordinates instead of accepting every finite value.
- Recent Scene View overlay hardening now uses saturating coordinate offsets for toolbar/orientation controls near extreme viewport coordinates.
- Recent Asset Browser interaction hardening now normalizes `asset:` stable IDs before lookup and rejects empty asset IDs/empty item GUIDs.
- Recent Asset Browser selection hardening now refreshes Inspector selection state when an asset is selected.
- Recent command-palette property edit hardening now reports rejected setting/property edit results instead of marking the model dirty for missing or read-only properties.
- Recent property commit hardening now avoids dirty flags, proxy rebuild requests, panel revisions, and search rebuilds when a committed value is unchanged.
- Recent editor scroll hardening now increments scroll revision when metrics change even if the clamped offset stays the same, normalizes negative content/viewport metrics before clamping/thumb layout, propagates invalid clip-stack state, and saturates clip rectangle ends, virtual list/grid content pixels, range counts, item visibility math, ceil division, wheel deltas, and scrollbar thumb coordinates.

## Current generated/cache state

This archive may contain `.akcache/` and `build/` directories. Treat them as local generated state, not source of truth. Codex should not edit these directories unless a task explicitly targets generated artifacts.

## Latest patch summary

Patch: `post-v10.8 - architecture/state sync after latest five one-file editorui interaction/scroll hardening commits`.

Changed files:

```text
ENGINE_STATE.md
```

Intent:

```text
Synchronize the repository state file with the latest five one-file editorui interaction/scroll commits without changing production code, CMake, docs, probes, tests, generated output, or zip artifacts.
Record the changed subsystems, risks, intentionally skipped docs/probes, and next small patch candidates.
```

Public API / format changes:

```text
None in this architecture/state sync patch.
The reviewed commits only touched editor runtime implementation files, with no observed serialized format, scene format, command ID, or asset contract changes in the inspected stats/diffs.
```

Verification result:

```text
AGENTS.md -> read
ENGINE_STATE.md -> read
QUALITY_GATE.md -> read
ENGINE_ROADMAP.md, tasks/NEXT_PATCH.md, nearby docs, and CMake context reads -> intentionally skipped by operator constraint: architecture sync only, use git history first, review only last five commits, no docs/CMake work
git status --short -> clean before patch
git log --oneline -5 -> reviewed 520053a, 9926324, cff6c8e, a0e1aa1, ca8296b
git show --stat --oneline -5 -> reviewed; all five commits were one-file editorui implementation patches
git show -5 --format=fuller --name-status -> reviewed; commit subjects were generic `Patch:`
git show --stat --patch --unified=40 520053a -- engine/editorui/src/EditorInteraction.cpp -> reviewed
git show --stat --patch --unified=40 9926324 -- engine/editorui/src/EditorScrollClip.cpp -> reviewed
git show --stat --patch --unified=40 cff6c8e -- engine/editorui/src/EditorScrollClip.cpp -> reviewed
git show --stat --patch --unified=60 a0e1aa1 -- engine/editorui/src/EditorInteraction.cpp -> reviewed
git show --stat --patch --unified=60 ca8296b -- engine/editorui/src/EditorInteraction.cpp -> reviewed
git diff --stat -> ENGINE_STATE.md only, 1 file changed
git diff --check -> passed; warning only that Git may rewrite ENGINE_STATE.md LF to CRLF on next touch
cmake configure/build -> intentionally skipped by operator constraint: architecture sync only, edit only ENGINE_STATE.md, no code
probes/tests -> intentionally skipped by operator constraint: no probes, no tests
docs update -> intentionally skipped by operator constraint: do not edit docs/
zip readability test -> intentionally skipped by operator constraint: do not create zip archives
```

Next recommended step:

```text
Prefer one more narrow editorui verification or behavior patch before a broader feature if verification finds gaps.
Best one-file candidates:
1. EditorInteraction.cpp: verify whether unchanged property commits should still emit a pending change/modelDirty result or whether no-op commits should become fully quiet.
2. EditorInteraction.cpp: harden command-palette setting search results around stale property paths and read-only properties, keeping rejected edits visible but non-dirty.
3. EditorInteraction.cpp: extend Asset Browser selection behavior toward asset-aware Inspector details if the current Inspector selection bridge still only reflects scene component state.
4. EditorScrollClip.cpp: add focused regression coverage only if the operator lifts the no-tests/no-probes limit for saturated wheel deltas and scrollbar thumb coordinates near extreme track positions.
5. Otherwise continue with v10.9 - Asset Browser thumbnails and import status model, but only as a normal feature patch with docs/probes/CMake policy restored.
```

## Post-v10.8 architecture sync notes

Reviewed commits:

```text
520053a Patch: Command-palette property edit rejection handling
9926324 Patch: Editor scrollbar thumb coordinate saturation
cff6c8e Patch: Editor scroll wheel delta saturation
a0e1aa1 Patch: Property commit no-op dirty/rebuild suppression
ca8296b Patch: Asset Browser selection Inspector synchronization
```

Changed subsystems:

```text
engine/editorui Editor interaction property editing from command-palette setting results, property commit mutation flags, and Asset Browser selection propagation into Inspector selection state
engine/editorui Editor scroll clipping scrollbar thumb positioning and mouse-wheel delta overflow guards
```

Architectural risks noticed:

```text
The last five patches were implementation-only and one-file, so docs/probe coverage is intentionally skipped in this sync until the operator lifts the no-docs/no-probes/no-tests constraint.
Command-palette property edits now only mark the model dirty when the edit actually begins; stale or read-only property search results are visible rejections rather than dirtying UI state.
No-op property commits now suppress serialized dirty flags, proxy rebuild requests, panel revision increments, and search rebuilds; consumers that used pending property-change records as mutation evidence should inspect the change flags rather than assuming every accepted commit mutated state.
Asset selection now updates Inspector selection state; this closes a stale-selection gap but should be checked against any future asset-detail Inspector model so asset and scene-entity selection contracts do not diverge.
Scroll wheel deltas and scrollbar thumb coordinates now saturate to i32 bounds; this prevents overflow but means extreme inputs collapse to bounded positions rather than preserving mathematical magnitude.
Docs, probes, tests, production code, CMake, and zip artifacts are intentionally skipped by operator instruction for this architecture sync.
```

## Checks that were expected around v10.8

The latest patch reported these checks in the available environment:

```text
ak_editorpropertyprobe
ak_editorpanelprobe
ak_commandprobe
ak_editormenutoolbarprobe
ak_editorcommandstateprobe
ak_editor target build
zip readability test
```

Windows/MSVC verification should still be run on the operator machine after local changes:

```powershell
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug

.\build\windows-vs-debug\bin\Debug\ak_editorsceneviewprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editorpropertyprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editorpanelprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editortransformgizmoprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editormenutoolbarprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editorcommandstateprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_commandprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editor.exe
```

## Known documentation mismatch

`README.md` still describes an older v5.0/vehicle-stage baseline. Do not rely on it as the current state authority until it is updated. Use this file, `ENGINE_ROADMAP.md`, the CMake project version, and the module docs under `docs/`.

## Recommended next patch

The strongest next step is likely one of:

1. `v10.9 - Asset Browser thumbnails and import status model`.
2. `v11.0 - Editor/runtime Vulkan viewport bridge hardening`.
3. `v11.1 - Inspector scene bridge for ECS-backed property application`.

Prefer v10.9 unless the build is broken or inspection reveals a higher-priority regression.

## State update protocol

Every Codex patch should append or update:

- patch title/version;
- changed files;
- public API/format changes;
- checks run and their exact result;
- checks skipped and the exact reason;
- next recommended step.

Do not remove historical decisions unless they were superseded by a committed migration.
