# AK Engine v3.0 — Serialization & Schema Foundation

This layer fixes a dangerous long-term engine problem: every persistent file must have a clear magic header, schema version, compatibility rule and migration path.

## Rules

- Every persistent format starts with `<MAGIC> <VERSION>`.
- The loader must decide whether the document is compatible, needs migration, too old or too new before parsing payload data.
- Old project files are loaded through explicit migration steps, not through accidental parser tolerance.
- Text serialization must be deterministic so diffs, cache keys and cooked outputs stay stable.
- Text values are quoted/escaped through one shared utility instead of each subsystem inventing its own parser.

## Current registered schemas

- `AKSCENE 6`
- `AKSETTINGS 1`
- `AKASSETDB 1`
- `AKPAK 1`

## Why this matters

Without a shared schema layer, scene loading, settings loading, asset manifests, package TOCs and future prefabs will drift into incompatible hand-written formats. That causes silent data loss, broken projects after upgrades and non-deterministic asset caches.

The v3.0 layer does not replace all existing parsers yet. It introduces the contract and probe-tested primitives that future scene/prefab/material/shader/importer formats will use.
