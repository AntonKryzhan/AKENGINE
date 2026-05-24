# AK Engine Command Registry

AK Engine v1.9 adds a command registry above the input action layer.

The command layer is intentionally separate from raw keyboard and mouse input:

```text
WindowInput -> InputMap/InputAction -> CommandRegistry/CommandId -> editor operation
```

This prevents a common editor architecture bug: toolbar buttons, shortcuts, menus and future command palette entries calling different pieces of code for the same action.

## What the registry owns

- Stable command ids.
- Human-readable command names.
- Categories for future menus and command palette grouping.
- Shortcut action metadata.
- Toolbar visibility metadata.
- Destructive command metadata.
- Selection requirement metadata.

## Current editor command set

- Close editor.
- New entity / mesh / camera / light.
- Delete selection.
- Save / load scene.
- Rescan assets.
- Undo / redo.
- Duplicate selection.
- Focus selection.
- Reset viewport.
- Toggle grid snap.
- Select previous / next.
- Rename selection.

## Future use

The same command registry can drive:

- Menu bar.
- Command palette.
- Shortcut remapping UI.
- Toolbar customization.
- Undo-aware command routing.
- Editor automation scripts.

The current implementation keeps the registry lightweight. Commands are metadata plus dispatch routing in the editor. Later versions can move command execution into typed command handlers when the editor is split into panels and services.
