# AK Engine v7.7 — Editor Shell Integration / Layout-Driven GDI Bridge

This patch is the first visible integration step after the v7.3-v7.6 editor UI foundations.
The old Win32/GDI editor shell is no longer forced to draw only a hardcoded six-rectangle layout.
It can now consume the `EditorRuntimeBridge` frame plan and render a layout-driven editor shell.

## Goals

- Keep the current working GDI editor executable stable.
- Start using the `engine/editorui` runtime layout in the real `ak_editor.exe`.
- Draw menu bar, toolbar, status bar, dock tabs, active panel bodies and splitters from runtime state.
- Keep the old hardcoded renderer as a fallback if the layout model is invalid.
- Prepare the same UI data path for the later Vulkan/ImGui editor shell.

## What changed

### Render shell descriptors

`AK::EditorFrameDesc` now has an optional layout-driven shell payload:

```cpp
bool useLayoutDrivenShell;
EditorShellLayoutDesc shellLayout;
```

The shell layout contains:

```cpp
menuBar
toolbar
dockSpace
statusBar
panels
splitters
toolbarButtons
overlays
statusLeft/statusRight
hoverLabel
```

The renderer still supports the old editor shell. If `useLayoutDrivenShell` is false or the payload is invalid, the previous hardcoded layout path remains active.

### Runtime layout integration

`ak_editor.exe` now creates an `EditorRuntimeBridge` at startup and resizes it when the window changes size.

Per frame it:

1. updates command enabled state from selection;
2. routes toolbar/tab/menu/status/splitter/overlay hit tests through `EditorRuntimeBridge`;
3. converts active runtime panel body rectangles into the legacy interaction layout;
4. builds `EditorShellLayoutDesc` for the renderer;
5. draws the GDI shell from runtime dock tabs and active panel bodies.

### Visible UI changes

The editor should now show a more engine-like shell:

- menu bar;
- layout-driven toolbar buttons;
- dock tab headers;
- active/focused panel borders;
- splitters;
- bottom status bar;
- Scene/Game tabs;
- Project/Console/RenderGraph tabs;
- Inspector/Diagnostics tabs;
- Hierarchy/World Partition tabs.

The content is still intentionally simple GDI rendering, but the position and active panel model now comes from EditorUI.

## Probe

New tool:

```powershell
cmake --build --preset windows-vs-debug --target ak_editorshellprobe
.\build\windows-vs-debug\bin\Debug\ak_editorshellprobe.exe
```

Expected summary:

```text
[ ok ] editor shell integration / layout-driven gdi bridge
```

## Why this matters

Before this patch, `engine/editorui` was mostly an architecture/probe layer.
After this patch, the real editor executable starts consuming that layer.
This is the bridge between the bootstrap GDI editor and the future Vulkan/ImGui docking editor.

## Follow-up

Good next patches:

```text
v7.8 Editor Dock Tab Interaction Hardening
v7.9 Inspector Property Editing UI
v8.0 Asset Browser Runtime Integration
v8.1 Scene View Overlay/Gizmo UI
v8.2 Vulkan/ImGui Editor Shell Bootstrap
```
