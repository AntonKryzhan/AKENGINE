# AK Engine Virtual File System Foundation

Version: v2.6

## Why this exists

The runtime must not directly depend on loose source folders. Development builds can read `assets/`, but player/shipping builds should read deterministic packages such as `.akpak`. A virtual file system gives both modes one API.

## Core rule

```text
AssetGuid / logical path
  -> VFS mount table
  -> package payload or loose file
  -> bytes returned to asset/runtime systems
```

## Problems fixed early

- runtime code no longer needs to know whether an asset comes from `assets/` or `.akpak`;
- package and loose development assets can be mounted side by side;
- duplicate GUIDs are diagnosed instead of silently shadowing data;
- payload hashes can be validated on read;
- future streaming and hot reload can use the same logical asset lookup layer.

## Current limits

This is still a foundation layer. It reads payload bytes synchronously and reparses packages on mount. Future patches should add async IO, chunk cache, residency budgets, compression blocks and package indices stored in binary form.
