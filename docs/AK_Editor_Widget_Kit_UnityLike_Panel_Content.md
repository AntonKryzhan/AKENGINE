# AK Engine v7.9 — Editor Widget Kit / Unity-like Panel Content Foundation

This patch moves the editor UI one layer above dock rectangles and debug text rendering.

The v7.3-v7.8 series established the layout tree, runtime bridge and a darker visual shell. v7.9 adds the first reusable widget/content grammar that AK Editor needs before the future Vulkan/ImGui rewrite:

- hierarchy tree rows with icon slots, search row, scene root and visibility gutter;
- inspector object header with enable/name/static/tag/layer controls;
- inspector component headers and property-style rows;
- vec3 field grammar for Transform;
- project browser split view: folder tree + asset icon grid;
- scene viewport internal toolbar with Pivot/Global/2D/Shaded/Snap/Gizmos controls;
- GDI vector-icon placeholders for entity, mesh, camera, light, folder, asset, material, shader, transform and bounds;
- data-only `EditorWidgets` module for future renderer-independent widgets;
- context menu and popup models for hierarchy menus and viewport stats;
- `ak_editorwidgetprobe` to validate widget counts, rects, ids, context menus and popup models.

The patch still intentionally keeps the current Win32/GDI shell. It does not introduce ImGui or Vulkan UI rendering yet. The point is to make the visible editor look and behave more like a real scene editor while keeping the UI model renderer-independent.

## New module surface

```text
engine/editorui/include/AK/EditorUI/EditorWidgets.hpp
engine/editorui/include/AK/EditorUI/EditorWidgetProbe.hpp
engine/editorui/src/EditorWidgets.cpp
engine/editorui/src/EditorWidgetProbe.cpp
tools/editorwidgetprobe
```

## Visual effect in ak_editor

The main visible improvement is that the old debug text panels are no longer the primary visual grammar:

- Hierarchy now reads as a tree, not a raw log list.
- Inspector now reads as component/property UI, not a diagnostic dump.
- Project now reads as a browser with tree + grid cells.
- Scene View has an internal toolbar similar to Unity's Scene toolbar.

The underlying editor state is still the existing scene, ECS, asset registry and runtime bridge. Property editing remains future work; v7.9 only establishes the widget/content foundation and visual rendering path.
