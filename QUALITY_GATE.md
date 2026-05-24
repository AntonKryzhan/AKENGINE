# AK Engine Quality Gate

Use this checklist for every local Codex patch. A patch is not done until the relevant checks either pass or the exact reason they cannot run is recorded in the final response and `ENGINE_STATE.md`.

## 1. Pre-change checks

From the repository root:

```powershell
git status --short
```

If the tree is dirty and the task did not explicitly allow working over local changes, stop and report the dirty files. Do not overwrite operator work.

Read:

```text
AGENTS.md
ENGINE_STATE.md
ENGINE_ROADMAP.md
QUALITY_GATE.md
tasks/NEXT_PATCH.md
```

Inspect nearby docs and `CMakeLists.txt` files for the subsystem being changed.

## 2. Patch constraints

The patch must:

- be one coherent feature/fix layer;
- preserve existing API, serialized formats, scene compatibility, command IDs, and asset contracts unless migration is explicitly part of the task;
- avoid unrelated refactoring and mass formatting;
- include focused probes for new runtime behavior;
- update docs when contracts or behavior change;
- update `ENGINE_STATE.md` before completion;
- avoid editing `build/`, `.akcache/`, `logs/`, or third-party code unless explicitly required.

## 3. Required build

Primary Windows check:

```powershell
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug
```

If Visual Studio generator is unavailable but Ninja/MSVC is available:

```powershell
cmake --preset windows-ninja-debug
cmake --build --preset windows-ninja-debug
```

If neither can run, record the reason exactly: missing Visual Studio, missing CMake, missing Ninja, non-Windows environment, missing Vulkan SDK, compile error, link error, or another concrete cause.

## 4. Required smoke binaries

When the build succeeds, run the generic smoke checks that exist in the build output:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_commandprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editorcommandstateprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editormenutoolbarprobe.exe
```

For Ninja debug builds, use:

```powershell
.\build\windows-ninja-debug\bin\ak_commandprobe.exe
.\build\windows-ninja-debug\bin\ak_editorcommandstateprobe.exe
.\build\windows-ninja-debug\bin\ak_editormenutoolbarprobe.exe
```

Run the subsystem-specific probe for the changed area. Examples:

```text
Editor Scene View       -> ak_editorsceneviewprobe
Transform Gizmo/Tools   -> ak_editortransformgizmoprobe
Editor properties       -> ak_editorpropertyprobe
Editor layout/session   -> ak_editorlayoutprobe / ak_editorsessionprobe
Input/shortcuts         -> ak_inputprobe / ak_editorshortcutprobe
Assets/VFS/streaming    -> ak_assetdbprobe / ak_vfsprobe / ak_streamingprobe
World/partition/topology-> ak_worldprobe / ak_worldpartitionprobe / ak_worldtopologyprobe
Render/RHI/Vulkan       -> ak_renderprobe / ak_rhiprobe / ak_vulkanprobe / related Vulkan probes
Physics/destruction     -> ak_physicsprobe / ak_destructionprobe / ak_csgprobe / related physics probes
```

Do not claim a probe passed unless it was executed and returned exit code 0.

## 5. Editor run smoke

When the task touches editor window behavior and a Windows desktop session is available:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_editor.exe
```

Manual smoke checklist:

- window opens;
- Scene View draws;
- hierarchy/inspector/asset browser/console are visible;
- current toolbar/menu commands still appear;
- changed editor feature can be triggered at least once;
- no crash on close.

If no desktop session is available, say so explicitly.

## 6. Diff review before completion

Show or inspect:

```powershell
git diff --stat
git diff --check
```

`git diff --check` should not report whitespace errors. If Git is unavailable, inspect the changed files manually and state that Git checks could not run.

## 7. Completion report format

Final Codex response must include:

```text
Patch:
Changed files:
Checks run:
Checks passed:
Checks skipped / failed:
Public API or format changes:
Next recommended step:
```

For failed checks, include the exact command and the first useful diagnostic lines. Do not hide failures behind generic wording.
