# AK Engine v10.7 - Scene View Overlay and Orientation Widget Foundation

## Goal

This patch adds a deterministic Scene View overlay model in `engine/editorui`, so viewport controls can be tested without depending on the current Win32/GDI shell. It builds on the v10.6 camera and picking contracts and keeps control state, hit-testing, and command routing data in editor runtime code.

## Runtime additions

Updated module:

```text
engine/editorui/include/AK/EditorUI/EditorSceneView.hpp
engine/editorui/src/EditorSceneView.cpp
engine/editorui/src/EditorSceneViewProbe.cpp
```

New public runtime pieces:

```text
EditorSceneViewOverlayControl
EditorSceneViewOverlayBuildInput
EditorSceneViewOverlayControlDesc
EditorSceneViewOverlaySurface
EditorSceneViewOverlayHit
BuildEditorSceneViewOverlaySurface()
HitTestEditorSceneViewOverlay()
```

The overlay surface is backend-neutral. It describes screen rectangles, labels, active state, enabled state, and the command each control should dispatch.

## Controls

The default Scene View overlay surface exposes:

```text
2D / 3D camera mode controls
Snap state
Focus Selection
Frame All
Reset View
Translate / Rotate / Scale tool state
World / Local transform space state
Orientation X / Y / Z axis controls
```

Command mapping stays on existing command IDs:

```text
Mode2D             -> ViewScene2D
Mode3D             -> ViewScene3D
GridSnap           -> ToggleGridSnap
ToolTranslate      -> ToolTranslate
ToolRotate         -> ToolRotate
ToolScale          -> ToolScale
SpaceWorld         -> ToolSpaceWorld
SpaceLocal         -> ToolSpaceLocal
FocusSelection     -> FocusSelection
FrameAll           -> ViewFrameAll
ResetView          -> ResetViewport
OrientationAxisX   -> ViewAxisRight
OrientationAxisY   -> ViewAxisTop
OrientationAxisZ   -> ViewAxisFront
```

The X/Y/Z orientation controls intentionally map to the already established v10.6 camera snaps. X snaps to the right view, Y snaps to the top view, and Z snaps to the front view.

## Hit-testing contract

`HitTestEditorSceneViewOverlay()` returns an `EditorSceneViewOverlayHit` with:

```text
control
command
itemIndex
actionable
active
label
```

Hidden surfaces and misses return `EditorSceneViewOverlayControl::None` and are not actionable. Disabled controls, such as Focus Selection with no current selection, still hit deterministically but are returned as non-actionable.

## Probe

`ak_editorsceneviewprobe` now also validates:

```text
overlay surface construction from camera/tool/space/snap/selection state
orientation widget rectangle generation
active rotate/local/snap state
X-axis orientation hit-test
orientation X -> ViewAxisRight command mapping
Focus Selection hit-test and enabled command mapping
```

Expected output includes:

```text
overlay=1 overlayHit=1 overlayCommand=1 ok=1
```
