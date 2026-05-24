# AK Engine v2.4 — Asset Database Foundation

This patch adds the first deterministic asset database layer. The goal is to stop treating files as anonymous paths and start treating them as stable engine assets with GUIDs, fingerprints, importer identity and predictable cooked output paths.

## Problem fixed early

A large engine breaks if scenes reference raw paths directly:

- moving a file breaks scene references;
- repeated imports can produce inconsistent cooked names;
- asset hot reload cannot reliably compare source changes;
- packaging cannot know what must be cooked;
- importers cannot be diagnosed in a common way.

The engine now has a manifest format that maps source files to stable asset records.

## Core concepts

```text
source path -> normalized relative path -> AssetGuid
AssetGuid + kind + fingerprint -> cooked target path
manifest -> future cooker/package/streaming input
```

## Added API

```text
AssetKind
AssetCookStatus
AssetImportPolicy
AssetManifestRecord
AssetManifestStats
AssetManifest
BuildAssetManifest
WriteAssetManifestText
BuildAssetDatabaseProbe
```

## Manifest file

The first text manifest is written to:

```text
.akcache/assets/asset_manifest.akassetdb
```

The format starts with:

```text
AKASSETDB 1
```

It is intentionally simple and text-based for early debugging. A binary package database can be added later after the import/cook pipeline is stable.

## Determinism rules

- GUIDs are derived from normalized relative paths for now.
- Cooked paths are derived from GUID + asset kind.
- Source fingerprints track size, write time and optional content hash.
- Path risk and duplicate GUIDs are counted in manifest stats.

Later, `.akmeta` sidecar files can override generated GUIDs so that GUIDs survive file moves. The current layer prepares that transition without forcing it too early.

## Next steps

```text
v2.5 Importer Registry
  mesh/texture/material/shader importer descriptors
  import options
  dependency extraction

v2.6 Cook Cache
  source fingerprint -> cooked artifact validity
  recook only changed assets

v2.7 Package Foundation
  .akpak layout
  package table of contents
  asset residency metadata
```
