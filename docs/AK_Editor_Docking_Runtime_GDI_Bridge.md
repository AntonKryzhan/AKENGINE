# AK Engine v7.4 — Editor Docking Runtime / GDI Bridge Foundation

This patch turns the v7.3 editor UI architecture model into a runtime-facing frame plan that the current Win32/GDI editor shell can consume before the Vulkan/ImGui editor rewrite.

## Added

- `EditorRuntimeBridge`
- runtime panel states from dock placements
- tab/body hit testing
- splitter handle generation
- toolbar command hit routing
- focused panel state
- overlay visibility toggles
- command enabled/disabled routing
- runtime layout serialization wrapper over `AKLAYOUT 1`
- `ak_editorruntimeprobe`

## Design goal

The old editor shell can keep drawing with GDI for now, but panel ownership, focus, tabs, command routes and overlay descriptors are now represented by engine-side data instead of being hardcoded only in `editor/src/main.cpp`.

This avoids coupling editor logic to the temporary rendering backend. Later patches can connect this runtime model to:

- GDI bridge drawing
- Vulkan/ImGui docking
- command palette
- inspector property system
- asset browser rewrite
- Scene/Game viewport tabs

## Runtime flow

```text
EditorPanelRegistry
    ↓
EditorFrameLayout / Dock tree
    ↓
EditorRuntimeFramePlan
    ↓
PanelRuntimeState + SplitterHandles + CommandRoutes + Overlays
    ↓
GDI shell now / Vulkan+ImGui shell later
```

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_editorruntimeprobe
.\build\windows-vs-debug\bin\Debug\ak_editorruntimeprobe.exe
```

Expected output begins with:

```text
[ ok ] editor docking runtime / gdi bridge foundation
```
