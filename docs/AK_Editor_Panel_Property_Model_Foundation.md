# AK Engine v7.5 Editor Panel / Property Model Foundation

This patch adds the editor-side data model layer that sits between the dock/runtime bridge and the current GDI shell.
It intentionally does not copy Unity UI code. It formalizes the same kind of editor concepts in AK Engine terms: inspector components, serialized properties, hierarchy nodes, asset browser entries, console records and diagnostics metrics.

## Goals

- Keep the current editor shell stable while extracting panel data contracts.
- Make Inspector, Hierarchy, Asset Browser, Console and Diagnostics independently testable.
- Prepare a clean bridge to a future Vulkan/ImGui editor without rewriting editor logic.
- Keep command/search/selection routing compatible with the v7.3/v7.4 editor UI layers.

## Added systems

- `EditorPropertyDesc` / `EditorPropertyValue`
- `EditorComponentInspector`
- `EditorInspectorModel`
- `EditorHierarchyModel`
- `EditorAssetBrowserModel`
- `EditorConsoleModel`
- `EditorDiagnosticsPanelModel`
- `EditorPanelModelFrame`
- `BuildPanelModelSearchIndex`
- `ak_editorpanelprobe`

## What this unlocks

The existing editor can keep drawing with GDI for now, but it no longer has to own the meaning of every panel directly.
A later shell can render the same model through ImGui, native Win32 controls, or a Vulkan UI layer.

The Inspector model is designed around explicit property descriptors instead of ad-hoc hardcoded text.
This prepares future reflection/schema-driven component editing without forcing a C#-style reflection system into C++.
