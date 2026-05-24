# AK Engine v8.8 — Editor Theme / Skin / Visual Metrics Foundation

This patch introduces the first centralized editor theme layer for the GDI editor shell.

## Purpose

The editor UI previously had many visual constants scattered through panel/widget/rendering code: row heights, tab heights, colors, accent colors, text colors, button states and spacing. That makes visual polish fragile and causes inconsistent panel rhythm.

v8.8 adds `EditorTheme` as a single source of truth for:

- color roles;
- UI metrics;
- typography hints;
- widget skin conversion;
- DPI scaling hooks;
- contrast and metric validation;
- deterministic theme hashing.

## Added systems

- `EditorThemeProfile`
- `EditorThemeColorRole`
- `EditorThemeMetricRole`
- `EditorThemeMetrics`
- `EditorThemeTypography`
- `EditorTheme`
- `EditorThemeStateColors`
- `EditorThemeDiagnostics`
- `BuildDefaultEditorTheme()`
- `BuildHighContrastEditorTheme()`
- `BuildLightPreviewEditorTheme()`
- `ScaleEditorThemeForDpi()`
- `ValidateEditorTheme()`
- `HashEditorTheme()`
- `MakeEditorWidgetSkinFromTheme()`

## Why this matters

Future editor polish should no longer be done by changing random RGB values and dimensions inside renderer code. New UI code can depend on named roles such as:

- `PanelBackground`
- `TabActiveBackground`
- `SelectionBorder`
- `TextMuted`
- `AccentHovered`
- `GridMinor`
- `ViewportBackground`

Metrics now also have explicit roles:

- `MenuHeight`
- `ToolbarHeight`
- `TabHeight`
- `RowHeight`
- `FieldHeight`
- `InspectorLabelWidth`
- `AssetCellWidth`
- `PopupRowHeight`

## Probe

Run:

```powershell
cmake --build --preset windows-vs-debug --target ak_editorthemeprobe
.\build\windows-vs-debug\bin\Debug\ak_editorthemeprobe.exe
```

Expected result starts with:

```text
[ ok ] editor theme / skin / visual metrics foundation
```
