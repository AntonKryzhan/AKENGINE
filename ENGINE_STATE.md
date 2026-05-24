# AK Engine State

Last updated by Codex patch: post-v10.8 architecture/state sync after latest five one-file Scene View camera-coordinate/navigation and panel-model diagnostics commits.

## Current repository baseline

- Root project: `AKEngine`
- Primary local path used by the operator: `D:\AKENGINE`
- Build system: CMake
- Primary compiler/generator: Visual Studio 17 2022 / MSVC
- Alternate local generator: Ninja with MSVC environment
- Language target: C++20/23 style, current CMake requires `cxx_std_20`
- Current project version in `CMakeLists.txt`: `10.8.0`
- Latest functional patch present in this archive: post-v10.8 one-file editorui Scene View camera-coordinate/navigation no-op hardening and panel-model validation diagnostics patches after `v10.8 - Inspector Component Editing Expansion`

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
- Post-v10.8 one-file hardening: Inspector property editing validation/live-preview/cancel correctness, command-palette property edit rejection, selection no-op/large-delta handling, hierarchy/asset selection Inspector revision propagation, Scene View camera sanitizer/mode/focus coordinate/navigation robustness, Asset Browser selection/inspector/search synchronization, panel-model validation diagnostics, and editor scroll clipping/range/thumb/wheel overflow robustness.

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
- Recent Scene View camera mode sanitization now restores invalid camera modes to 2D before enforcing the matching projection contract.
- Recent Scene View mode input hardening now clamps invalid requested mode inputs to 2D before switching projection/camera contracts.
- Recent Scene View focus placement hardening now clamps camera focus and derived camera position coordinates through the same camera-coordinate bounds used by sanitizer paths.
- Recent Scene View navigation hardening now suppresses no-op focus/2D zoom change reporting, ignores 2D zoom input when the viewport is unusable, and keeps orthographic ray origins inside the camera coordinate policy.
- Recent Scene View focus/frame hardening now rejects non-finite and out-of-camera-range bounds/focus coordinates instead of accepting every finite value.
- Recent Scene View ray/navigation hardening now clamps orthographic ray origins, 2D pan/zoom centers, 3D pan focus/position offsets, and dolly positions through the camera-coordinate policy.
- Recent Scene View navigation revision hardening now reports pan/orbit/dolly changes only when the navigation pose actually changes after clamping.
- Recent Scene View overlay hardening now uses saturating coordinate offsets for toolbar/orientation controls near extreme viewport coordinates.
- Recent Asset Browser interaction hardening now normalizes `asset:` stable IDs before lookup and rejects empty asset IDs/empty item GUIDs.
- Recent Asset Browser search hardening now skips assets with empty GUIDs when building panel search records.
- Recent Asset Browser selection hardening now refreshes Inspector selection state when an asset is selected.
- Recent panel-model validation hardening now fails diagnostics for empty or duplicate property paths and invalid Asset Browser selected indexes.
- Recent hierarchy and Asset Browser selection hardening now increments Inspector panel revision when selection state is refreshed for selected scene entities or assets, while suppressing hierarchy/asset model revisions when the selected flags/index do not change.
- Recent command-palette property edit hardening now reports rejected setting/property edit results instead of marking the model dirty for missing or read-only properties.
- Recent command-palette selection hardening now suppresses palette/model revisions when moving selection by a zero-result delta or large wrapped delta keeps the same selected result.
- Recent property commit hardening now avoids dirty flags, proxy rebuild requests, panel revisions, and search rebuilds when a committed value is unchanged.
- Recent editor scroll hardening now increments scroll revision when metrics change even if the clamped offset stays the same, normalizes negative content/viewport metrics before clamping/thumb layout, propagates invalid clip-stack state, clamps tiny scrollbar tracks to a track-sized minimum thumb, and saturates clip rectangle ends/spans, virtual list/grid content pixels, range counts, item visibility math, ceil division, wheel deltas, and scrollbar thumb coordinates.

## Current generated/cache state

This archive may contain `.akcache/` and `build/` directories. Treat them as local generated state, not source of truth. Codex should not edit these directories unless a task explicitly targets generated artifacts.

## Latest patch summary

Patch: `post-v10.8 - architecture/state sync after latest five one-file Scene View camera-coordinate/navigation and panel-model diagnostics commits`.

Changed files:

```text
ENGINE_STATE.md
```

Intent:

```text
Synchronize the repository state file with the latest five one-file Scene View camera-coordinate/navigation no-op hardening and panel-model diagnostics commits without changing production code, CMake, docs, probes, tests, generated output, or zip artifacts.
Record the changed subsystems, risks, intentionally skipped docs/probes, and next small patch candidates.
```

Public API / format changes:

