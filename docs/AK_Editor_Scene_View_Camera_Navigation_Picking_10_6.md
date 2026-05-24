# AK Engine v10.6 - Editor Scene View Camera Navigation and Picking Hardening

## Goal

This patch hardens the v10.5 Scene View runtime camera contract before deeper viewport renderer work. Camera reset, framing, axis snaps, focus, 3D orbit/pan, zoom-to-cursor picking, and invalid input handling now live in `engine/editorui` state and are covered by `ak_editorsceneviewprobe`.

## Runtime additions

Updated module:

```text
engine/editorui/include/AK/EditorUI/EditorSceneView.hpp
engine/editorui/src/EditorSceneView.cpp
engine/editorui/src/EditorSceneViewProbe.cpp
```

New public runtime pieces:

```text
EditorSceneViewAxis
EditorSceneViewBounds
SanitizeEditorSceneViewCamera
ResetEditorSceneViewCamera
FrameEditorSceneViewCamera
SnapEditorSceneViewCameraToAxis
```

`EditorSceneViewCamera` now also stores `focusY`, so 3D focus, frame, and axis snap operations keep a deterministic camera target instead of only preserving X/Z.

## Camera commands

New command IDs:

```text
ViewFrameAll
ViewAxisTop
ViewAxisFront
ViewAxisRight
```

Default command names:

```text
view.frame_all
view.axis_top
view.axis_front
view.axis_right
```

The editor routes them through the existing command registry and menu/runtime command path. The GDI shell only dispatches the command; camera math remains in `EditorSceneView`.

Menu entries:

```text
Window / Scene View / Frame All
Window / Scene View / View Top
Window / Scene View / View Front
Window / Scene View / View Right
```

## Picking and navigation contracts

The Scene View runtime now rejects unusable viewport rectangles and non-finite camera inputs when building pick rays. Ground-plane intersection also rejects non-finite output points.

2D zoom keeps the world point under the cursor stable after changing `orthographicScale`.

3D mode switch, reset, focus, frame, and orbit position the camera so the center ray intersects the requested focus plane at the target point. Middle-drag 3D pan moves both camera and focus together in the camera plane, so the center ray remains stable. Wheel dolly is clamped to keep the focus distance finite and in front of the camera. Frame All uses scene bounds plus the active viewport rectangle to fit 2D orthographic scale or 3D perspective distance.

Axis snaps are deterministic:

```text
Top   -> 3D perspective camera looking down the Y axis
Front -> 3D perspective camera looking along +Z
Right -> 3D perspective camera looking along -X
```

## Probe

`ak_editorsceneviewprobe` now validates:

```text
2D -> 3D mode switching
orthographic center ray
perspective center ray
ground-plane intersection
orbit navigation and 3D pan focus preservation
focus selected math
reset view
frame bounds
top/front/right axis snaps
2D zoom-to-cursor picking stability
invalid viewport / ray rejection
```

Expected output:

```text
[ ok ] editor scene view foundation
editor-scene-view modeSwitch=1 ray2D=1 ray3D=1 ground=1 navigation=1 focus=1 reset=1 frame=1 axis=1 stablePick=1 invalidInput=1 overlay=1 overlayHit=1 overlayCommand=1 ok=1
```
