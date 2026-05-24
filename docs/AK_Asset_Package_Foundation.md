# AK Engine v2.5 Asset Package Foundation

This layer introduces deterministic `.akpak` package construction on top of the asset manifest.

## Problem

A runtime must not depend on arbitrary loose source files forever. Loose files create fragile load order, slow startup scans, unstable deployment, unsafe package extraction paths, and hard-to-debug asset residency behavior.

## Current contract

`AKPAK 1` is a binary package with a UTF-8 table of contents followed by payload bytes.

The package builder uses:

- `AssetManifest` as input.
- stable `AssetGuid` identifiers.
- deterministic ordering.
- normalized source paths.
- bounded entry size.
- atomic output write.

The first implementation stores source bytes for cookable assets. Later importers can replace those payloads with cooked bytes without changing runtime package identity.

## Files

```text
engine/package/include/AK/Package/AssetPackage.hpp
engine/package/src/AssetPackage.cpp
tools/packageprobe/src/main.cpp
```

## Commands

```powershell
.\build\windows-vs-debug\bin\Debug\ak_packageprobe.exe
.\build\windows-vs-debug\bin\Debug\ak_assetc.exe D:\AKENGINE
```

`ak_assetc` now writes:

```text
.akcache/assets/asset_manifest.akassetdb
.akcache/packages/sandbox.akpak
```

## Invariants

- Runtime package entries are addressed by GUID, not by mutable filenames.
- The package table of contents is deterministic.
- Package writing is atomic through the filesystem layer.
- Empty packages are valid for empty/unsupported source trees.
- Oversized entries are skipped instead of forcing unbounded memory usage.
- Path-risk and duplicate-GUID diagnostics stay visible in package stats.

## Future direction

The next layers can build on this without changing the outer contract:

- cooked mesh/texture/material payloads;
- package compression blocks;
- package mount table;
- async package IO;
- asset residency budgets;
- streaming by world cell/chunk.
