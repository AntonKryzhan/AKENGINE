# AK Engine v9.8 — Editor Diagnostics Interaction / Filtering Runtime Foundation

This patch turns the diagnostics tab from a passive rendered surface into an interaction-ready runtime model.

## Added

- `EditorDiagnosticsInteractionState`
- `EditorDiagnosticsInteractionInput`
- `EditorDiagnosticsInteractionResult`
- Diagnostics filter state derived from UI interactions
- Search query state
- Row selection state with stable row id retention
- Scroll state independent from Console scroll
- Badge-driven filter toggles
- Mark-all-read activity operation
- Selected notification dismissal operation
- Clear-completed-tasks operation
- Command invocation from selected diagnostics rows
- `ak_editordiagnosticsinteractionprobe`

## Runtime bridge

`ak_editor.exe` now keeps diagnostics interaction state and uses it when building `EditorDiagnosticsPanelState`.
The diagnostics tab supports row selection, wheel scrolling, and badge toggles for unread/warnings/errors/tasks.

## Safety

The interaction layer does not mutate panel rows directly. It mutates the filter/session state and, when needed,
uses safe Activity Center APIs for notification/task actions.
