# AK Engine v7.6 — Editor Interaction / Command Palette Workflow Foundation

This patch adds the first editor interaction workflow layer on top of the v7.3-v7.5 editor UI architecture.

The goal is to keep the current GDI editor shell compatible while moving editor behavior out of ad-hoc UI code and into a deterministic, command-oriented model that can later be driven by Vulkan/ImGui.

## Added systems

- `EditorInteractionContext`
- `EditorInteractionEvent`
- `EditorInteractionFrameResult`
- `EditorCommandPaletteState`
- `EditorPropertyEditState`
- `EditorPropertyChangeRequest`
- `EditorSelectionChangeRequest`
- `EditorInteractionDiagnostics`

## Workflow foundations

### Command palette

The command palette queries the combined editor search index from:

- commands
- panels
- scene entities
- assets
- inspector properties
- diagnostics metrics

Accepted results are converted into stable actions:

- command result -> `CommandInvocation`
- entity result -> selection request
- asset result -> asset selection request
- panel result -> focused panel
- property result -> property edit begin

### Selection requests

Hierarchy and asset selection are expressed as explicit before/after requests:

```text
before selection snapshot
    -> user/search/command reason
    -> after selection snapshot
    -> undoable flag
```

This is intentionally compatible with a future undo/redo command stack.

### Property changes

Inspector edits do not directly mutate runtime scene data. They produce a structured change request:

```text
property path
old value
new value
accepted/rejected status
scene dirty flag
proxy rebuild flag
undoable flag
```

The current implementation updates the editor panel model only. The future scene bridge can consume these requests and apply them to ECS/components through transaction-safe commands.

### Overlay and panel routing

Overlay toggles and panel focus are routed through the runtime bridge instead of being hardcoded into drawing code.

## Probe

`ak_editorinteractionprobe` validates:

- hierarchy selection request
- property begin/commit request
- command palette query and command accept
- overlay toggle through the interaction event path
- interaction diagnostics

Expected success line:

```text
[ ok ] editor interaction / command palette workflow foundation
```

## Why this matters

This layer is the bridge between static editor models and real user workflows:

```text
input / pointer / search / toolbar / menu
        ↓
EditorInteractionEvent
        ↓
selection/property/command/layout requests
        ↓
future undo stack / scene transaction / renderer update
```

It prevents the editor from becoming a monolithic UI file and prepares AK Engine for a proper Unity-like editor structure without copying Unity internals.
