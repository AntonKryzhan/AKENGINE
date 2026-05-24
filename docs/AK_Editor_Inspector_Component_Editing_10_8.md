# AK Engine v10.8 - Inspector Component Editing Expansion

This patch expands the backend-neutral Inspector property model so editor shells can present more real component fields without depending on the current Win32/GDI drawing code.

## Added Inspector coverage

The default panel model now exposes typed component inspectors for:

- `TransformComponent`
- `WorldPositionComponent`
- `CameraComponent`
- `LightComponent`
- `MeshComponent`
- `BoundsComponent`
- `DestructibleComponent`

The fields remain editor-model descriptors. They do not mutate ECS storage directly until a scene bridge consumes the existing property change requests.

## Numeric metadata

`EditorPropertyDesc` now carries optional numeric step metadata next to the existing min/max range fields:

```text
stepValue
hasStep
```

Ranges and steps are populated for transform, world position, camera, light, bounds, and destructible settings where editor widgets need deterministic clamping and increment controls.

## Validation and probe coverage

The existing `ak_editorpropertyprobe` now covers:

- preview edits do not create undo transactions;
- cancel restores the original property value;
- camera FOV values clamp through property range metadata;
- mesh material edits request a proxy rebuild;
- committed edits produce one undo transaction per accepted change.

The existing panel diagnostics also count ranged, stepped, and read-only properties.
