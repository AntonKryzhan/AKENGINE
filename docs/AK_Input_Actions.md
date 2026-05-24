# AK Engine Input Actions

AK Engine v1.8 introduces a small action layer between raw window events and editor commands.

## Problem fixed

Earlier editor code read `WindowInput` hotkeys directly from `main.cpp`. That works for a prototype, but it creates future limits:

- editor shortcuts become scattered across UI/gameplay code;
- text-entry modes can accidentally trigger destructive commands;
- runtime controls, editor shortcuts and future gamepad bindings have no common contract;
- rebinding keys later requires editing behavior code instead of a binding table;
- command conflicts are hard to diagnose.

## Current architecture

```text
Win32/stub WindowInput
    ↓
AK::InputMap
    ↓
InputActionSnapshot
    ↓
Editor command execution
```

The editor now evaluates actions through an explicit context:

```text
InputContext::Editor
InputContext::TextEntry
```

This means rename/text-entry mode only allows text-safe actions such as `Cancel`, `Confirm` and `RenameBackspace`.

## Default editor actions

Important bindings:

```text
Ctrl+S      SaveScene
Ctrl+L      LoadScene
Ctrl+Z      Undo
Ctrl+Y      Redo
Ctrl+D      DuplicateSelection
Delete      DeleteSelection
N           NewEntity
R           RescanAssets
F           FocusSelection
Home        ResetViewport
G           ToggleGridSnap
F2          RenameSelection
W/A/S/D     Move selected entity in X/Z
Q/E         Move selected entity in Y
Arrow keys  select/rotate depending on command group
+/-         uniform scale
```

## Why this matters before Vulkan/physics

Input is a cross-cutting system. If shortcuts stay hardwired inside editor logic, later systems will fight each other: editor viewport, runtime player controls, gizmo controls, text fields, prefab editor, material editor, console and gamepad input. The action layer makes input context explicit before those systems exist.

## Next steps

- load/save bindings from a project/user config file;
- add mouse-button and analog-axis actions;
- add command registry with display names and enabled/disabled states;
- route editor toolbar buttons through the same command layer;
- add game runtime input maps separate from editor input maps.
