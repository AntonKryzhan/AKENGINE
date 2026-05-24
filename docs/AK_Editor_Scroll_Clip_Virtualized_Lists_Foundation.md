# AK Engine v8.3 — Editor Scroll / Clip / Virtualized Lists Foundation

This patch adds the first proper editor scroll, clipping, and virtualized-list layer.

## Why

Unity-like editors quickly break if panels are drawn as raw full lists. Common bugs are:

- Hierarchy rows drawing over tabs/status bars.
- Inspector fields leaking outside the panel body.
- Project grid items drawing under other panels.
- Mouse wheel routed to the wrong panel.
- Long lists forcing all rows to be drawn every frame.
- Scroll offsets going negative or past the end of content.

v8.3 adds a reusable foundation for these cases before the editor grows into large scenes and large asset folders.

## Added

### `engine/editorui/EditorScrollClip`

Core types:

- `EditorScrollState`
- `EditorClipStack`
- `EditorVirtualListRange`
- `EditorVirtualGridRange`
- `EditorScrollbarThumb`

Core functions:

- `ClampEditorScrollOffset`
- `ApplyEditorScrollWheel`
- `EnsureEditorItemVisible`
- `BuildEditorVirtualListRange`
- `BuildEditorVirtualGridRange`
- `BuildEditorScrollbarThumb`
- `PushEditorClipRect` / `PopEditorClipRect`

### Runtime integration

`ak_editor.exe` now keeps per-panel scroll state for:

- Hierarchy
- Inspector
- Project grid
- Console

Mouse wheel is routed by hovered panel:

- Wheel over Scene View still zooms Scene View.
- Wheel over Hierarchy scrolls Hierarchy.
- Wheel over Inspector scrolls Inspector.
- Wheel over Project scrolls Project grid.
- Wheel over Console scrolls Console.

### GDI rendering integration

Renderer-side changes:

- Added scoped GDI clip rects.
- Added vertical scrollbar drawing.
- Hierarchy entity rows are virtualized.
- Project asset grid is virtualized.
- Inspector content is clipped and scrollable.
- Console text list is clipped and scrollable.

Popup/context surfaces stay above panel clipping and are not clipped by parent panels.

## Probe

New tool:

```powershell
cmake --build --preset windows-vs-debug --target ak_editorscrollprobe
.\build\windows-vs-debug\bin\Debug\ak_editorscrollprobe.exe
```

Expected output:

```text
[ ok ] editor scroll / clip / virtualized lists foundation
editor-scroll clip=ok list=... grid=... clamp=... ensure=ok scrollbar=ok ok=true
```

## Next steps

The next UI-critical layer should be input capture and text edit state:

- keyboard capture
- mouse capture
- focused text field
- numeric-field begin/preview/commit/cancel
- command routing suppression while editing text
- undo transaction grouping for property edits
