# AK Engine v9.7 — Editor Diagnostics View / Runtime Panel Foundation

This patch turns the structured diagnostics model from v9.6 into a render-facing editor diagnostics view.

## Goals

- Build a compact visual state for the Diagnostics tab.
- Separate diagnostics aggregation from diagnostics presentation.
- Keep renderer independent from `AK::EditorUI` internals.
- Provide severity badges, metric tiles and virtualized diagnostic rows.
- Feed the current GDI shell without locking future Vulkan/ImGui UI to GDI-specific code.

## Added systems

- `EditorDiagnosticsViewState`
- `EditorDiagnosticsViewBadge`
- `EditorDiagnosticsViewMetricTile`
- `EditorDiagnosticsViewRowSurface`
- `BuildEditorDiagnosticsViewState`
- `ak_editordiagnosticsviewprobe`

## Runtime bridge

`ak_editor` now builds a live diagnostics source each frame from:

- activity notifications;
- background task center;
- console lines;
- diagnostic counters/events;
- autosave runtime;
- workspace load/repair report;
- command state cache.

The result is converted into simple `EditorDiagnostics*RenderDesc` data in `EditorFrameDesc`, so `ak_render` does not depend on the full editor UI module.

## GDI shell integration

The `Diagnostics` tab now renders:

- search field placeholder;
- severity/status chips;
- pinned metric tiles;
- diagnostic row list with severity icons;
- unread/sticky/active badges;
- scrollbar and status summary.

The panel is still a foundation view, not a final profiler, but it is no longer just placeholder text.
