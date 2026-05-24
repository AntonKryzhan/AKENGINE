# AK Engine v9.6 — Editor Diagnostics Panel / Activity Console Integration Foundation

This patch adds a dedicated editor diagnostics panel model that merges runtime editor signals into one inspectable stream instead of scattering them across ad-hoc console lines.

## Added

- `EditorDiagnosticsPanelState`
- diagnostics rows for activity notifications, background tasks, console entries, metrics, autosave, workspace and command state
- severity/category/source model
- diagnostics text filter and severity filter
- counters for unread/warnings/errors/active tasks/sticky rows
- metrics bridge for the existing Diagnostics panel model
- console synchronization pass for activity/workspace/autosave rows
- `ak_editordiagnosticspanelprobe`

## Why

The editor already has workspace recovery, autosave, notifications, shortcuts, command state and panel models. Without an aggregation layer every subsystem would keep adding its own console-only messages. The new diagnostics panel foundation makes these systems visible as structured rows and metrics that can later be rendered as a proper Unity-like Diagnostics/Activity Center tab.

## Current scope

This is a model/runtime foundation. It does not yet replace the visual Console tab with a full diagnostics grid. The current `ak_editor.exe` links the system and reports probe diagnostics in the existing console. A later patch can draw the structured rows directly in the `Diagnostics` panel.
