# AGENTS.md — AK Engine Codex Operating Rules

These instructions apply to the whole repository unless a more specific `AGENTS.md` exists in a subdirectory.

## Project identity

AK Engine is a Windows-first custom game engine written in C++20/23 with CMake and MSVC / Visual Studio 2022 as the primary toolchain. The renderer is Vulkan-first, with Slang/HLSL/SPIR-V shader tooling planned or partially staged. The editor intentionally borrows useful workflow ideas from Unity/Unreal/Godot, but the implementation must remain AK Engine-specific and architecture-driven.

Default local project path for the operator is:

```text
D:\AKENGINE
```

Primary Windows build:

```powershell
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug
```

Ninja/MSVC build is also supported when the environment is configured:

```powershell
cmake --preset windows-ninja-debug
cmake --build --preset windows-ninja-debug
```

## Required context before edits

Before changing code, read these files from the repository root:

1. `AGENTS.md`
2. `ENGINE_STATE.md`
3. `ENGINE_ROADMAP.md`
4. `QUALITY_GATE.md`
5. the active task file, usually `tasks/NEXT_PATCH.md`

Also inspect the nearest module documentation in `docs/` and the affected `CMakeLists.txt` files before adding/removing targets.

## Patch discipline

Make one controlled patch per task.

Do:

- Prefer the next narrow engine/editor/runtime layer that unlocks future work.
- Keep changes minimal but complete enough to compile and be testable.
- Add or update a focused probe executable for every new runtime subsystem or non-trivial editor model.
- Update documentation for new contracts, file formats, commands, tools, public behavior, or architecture decisions.
- Update `ENGINE_STATE.md` after the patch.
- Update CMake only for files/targets that are actually added or removed.
- Preserve existing public APIs, serialized formats, command IDs, asset IDs, scene versions, and behavior unless the task explicitly requires a migration.
- Prefer backward-compatible additive changes.

Do not:

- Do cosmetic refactors, mass formatting, renames, folder moves, or include reshuffles unrelated to the task.
- Replace working systems with stubs or pseudocode.
- Add TODO-only code, fake implementations, or silent fallbacks that hide errors.
- Touch generated/cache/build output unless the task is specifically about generated artifacts.
- Commit or push automatically.
- Introduce external dependencies without an explicit task and a documented integration plan.
- Copy Unity/Unreal/Godot internals blindly; use them only as product/workflow inspiration.

## Repository areas that should normally stay unchanged

Avoid editing these unless explicitly required:

```text
build/
.akcache/
logs/
third_party/
```

`build/`, `.akcache/`, and `logs/` may exist locally and should not be treated as source of truth.

## Core invariants

Do not break these architectural decisions:

- Units: meters, seconds, kilograms.
- Angles: radians internally, degrees only at UI/tool boundaries.
- World convention: right-handed, Y-up.
- Large worlds: authoritative world position is `int64` cell + local `double`; GPU/render paths should use camera-relative floats.
- Depth: reversed-Z policy is the default renderer direction.
- Simulation timing: fixed tick with `uint64` tick/frame counters; no absolute game time stored as `float`.
- Entity references: runtime entity handles are generational; persistent references use GUID-style IDs where appropriate.
- Assets: scenes and cooked data should refer to stable `AssetGuid`, not fragile source paths.
- Resources: generation handles and deferred release are required; GPU-style resources must not be destroyed while a frame may still reference them.
- Serialization: every engine-owned persistent format needs magic/version/schema policy and migration path.
- Save/write: write atomically through temp file + flush + replace/rename where possible.
- Diagnostics: new subsystems should expose counters/events/probe output that make failures visible.
- Job/physics determinism: when parallelizing, sort/merge deterministically and avoid unordered iteration in gameplay-visible outputs.

## C++ style

- Use C++20-compatible code unless a task explicitly raises the requirement.
- Keep headers self-contained.
- Prefer explicit small structs and free functions for foundation layers.
- Use `namespace AK` conventions already present in nearby files.
- Match nearby naming, include order, indentation, and file organization.
- Keep hot runtime paths allocation-conscious.
- Validate finite numeric values at engine boundaries where transforms, physics, render, or serialization can be corrupted.
- Prefer `Result<T>` / explicit error reporting where the existing module uses it; do not add broad exception-based runtime control flow.
- Do not introduce global mutable state unless the existing subsystem already owns that lifecycle.

## Editor/runtime rules

- Editor UI runtime models should be testable without a real Win32/Vulkan window when possible.
- GDI editor shell code may remain a bridge, but new editor behavior should be represented as engine/editorui state first.
- Hotkeys, menus, toolbars, command palette entries, and command state should share the command registry/action layer rather than hardcoding unrelated paths.
- Scene View changes must preserve 2D/3D mode, transform mode, transform space, selection, undo, dirty flags, and property editing contracts.

## Probe and test expectations

When adding a feature, add or extend a probe that can fail with a non-zero exit code. A probe should cover the contract, not just construction. For editor-runtime features, prefer deterministic model tests such as hit-test, projection, command state, property mutation, serialization round-trip, or state transition checks.

## Final response format for local Codex runs

At the end of a task, report:

- patch title and intent;
- changed files;
- checks run;
- check result, including exact failure reason if a check could not run;
- any public API or format changes;
- next recommended step.

Do not claim a build/test passed unless the command actually ran and returned success.
