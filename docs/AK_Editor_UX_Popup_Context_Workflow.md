# AK Engine v8.0 — Editor UX Polish / Popup Context Workflow Foundation

This patch moves the editor from a layout/widget mockup toward a production editor UX model.
It keeps the current Win32/GDI shell, but adds editor-side contracts for Unity-like context menus,
anchored popups, compact panel proportions and dropdown workflows.

## Added systems

- `EditorPopupWorkflowDesc`
- `EditorUXFrame`
- `EditorUXDiagnostics`
- GameObject/Create menu workflow
- Hierarchy context menu workflow
- Project Browser context menu workflow
- Inspector Add Component workflow
- Scene View Shading dropdown workflow
- Grid/Snap popup workflow
- `ak_editoruxprobe`

## Visual tuning

The default docking proportions are tightened for a more editor-like layout:

- narrower Hierarchy zone;
- narrower Inspector zone;
- larger Scene View;
- smaller Project bottom zone;
- denser menu/toolbar/tab/status rows.

The toolbar model is also compacted. Heavy creation commands remain available through menu/context workflows,
while the visible toolbar behaves more like a grouped editor toolbar.

## Next step

The follow-up should connect the popup workflow model to actual mouse interaction in `ak_editor.exe`:
right-click context menus, Add Component popup, Scene Shading dropdown and Project item context menu.
