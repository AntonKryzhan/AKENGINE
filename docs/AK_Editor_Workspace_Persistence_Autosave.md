# AK Engine v9.3 — Editor Workspace Persistence / Autosave Runtime Foundation

This layer ties the editor preference, shortcut, layout and session formats into a single workspace runtime.

## Goals

- Resolve editor cache paths from the active project root.
- Create `.akcache/editor`, autosave and backup directories safely.
- Load preferences, shortcut profile, dock layout and session state with repair/default fallback.
- Save editor state through atomic text writes.
- Keep transient UI state out of startup recovery.
- Prepare autosave/crash-recovery integration without hard-wiring it into the render loop.

## Files

- `preferences.akeditorprefs`
- `shortcuts.akeditorshortcuts`
- `layout.akeditorlayout`
- `session.akeditorsession`
- `autosaves/`
- `backups/`

## Contract

The editor must be able to start even if every workspace file is missing or corrupted. Missing files use defaults; corrupted files are repaired by the underlying schema-specific repair layers.
