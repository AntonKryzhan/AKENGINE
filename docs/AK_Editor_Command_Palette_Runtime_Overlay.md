# AK Engine v10.0 — Editor Command Palette Runtime / Overlay Integration

This patch turns the command palette from a search/runtime foundation into a visible editor overlay.

## Goals

- `Ctrl+K` or toolbar search opens the command palette.
- The palette captures keyboard and mouse while open.
- Text input updates the search query.
- Up/Down changes selection.
- Enter accepts the selected result.
- Escape and outside click close the palette.
- Commands dispatch through `CommandSource::CommandPalette`.
- Panel/entity/asset results are handled without raw pointers.

## Layers

- `EditorCommandPalette` builds result rows from `EditorSearchIndex`.
- `EditorCommandPaletteRuntime` owns capture, hit-test and interaction state.
- `Renderer` draws the command palette as the top-most GDI overlay.
- `ak_editor` feeds live commands, panels, entities and assets into the search index.

## Safety

The runtime uses generation-safe `EntityId`, `EditorPanelId`, stable IDs and command-state validation. Disabled commands remain visible but cannot be activated.

## Known scope

This is still a Win32/GDI editor shell integration. The same state/surface model is intended to survive the future Vulkan/ImGui editor shell.
