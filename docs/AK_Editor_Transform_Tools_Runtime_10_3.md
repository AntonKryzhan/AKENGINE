# AK Engine v10.3 — Editor Transform Tools Runtime

## Goal

v10.3 promotes the Scene View transform gizmo from a translate-only overlay to a runtime transform tool system. The editor can now switch the active Scene View tool through registered commands, menu items, toolbar buttons, command palette entries, and checked command state.

## Added runtime behavior

- `ToolTranslate`, `ToolRotate`, and `ToolScale` commands are part of the command registry.
- The default toolbar exposes `Move`, `Rot`, and `Scale` buttons.
- The default menu exposes `Tools / Transform / Translate`, `Rotate`, and `Scale`.
- Command state marks the active transform tool as checked.
- The transform gizmo surface is built from the active tool mode.
- Active drag is cancelled safely when switching tools.

## Gizmo modes

### Translate

The existing translate gizmo remains available:

- center XZ plane handle
- X axis handle
- Y axis handle
- Z axis handle
- snap uses the editor grid snap step

### Rotate

Rotation mode adds drag output for Euler rotation degrees:

- center / Y handle: yaw
- X handle: pitch
- Z handle: roll
- snap uses a rotation snap step of 15 degrees

### Scale

Scale mode adds drag output for non-uniform and uniform scaling:

- center handle: uniform scale
- X handle: X scale
- Y handle: Y scale
- Z handle: Z scale
- snap uses a scale snap step of 0.1
- scale is clamped above zero before returning to the editor

## Editor integration

The editor now passes position, rotation, and scale into `EditorTransformGizmoTarget`. A drag result applies only the transform channel owned by the active mode:

- Translate modifies `TransformComponent::position`.
- Rotate modifies `TransformComponent::rotation`.
- Scale modifies `TransformComponent::scale`.

After a gizmo edit, the editor still runs the existing safety path:

- mark transform dirty
- sanitize transform
- sync `WorldPositionComponent`
- mark scene dirty
- update status line

## Validation

`ak_editortransformgizmoprobe` now checks:

- translate surface handle count
- hit testing
- XZ plane drag
- snap drag
- Y-axis drag
- rotate drag with 15 degree snapping
- uniform scale drag with 0.1 snapping
- rotate/scale surface construction

Related probes:

- `ak_commandprobe`
- `ak_editormenutoolbarprobe`
- `ak_editortransformgizmoprobe`
