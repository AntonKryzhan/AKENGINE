# AK Engine v10.5 — Editor Scene View 2D / 3D Runtime Foundation

## Goal

This patch turns the Scene View camera into an explicit runtime/editor contract instead of a hardcoded top-down viewport. The GDI editor can now switch between a 2D orthographic Scene View and a 3D perspective Scene View, while keeping the same command, menu, toolbar, picking, focus, and future Vulkan viewport path.

## Added runtime model

New module:

```text
engine/editorui/include/AK/EditorUI/EditorSceneView.hpp
engine/editorui/src/EditorSceneView.cpp
engine/editorui/src/EditorSceneViewProbe.cpp
```

Core types:

```text
EditorSceneViewMode
  Mode2D
  Mode3D

EditorSceneViewProjection
  Orthographic
  Perspective

EditorSceneViewCamera
EditorSceneViewRay
EditorSceneViewGroundPoint
EditorSceneViewNavigationInput
EditorSceneViewNavigationResult
```

The camera stores both 2D and 3D state:

```text
2D:
  centerX / centerZ
  orthographicScale

3D:
  positionX / positionY / positionZ
  yawDegrees / pitchDegrees
  fovYDegrees
  nearPlane / farPlane
```

## Added commands

```text
ViewScene2D
ViewScene3D
```

Default command names:

```text
view.scene_2d
view.scene_3d
```

These are visible through command registry, command palette, menu, and toolbar routing.

## Added menu / toolbar entries

Menu:

```text
Window / Scene View / 2D Mode
Window / Scene View / 3D Mode
```

Toolbar:

```text
2D
3D
```

The active Scene View mode is tracked by `EditorCommandStateCache`, so the currently selected mode gets checked-state like transform tools and transform space.

## Editor behavior

2D mode:

```text
projection: orthographic
viewport: XZ top-down
wheel: zoom to cursor
RMB/MMB drag: pan
picking: ground-plane point from orthographic ray
```

3D mode:

```text
projection: perspective
viewport: 3D floor grid preview in GDI renderer
wheel: camera dolly along forward vector
RMB drag: orbit/look yaw + pitch
picking: ray to ground plane
focus selection: keeps selected entity centered and positions 3D camera behind the look direction
```

The current GDI renderer is still not the final Vulkan viewport, but it now has a real 3D camera contract and a perspective floor-grid preview. This removes the architectural limitation where Scene View was implicitly always 2D.

## Renderer bridge

`EditorFrameDesc` now exposes:

```text
viewportMode
viewportProjection
viewportPerspective
viewportCameraX/Y/Z
viewportYawDegrees
viewportPitchDegrees
viewportFovYDegrees
```

The software/GDI editor renderer uses these fields to draw a different Scene View overlay and perspective grid in 3D mode. Later Vulkan/ImGui viewport work should consume the same Scene View camera fields instead of inventing another camera state.

## Probe

New tool:

```text
ak_editorsceneviewprobe
```

It validates:

```text
2D -> 3D mode switching
orthographic center ray
perspective center ray
ground-plane intersection
orbit navigation
focus selected math
```

Expected output:

```text
[ ok ] editor scene view foundation
editor-scene-view modeSwitch=1 ray2D=1 ray3D=1 ground=1 navigation=1 focus=1 ok=1
```
