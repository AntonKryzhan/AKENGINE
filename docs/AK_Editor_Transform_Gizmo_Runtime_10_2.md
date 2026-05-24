# AK Engine v10.2 — Editor Transform Gizmo Runtime Foundation

This patch adds the first runtime-backed transform gizmo for the GDI scene viewport.

## Goals

- Add a reusable editor transform gizmo model outside `editor/src/main.cpp`.
- Give selected scene entities visible translate handles in the Scene View.
- Route gizmo hit-testing before normal viewport selection and free-drag movement.
- Preserve existing keyboard transform controls, undo snapshots, grid snap, dirty flags and world-position synchronization.
- Keep the renderer contract data-only so the future Vulkan/ImGui editor can draw the same gizmo model.

## Added module

```text
engine/editorui/include/AK/EditorUI/EditorTransformGizmo.hpp
engine/editorui/include/AK/EditorUI/EditorTransformGizmoProbe.hpp
engine/editorui/src/EditorTransformGizmo.cpp
engine/editorui/src/EditorTransformGizmoProbe.cpp
tools/editortransformgizmoprobe
```

The module provides:

- selected target projection into viewport screen space;
- X, Y, Z and XZ-plane translate handles;
- handle hit testing;
- drag begin/update/end runtime state;
- snap-aware absolute transform output;
- diagnostics for handle construction, hit testing, plane drag, vertical drag and snap.

## Editor integration

```text
selected TransformComponent
  -> EditorTransformGizmoBuildInput
  -> EditorTransformGizmoSurface
  -> HitTestEditorTransformGizmo before viewport selection
  -> BeginEditorTransformGizmoDrag
  -> UpdateEditorTransformGizmoDrag
  -> TransformComponent.position
  -> SanitizeTransformComponent
  -> SyncWorldPositionFromTransform
  -> MarkTransformDirty / scene dirty / undo
```

The current viewport is still top-down XZ, so the gizmo uses:

```text
X handle       horizontal screen drag -> world X
Z handle       vertical screen drag   -> world Z
Y handle       vertical screen drag   -> world Y
XZ handle      free planar drag       -> world X/Z
```

## Renderer integration

`EditorFrameDesc` now contains `EditorViewportGizmoRenderDesc`.
The GDI renderer draws:

- axis lines from selected entity origin;
- labeled X, Y, Z and XZ handles;
- hover/active state;
- small mode/status label.

No editor rendering code depends directly on the runtime gizmo implementation; `editor/src/main.cpp` maps the runtime surface into a render descriptor.

## Probe

```text
ak_editortransformgizmoprobe
```

Checks:

- surface visibility;
- four visible handles;
- X-axis hit test;
- XZ-plane drag;
- snap rounding;
- Y-axis drag.

## Notes

This is a transform-gizmo foundation, not the final Unity/Unreal-class manipulator. Rotation rings, scale cubes, local/world orientation, multi-selection pivots, screen-space precision handles and ImGui/Vulkan drawing should be layered on top of this runtime contract rather than hardcoded into the editor window loop.
