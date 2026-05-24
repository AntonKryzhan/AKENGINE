# AK Engine v9.1 — Editor Shortcut Manager / Keymap Preferences Foundation

This patch adds the first dedicated editor shortcut profile layer. It separates raw input bindings from user-editable editor keymaps, so future UI can expose a Unity-like Shortcut Manager without rewriting the low-level input system.

## Added

- `EditorShortcutProfile`
- `EditorShortcutBindingDesc`
- `EditorShortcutChord`
- shortcut contexts:
  - Global
  - Scene View
  - Hierarchy
  - Inspector
  - Project Browser
  - Console
  - Text Entry
- `AKEDITORSHORTCUTS 1` serialization format
- keymap repair/recovery policy
- duplicate action binding repair
- chord conflict detection
- unsafe text-entry shortcut blocking
- user override / disabled binding state
- conversion back into runtime `InputMap`
- `ak_editorshortcutprobe`

## Why this exists

The editor now has focus capture, popups, text input, drag/drop, command states, layout/session persistence, and real panel widgets. Hardcoded shortcuts would quickly become unsafe because the same key can mean different things depending on whether the user is editing text, navigating the Scene View, selecting Hierarchy rows, or invoking global commands.

The shortcut profile layer keeps those concerns separate:

```text
EditorShortcutProfile
  -> repair/validate conflicts
  -> user override storage
  -> context-aware shortcut table
  -> InputMap runtime rebuild
```

## Guarded bugs

- `Delete` firing while editing text.
- `WASD` camera movement stealing text-entry input.
- duplicate shortcuts in the same context.
- stale user keymap after command rename/add/remove.
- broken shortcut config forcing editor startup failure.
- unsafe text-entry bindings such as bare `N`, `W`, `S`.

## Next steps

Later patches can connect this to a visual Shortcut Manager panel:

```text
Edit -> Shortcuts...
Preferences -> Keymap
Search command
Rebind shortcut
Conflict warning
Reset binding
Export/import keymap
```
