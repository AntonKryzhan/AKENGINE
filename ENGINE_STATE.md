# AK Engine State

Last updated by Codex patch: post-v10.8 architecture/state sync after latest five one-file editorui scroll/overlay/asset-selection hardening commits.

## Current repository baseline

- Root project: `AKEngine`
- Primary local path used by the operator: `D:\AKENGINE`
- Build system: CMake
- Primary compiler/generator: Visual Studio 17 2022 / MSVC
- Alternate local generator: Ninja with MSVC environment
- Language target: C++20/23 style, current CMake requires `cxx_std_20`
- Current project version in `CMakeLists.txt`: `10.8.0`
- Latest functional patch present in this archive: post-v10.8 one-file editorui Scene View, Asset Browser interaction, and scroll clipping/range overflow hardening patches after `v10.8 - Inspector Component Editing Expansion`

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
- Post-v10.8 one-file hardening: Inspector property editing validation/live-preview/cancel correctness, Scene View camera/navigation/ray/overlay bounds and viewport guards, Asset Browser selection ID normalization, and editor scroll clipping/range overflow robustness.

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
- Recent editor scroll hardening now increments scroll revision when metrics change even if the clamped offset stays the same, normalizes negative content/viewport metrics before clamping/thumb layout, propagates invalid clip-stack state, and saturates clip rectangle ends, virtual list/grid content pixels, range counts, item visibility math, ceil division, and wheel deltas.

## Current generated/cache state

This archive may contain `.akcache/` and `build/` directories. Treat them as local generated state, not source of truth. Codex should not edit these directories unless a task explicitly targets generated artifacts.

## Latest patch summary

Patch: `post-v10.8 - architecture/state sync after latest five one-file editorui scroll/overlay/asset-selection hardening commits`.

Changed files:

```text
ENGINE_STATE.md
```

Intent:

```text
Synchronize the repository state file with the latest five one-file editorui commits without changing production code, CMake, docs, probes, tests, generated output, or zip artifacts.
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
git log --oneline -5 -> reviewed 2f5e7b4, 3242dc6, d1353b1, 85ca850, 4575c01
git show --stat --oneline -5 -> reviewed; all five commits were one-file editorui implementation patches
git show -5 --format=fuller --name-status -> reviewed; commit subjects were generic `Patch:`
git show -5 --unified=80 -- engine/editorui/src/EditorScrollClip.cpp engine/editorui/src/EditorInteraction.cpp engine/editorui/src/EditorSceneView.cpp -> reviewed only the affected one-file editorui diffs from git history
git show --stat --patch --unified=20 85ca850 -- engine/editorui/src/EditorSceneView.cpp -> reviewed
git show --stat --patch --unified=20 d1353b1 -- engine/editorui/src/EditorScrollClip.cpp -> reviewed
git show --stat --patch --unified=20 3242dc6 -- engine/editorui/src/EditorInteraction.cpp -> reviewed
git show --stat --patch --unified=30 4575c01 -- engine/editorui/src/EditorScrollClip.cpp -> reviewed
git show --stat --patch --unified=30 2f5e7b4 -- engine/editorui/src/EditorScrollClip.cpp -> reviewed
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
1. EditorScrollClip.cpp: add focused regression coverage only if the operator lifts the no-tests/no-probes limit for saturated clip rect ends, invalid clip-stack propagation, virtual range count saturation, and `EnsureEditorItemVisible` viewport-end overflow.
2. EditorSceneView.cpp: add focused regression coverage only if the operator lifts the no-tests/no-probes limit for overlay control placement/hit-testing near extreme viewport coordinates.
3. EditorInteraction.cpp: add focused regression coverage only if the operator lifts the no-tests/no-probes limit for Asset Browser selection with raw GUIDs, `asset:`-prefixed stable IDs, empty stable IDs, and empty item GUIDs.
4. Otherwise continue with v10.9 - Asset Browser thumbnails and import status model, but only as a normal feature patch with docs/probes/CMake policy restored.
```

## Post-v10.8 architecture sync notes

Reviewed commits:

```text
2f5e7b4 Patch: Editor item visibility viewport-end saturation
3242dc6 Patch: Asset Browser selection stable ID normalization
d1353b1 Patch: Editor virtual list/grid range count and index saturation
85ca850 Patch: Scene View overlay coordinate saturation
4575c01 Patch: Editor clip rectangle end saturation and invalid clip-stack propagation
```

Changed subsystems:

```text
engine/editorui Editor scroll clipping, nested clip-stack invalid state, virtual list/grid range math, and item visibility overflow guards
engine/editorui Scene View overlay control coordinate placement and orientation-widget hit-test rectangle construction
engine/editorui Editor interaction Asset Browser item lookup by raw GUID or `asset:`-prefixed stable ID
```

Architectural risks noticed:

```text
The last five patches were implementation-only and one-file, so docs/probe coverage is intentionally skipped in this sync until the operator lifts the no-docs/no-probes/no-tests constraint.
Editor scroll and clip math now saturates rectangle ends, virtual range counts, range indexes, and item visibility viewport ends; this prevents signed/size overflow but means extreme coordinates or collection sizes collapse to bounded values rather than preserving exact magnitude.
Invalid nested clip-stack pushes now preserve invalid state until popped; callers that expected a child push to recover clipping from an invalid parent should be checked before changing this behavior again.
Scene View overlay controls now saturate coordinate additions; this avoids overflow near extreme viewport positions but can pile controls at coordinate limits and should be verified with hit-testing if extreme window coordinates become realistic.
Asset Browser lookup now rejects empty stable IDs and empty item GUIDs; this is safer for selection identity, but any placeholder asset rows that intentionally used empty GUIDs will no longer be selectable through this path.
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
