# AK Engine v8.1 — Editor UX Runtime / Popup Surface Foundation

This patch connects the v8.0 popup/context workflow model to the running editor shell.

## Goals

- Keep the new Unity-like editor UI model independent from Win32/GDI details.
- Add a runtime state for one active anchored popup/context menu.
- Convert `EditorUX` workflow descriptions into renderable popup surfaces.
- Support hit testing, hover state, item activation, escape/outside-click closing, and command routing.
- Begin integrating real context workflows into `ak_editor.exe` without replacing the current GDI shell.

## Runtime Flow

```text
EditorUX workflow
  -> EditorUXRuntime
  -> EditorUXPopupSurface
  -> EditorFrameDesc.popupSurfaces
  -> Renderer GDI popup pass
```

## Integrated editor actions

- Right click Hierarchy: opens hierarchy context menu.
- Right click Project Browser: opens project context menu.
- Right click Inspector: opens Add Component menu.
- Left click popup item: activates item and routes command when available.
- Left click outside popup: closes popup.
- Escape: closes popup before closing the editor.

## Why this exists before ImGui/Vulkan

The goal is to validate editor behavior and UX contracts before swapping the renderer shell. The future Vulkan/ImGui editor can reuse the same `EditorUXRuntime` state and replace only the drawing backend.
