# AK Engine v10.1 — Editor Menu / Toolbar Runtime Integration

This patch turns the top editor chrome into a runtime-interactive surface instead of a purely rendered shell.

## Goals

- Route main-menu clicks through a dedicated runtime model.
- Route toolbar button clicks through command state and command invocations.
- Keep command palette, context popup and menu popup input separated.
- Reuse the existing popup renderer for main-menu dropdowns.
- Preserve the current GDI shell while making the UI behavior closer to a real editor.

## Added module

```text
engine/editorui/include/AK/EditorUI/EditorMenuToolbarRuntime.hpp
engine/editorui/src/EditorMenuToolbarRuntime.cpp
```

The module provides:

- top-level menu root surfaces;
- menu dropdown surfaces;
- toolbar command button surfaces;
- toolbar search-box hit testing;
- command-state-aware enable/disable routing;
- shortcut labels from the editor keymap profile;
- menu/toolbar command invocations;
- mouse/keyboard capture while a menu popup is open.

## Runtime behavior

```text
menu root click
  -> opens menu dropdown
  -> dropdown rows use CommandId + CommandSource::Menu
  -> disabled rows remain visible but do not invoke commands

main toolbar click
  -> hits runtime button rect
  -> checks EditorCommandStateCache
  -> emits CommandSource::Toolbar invocation

search box click
  -> opens command palette runtime
```

## Probe

```text
ak_editormenutoolbarprobe
```

Checks:

- root menu hit-test;
- popup command invocation;
- toolbar command hit-test;
- toolbar search-box hit-test;
- shortcut labels in menu rows;
- surface validation.

## Notes

This is still GDI rendering. The important part is that the menu and toolbar now have a real runtime interaction model and can later be rendered by ImGui/Vulkan without changing the command routing contract.
