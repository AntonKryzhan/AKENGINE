# AK Engine v10.4 — Editor Transform Space Runtime Foundation

v10.4 extends the Scene View transform tooling with explicit world/local transform spaces. The previous transform gizmo modes remain intact, but transform axes now carry a runtime space contract that can be selected through commands, menu entries, toolbar buttons, and command-state checked flags.

## Added

- `EditorTransformGizmoSpace` with `World` and `Local` modes.
- Local-space X/Z gizmo handle placement from the selected entity yaw.
- Local-space translation projection: axis drags are projected onto the selected transform local X/Z axes instead of always using world X/Z.
- Runtime status strings now include `Mode/Space`.
- Commands:
  - `ToolSpaceWorld`
  - `ToolSpaceLocal`
- Menu entries:
  - `Tools / Transform / World Space`
  - `Tools / Transform / Local Space`
- Toolbar entries:
  - `World`
  - `Local`
- Command-state checked flags for the active transform space.
- Probe coverage for local-space hit-testing and local-axis drag math.

## Behaviour

World space keeps the previous behaviour: X/Z handles move along global axes and the XZ center handle moves on the global XZ plane.

Local space keeps the center-plane free movement, but axis handles use the selected entity yaw:

```text
local X axis = yaw-rotated right vector
local Z axis = yaw-rotated forward vector
mouse drag  = projected onto selected local axis
result      = start position + projected local-axis delta
```

Scale and rotate modes keep their existing value semantics. Their handles are still built with the same local-space visual orientation so the editor does not silently mix world visual axes with local transform intent.

## Validation

`ak_editortransformgizmoprobe` now validates:

- visible handle build;
- world X/Z/Y drag;
- snap;
- rotate drag;
- scale drag;
- mode-specific surfaces;
- local-space hit-test;
- local-space projected drag.

This is still a GDI editor runtime foundation, not the final Vulkan/ImGui gizmo renderer. It is intentionally deterministic and independent from platform windows so later editor backends can reuse the same transform-space math.
