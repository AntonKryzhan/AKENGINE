# AK Engine v1.2 — Engine Invariants

Status: v1.2 foundation.

This layer removes several early architecture traps before the renderer, physics, asset pipeline and editor become large.

## Generational EntityId

Runtime entities are no longer plain integers. An entity handle is now:

```text
index:generation
```

When an entity is destroyed, its generation is advanced. A stale handle from undo/redo, selection, scripts, future physics proxies or render proxies can no longer silently point to a different entity after slot reuse.

This prevents:

```text
stale selection targeting a new object
old component reference mutating a reused entity
save/load reference confusion
render/physics proxy updating a destroyed entity
```

## AssetGuid

Asset records now use a stable 128-bit-style `AssetGuid` generated from normalized project-relative paths. The current implementation is deterministic and path-derived; later `.akmeta` files can persist generated GUIDs without changing the public API.

This is the bridge away from direct asset path references. Scenes and cooked packages should eventually store GUIDs, not fragile paths.

## Path normalization

Core path helpers now normalize paths before hashing and file operations. The engine uses generic slash-separated normalized strings for GUID generation. This avoids Windows-specific issues with slashes, case and relative path spellings.

## Atomic scene save

Scene saving now writes to a temporary file first and then replaces the target. This avoids corrupted `.akscene` files when the editor crashes, the process is killed, or the system loses power during a save.

```text
scene.akscene.tmp -> flush -> rename -> scene.akscene
```

## Result/Error

Core now has a small `Result<T>` / `Result<void>` layer. Hot runtime code can avoid exceptions while still returning structured errors from IO, parsing, asset cooking and future renderer initialization code.

## Generic Handle<T>

`Handle<Tag>` provides the same index/generation pattern for future resources:

```text
TextureHandle
MeshHandle
MaterialHandle
ShaderHandle
GpuBufferHandle
```

This is required before asset hot reload and deferred GPU destruction.

## Probe

`ak_invariantprobe.exe` validates:

```text
stale entity handles become invalid
entity slot reuse advances generation
asset GUID normalization is stable
AssetGuid string roundtrip works
scene save/load uses atomic write path
Result<T> works
```