```text
None in this architecture/state sync patch.
The reviewed commits only touched editor runtime implementation files, with no observed serialized format, scene format, command ID, CMake target, or asset contract changes in the inspected stats/diffs.
```

Verification result:

```text
AGENTS.md -> read
ENGINE_STATE.md -> read
QUALITY_GATE.md -> read
ENGINE_ROADMAP.md -> intentionally skipped by operator constraint: architecture sync only, use git history first, review only last five commits
tasks/NEXT_PATCH.md -> intentionally skipped by operator constraint: architecture sync only, use git history first, review only last five commits
nearby docs and CMake context reads -> intentionally skipped by operator constraint: architecture sync only, use git history first, review only last five commits, no docs/CMake work
git status --short -> clean before patch
git log --oneline -5 -> reviewed 067aebd, a472f1f, 1167852, d1189a4, 5509d52
git show --stat --oneline -5 -> reviewed; all five commits were one-file editorui implementation patches
git show --name-only --format="%h %s%n%b" -5 -> reviewed; all five commit subjects were generic `Patch:`
git show --stat --patch --unified=80 067aebd -- engine/editorui/src/EditorPanelModels.cpp -> reviewed
git show --stat --patch --unified=80 a472f1f -- engine/editorui/src/EditorSceneView.cpp -> reviewed
git show --stat --patch --unified=80 1167852 -- engine/editorui/src/EditorSceneView.cpp -> reviewed
git show --stat --patch --unified=80 d1189a4 -- engine/editorui/src/EditorPanelModels.cpp -> reviewed
git show --stat --patch --unified=80 5509d52 -- engine/editorui/src/EditorSceneView.cpp -> reviewed
git diff --stat -> ENGINE_STATE.md only, 1 file changed, 33 insertions, 31 deletions
git diff --check -> passed; Git warned that LF will be replaced by CRLF the next time Git touches ENGINE_STATE.md
git status --short after patch -> `M ENGINE_STATE.md` only
cmake configure/build -> intentionally skipped by operator constraint: architecture sync only, edit only ENGINE_STATE.md, no code
probes/tests -> intentionally skipped by operator constraint: no probes, no tests
docs update -> intentionally skipped by operator constraint: do not edit docs/
zip readability test -> intentionally skipped by operator constraint: do not create zip archives
```

Next recommended step:

```text
Prefer one more narrow editorui verification or behavior patch before a broader feature if verification finds gaps.
Best one-file candidates:
1. EditorSceneView.cpp: verify whether focus/frame/snap axis paths should suppress revisions when sanitized inputs leave the camera pose unchanged.
2. EditorSceneView.cpp: harden remaining enum-backed overlay/tool/space inputs to mirror the camera-mode/requested-mode sanitizer pattern.
3. EditorPanelModels.cpp: verify whether panel diagnostics should expose named failure reasons for invalid selected indexes, duplicate property paths, and empty property paths instead of only `ok=false`.
4. EditorPanelModels.cpp: harden Asset Browser selected-index normalization at model construction/update boundaries if any caller can persist an out-of-range index.
5. Otherwise continue with v10.9 - Asset Browser thumbnails and import status model, but only as a normal feature patch with docs/probes/CMake policy restored.
```

## Post-v10.8 architecture sync notes

Reviewed commits:

```text
067aebd Patch: Panel-model asset selection diagnostics
a472f1f Patch: Scene View navigation pose no-op suppression
1167852 Patch: Scene View navigation coordinate clamping
d1189a4 Patch: Panel-model property path diagnostics
5509d52 Patch: Scene View orthographic ray origin clamping
```

Changed subsystems:

```text
engine/editorui Scene View orthographic ray construction, 2D pan/zoom, 3D pan/orbit/dolly navigation, camera-coordinate clamping, and navigation revision reporting
engine/editorui Panel model diagnostics for property path uniqueness/emptiness, Asset Browser selected-index validity, and diagnostic summary output
```

Architectural risks noticed:

```text
The last five patches were implementation-only and one-file, so docs/probe coverage is intentionally skipped in this sync until the operator lifts the no-docs/no-probes/no-tests constraint.
Scene View navigation now suppresses revision/status updates when clamping makes pan/orbit/dolly pose unchanged; any callers that treated input activity as a guaranteed changed result should rely on the returned `changed` flag.
Scene View camera-coordinate policy now covers more ray/navigation paths, but focus/frame/snap-axis paths still merit a one-file no-op/revision review for consistency.
Panel-model diagnostics now fail on empty/duplicate property paths and invalid asset selected indexes, but diagnostics still expose only aggregate counts plus a summary string; named failure reasons would make probe output more actionable.
Asset Browser selected-index validity is now checked during diagnostics, but a separate normalization boundary may still be needed if runtime callers can retain an out-of-range selected index.
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
