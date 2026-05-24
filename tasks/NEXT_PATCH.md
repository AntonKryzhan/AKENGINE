# NEXT PATCH TASK — AK Engine

You are working in the local AK Engine repository.

## Required reading

Before editing files, read:

1. `AGENTS.md`
2. `ENGINE_STATE.md`
3. `ENGINE_ROADMAP.md`
4. `QUALITY_GATE.md`
5. nearby docs and CMake files for the subsystem you choose to modify

## Task

Find the next logical unclosed development stage and implement one maximally dense but controlled patch.

Default priority: continue from the current v10.8 editor runtime state unless the build is broken.

Preferred next stage:

```text
v10.9 — Asset Browser thumbnails and import status model
```

Expected direction for v10.9:

- extend the Asset Browser item view model with import/cook status, icon kind, GUID, source path, and cooked path where the current contracts support it;
- add search/filter/sort state without depending on final Vulkan/ImGui rendering;
- add thumbnail placeholder descriptors for texture, mesh, material, scene, and point cloud assets;
- preserve stable asset GUID/source path contracts;
- extend an existing asset/editor probe instead of adding a new probe target unless a new target is explicitly required;
- update docs and `ENGINE_STATE.md`.

If inspection shows a more urgent broken build, regression, missing CMake registration, or unsafe API issue, fix that instead and document why it took priority.

## Hard constraints

- Do not make cosmetic refactors.
- Do not rename or move unrelated files.
- Do not touch generated/cache/build output.
- Do not break existing public APIs, serialized formats, scene compatibility, command IDs, or asset contracts unless the patch explicitly includes a migration.
- Do not add external dependencies.
- Do not fake test success.
- Do not commit or push automatically.

## Required checks

Follow `QUALITY_GATE.md`.

At minimum, attempt:

```powershell
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug
```

Then run the generic probes and the subsystem probe that matches the patch.

For the preferred v10.9 stage, run:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_editorpanelprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_assetdbprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editormenutoolbarprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_editorcommandstateprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_commandprobe.exe
```

Run `ak_editor.exe` if a Windows desktop session is available.

## Required final response

Use this exact structure:

```text
Patch:
Changed files:
Checks run:
Checks passed:
Checks skipped / failed:
Public API or format changes:
Next recommended step:
```

Also update `ENGINE_STATE.md` with the patch summary, changed files, verification result, and next step before completing.
