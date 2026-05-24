# AK Engine v9.2 — Editor Preferences / Startup Profile Foundation

This patch adds a durable editor preferences layer above the layout, session, theme and shortcut foundations.

## Goals

- Keep user-editable editor behavior out of hardcoded renderer/editor shell state.
- Provide a stable `AKEDITORPREFS 1` profile format.
- Repair invalid preferences instead of crashing or silently accepting bad state.
- Build a deterministic startup plan that wires together theme, layout, session and shortcuts.

## Added systems

```text
engine/editorui
  EditorPreferencesProfile
  EditorAutosavePreferences
  EditorUiPreferences
  EditorViewportPreferences
  EditorSafetyPreferences
  EditorPreferencesPolicy
  EditorPreferencesRepairReport
  EditorStartupPlan
```

## Preference groups

```text
Startup:
  startup mode
  project root
  layout/session/shortcut paths
  recent projects

UI:
  theme profile
  DPI scale
  tooltip/status/debug UI toggles
  compact toolbar
  scroll lines
  asset icon scale

Autosave:
  enabled flag
  interval seconds
  max backup count
  save layout/session flags

Viewport:
  grid/gizmo/stats visibility
  camera move speed
  camera look sensitivity

Safety:
  confirm delete
  confirm scene close
  atomic save
  crash recovery backup
  block unsafe text shortcuts
```

## Repair policy

The loader repairs:

```text
unsupported version
parse failure
invalid theme profile
invalid startup mode
invalid DPI scale
invalid autosave interval
invalid backup count
invalid mouse wheel line count
invalid asset icon scale
invalid camera sensitivity
empty recent project
duplicate recent project
```

## Startup plan

`BuildEditorStartupPlan()` produces the future editor boot plan:

```text
preferences
  -> theme profile + DPI scale
  -> shortcut profile
  -> layout path
  -> session path
  -> project root
  -> autosave/session safety flags
```

This is still a foundation layer. It does not yet implement a full Preferences window UI. That should follow as a dedicated editor panel using the existing Inspector/property widget system.

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_editorpreferencesprobe
.\build\windows-vs-debug\bin\Debug\ak_editorpreferencesprobe.exe
```
