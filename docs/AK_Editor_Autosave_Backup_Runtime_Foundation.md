# AK Engine v9.4 — Editor Autosave / Backup Runtime Foundation

This patch adds a dedicated editor autosave runtime policy layer instead of hardcoding scene autosave timing directly in the editor loop.

## Goals

- Drive autosave behavior from editor preferences.
- Separate scene autosave, crash-recovery backups, and workspace/session saves.
- Avoid saving the same dirty revision repeatedly.
- Keep backup rotation deterministic and testable.
- Prepare the editor for layout/session/preferences autosave without tying it to GDI drawing code.

## Added systems

- `EditorAutosavePolicy`
- `EditorAutosavePaths`
- `EditorAutosaveRuntimeState`
- `EditorAutosaveTickInput`
- `EditorAutosaveTickResult`
- `EditorAutosaveAction`
- `EditorBackupRotationPlan`

## Runtime model

```text
preferences
  -> EditorAutosavePolicy
workspace paths + scene path
  -> EditorAutosavePaths
frame delta + dirty revisions
  -> TickEditorAutosaveRuntime
  -> SceneBackup / SceneAutosave / WorkspaceSave actions
```

The runtime tracks the last saved scene and workspace revisions. Live editor state may change every frame, but autosave only emits actions when the configured interval has passed and a revision has not already been saved.

## Backup rotation

`BuildEditorBackupRotationPlan` sorts backup names deterministically, keeps the newest `maxBackupCount` entries, and marks older files for pruning. The foundation is intentionally deterministic so tests do not depend on filesystem enumeration order.

## Probe

Run:

```powershell
cmake --build --preset windows-vs-debug --target ak_editorautosaveprobe
.\build\windows-vs-debug\bin\Debug\ak_editorautosaveprobe.exe
```
