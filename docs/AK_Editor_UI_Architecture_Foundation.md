# AK Engine v7.3 — Editor UI Architecture Foundation

This patch adds the first dedicated editor UI architecture layer. It is intentionally independent from the current GDI editor drawing code and from the future Vulkan/ImGui shell. The goal is to make editor behavior, layout, commands, search and panels stable before replacing the visual backend.

## Why this exists

The early editor was useful as a bootstrap shell, but fixed rectangles in `editor/src/main.cpp` do not scale into a real engine editor. A production editor needs a persistent layout tree, docked tab stacks, panel descriptors, command routing, menu/toolbar models, selection state, overlay descriptors and a search/command-palette service.

The model follows the strong architectural ideas seen in large editors:

```text
EditorShell
  -> Frame layout
    -> Dock tree
      -> Split nodes
      -> Tab stacks
        -> Editor panels
```

No Unity source code is copied. This is a clean C++ AK Engine model designed for the existing command/input layers and the future Vulkan/ImGui editor shell.

## Added module

```text
engine/editorui
```

Main systems:

```text
EditorPanelRegistry
EditorDockLayout
EditorFrameLayout
EditorMenuModel
EditorToolbarModel
EditorStatusBarModel
EditorOverlayDesc
EditorSelectionModel
EditorSearchIndex
AKLAYOUT 1 serializer
```

## Default editor layout

The default layout is now represented as data:

```text
Menu Bar
Toolbar
Dock Space
  Left stack:
    Hierarchy
    World Partition
  Center stack:
    Scene
    Game
  Right stack:
    Inspector
    Diagnostics
  Bottom stack:
    Project
    Console
    RenderGraph
Status Bar
```

The dock tree supports:

```text
split nodes
stack nodes
tab lists
active tabs
panel placements
computed panel rectangles
layout validation
text serialization/deserialization
```

## Panel model

The default panel registry includes:

```text
Scene Hierarchy
Scene Viewport
Game Viewport
Inspector
Asset Browser
Console
Diagnostics
RenderGraph Debugger
World Partition
Terrain Editor
Material Editor
Physics Debugger
Project Settings
Command Palette
```

Each panel has capabilities:

```text
dockable
closable
singleton
focusable
searchable
requires scene
requires selection
heavy diagnostics
```

This allows the editor shell to decide what can be docked, searched, closed, shown in command palette or disabled when no scene/selection exists.

## Command routing

The new chrome layer builds menu and toolbar models from the existing `AK::CommandRegistry`.

One command can be exposed through multiple surfaces:

```text
shortcut
menu item
toolbar button
command palette
programmatic invocation
```

This keeps editor actions centralized and avoids hardcoding `Ctrl+S`, toolbar `Save`, menu `Save Scene` and command-palette `Save Scene` as separate behaviors.

## Viewport overlays

The viewport overlay descriptors cover:

```text
Scene toolbar
Transform tools
Grid / Snap
Camera mode
Orientation gizmo
Frame stats
Selection outline
World Partition cells
Physics debug
Render mode selector
```

These are only data descriptors in v7.3. Rendering them is left to the current GDI bridge and later Vulkan/ImGui shell.

## Search foundation

The search index can combine:

```text
commands
panels
assets
scene entities
settings later
overlays later
```

This prepares a real command palette and searchable editor windows.

## Layout persistence

The layout serializer writes a debug-friendly text format:

```text
AKLAYOUT 1
frame ...
node ...
tab ...
```

The probe performs a round-trip save/load validation. Later this can move behind the common serialization/schema system.

## Probe

New tool:

```text
ak_editoruiprobe.exe
```

It validates:

```text
panel registry
dock tree
menu model
toolbar model
status bar model
overlay descriptors
selection model
search index
AKLAYOUT serialization round-trip
```

## Next steps

```text
v7.4 Editor Docking Runtime / GDI Bridge
  use EditorFrameLayout to drive the existing editor rectangles
  map current panels to EditorPanelId
  persist .aklayout under project/editor state

v7.5 Vulkan + ImGui Editor Shell
  replace GDI drawing backend
  keep the v7.3 editor data model

v7.6 Scene View Overlays / Gizmos
  draw overlay descriptors in viewport
  transform gizmo, orientation cube, grid/snap UI

v7.7 Inspector Property System
  component property descriptors
  typed fields, dirty state, undo integration

v7.8 Asset Browser Rewrite
  asset tree, filters, icons, import status, package/cook diagnostics

v7.9 Command Palette / Search UI
  search provider UI over the v7.3 index
```
