# AK Engine v1.0 — Scene Components Foundation

This stage turns the early entity editor into a component-aware scene foundation.

## Added runtime components

- `TransformComponent` remains the spatial base for all editable scene entities.
- `MeshComponent` stores the mesh and material asset identifiers for renderable objects.
- `CameraComponent` stores vertical FOV, near/far planes, and primary-camera intent.
- `LightComponent` stores light type, intensity, color, range, and spot cone metadata.

The first implementation deliberately keeps component storage simple and deterministic. It uses typed component maps inside `AK::World`, which is enough for editor work, scene persistence, and runtime preview before the later archetype/chunk ECS pass.

## Scene format

`AKSCENE 3` adds component records after entity transform records:

```text
entity <file-id> "Name" px py pz rx ry rz sx sy sz
camera <file-id> fov near far primary
light <file-id> type intensity r g b range innerCone outerCone
mesh <file-id> "mesh-id" "material-id"
```

Loading still accepts `AKSCENE 1` and `AKSCENE 2`. File entity ids are remapped to runtime ids on load, so saved component lines stay attached to the correct entity.

## Editor integration

The editor can now create component-specific entities from the toolbar:

- `Mesh` creates a transform plus `MeshComponent`.
- `Camera` creates a transform plus `CameraComponent`.
- `Light` creates a transform plus `LightComponent`.

Scene hierarchy rows show compact component tags:

- `[M]` mesh
- `[C]` camera
- `[L]` light
- `[E]` plain entity

Viewport markers also use component kind tags, making the temporary GDI editor more useful before Vulkan/ImGui docking is introduced.

## Validation

`ak_ecsprobe.exe` validates the basic component pipeline:

- entity creation
- typed component counts
- save to `AKSCENE 3`
- load/roundtrip of mesh, camera, and light components
