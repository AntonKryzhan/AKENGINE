# AK Engine v9.9 — Editor Command Palette / Global Search Runtime Foundation

This patch turns the existing command-palette/search foundation into a render-facing runtime model.

## Goals

- Keep command palette state independent from GDI/Vulkan/ImGui rendering.
- Build a stable search surface from commands, panels, entities and assets.
- Respect centralized command-state enable/disable rules.
- Show shortcut text from the editor keymap profile.
- Prevent disabled command activation while still showing why a result is disabled.
- Provide deterministic selection, scrolling and accept behavior.

## Added systems

- `EditorCommandPaletteState`
- `EditorCommandPaletteSurface`
- `EditorCommandPaletteRow`
- `EditorCommandPaletteInput`
- `EditorCommandPaletteResult`
- `ak_editorcommandpaletteprobe`

## Runtime behavior

The palette supports:

- open/close/toggle;
- query update/append/backspace/clear;
- selection move with clamping;
- scroll offset clamping;
- command accept through `CommandSource::CommandPalette`;
- disabled command rejection with reason text;
- shortcut display using `EditorShortcutProfile`.

This is the final foundation layer before drawing the command palette as a real overlay in `ak_editor.exe`.
