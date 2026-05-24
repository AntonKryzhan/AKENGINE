# AK Engine v8.5 — Editor Drag & Drop Foundation

This patch adds the editor drag/drop model used by Hierarchy, Scene View, Inspector, Project Browser and future dock tab workflows.

## Goals

- Keep drag state separate from selection, focus and popup state.
- Prevent stale raw pointers in drag payloads by storing stable ids/tokens only.
- Route drop actions through explicit operations and commands where possible.
- Support visual previews: insertion line, replace highlight, into-target and forbidden state.
- Keep hit-test/drop-target data deterministic and independent from rendering backend.

## Core types

- `EditorDragPayload`
- `EditorDragSource`
- `EditorDropTarget`
- `EditorDragDropState`
- `EditorDragPreview`
- `EditorDropResult`

## Supported foundation workflows

- Entity drag from Hierarchy to reorder or parent under root.
- Asset drag from Project Browser into Scene View to instantiate.
- Material-like asset drag into Inspector property slot.
- Component drag into Inspector component stack.
- Asset/file drag into Project folder.
- Dock tab payload/target foundation for future docking drag.

## Safety rules

- Drag starts as `Pending` and becomes `Dragging` only after threshold distance.
- Drops are accepted only if payload kind and target operation are compatible.
- Drop activation returns a result object instead of mutating scene data directly.
- Commands are mapped explicitly where an editor command already exists.

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_editordragdropprobe
.\build\windows-vs-debug\bin\Debug\ak_editordragdropprobe.exe
```
