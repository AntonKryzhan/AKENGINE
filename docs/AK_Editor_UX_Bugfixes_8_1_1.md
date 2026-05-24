# AK Engine v8.1.1 — Editor UX Bugfixes

This patch fixes the first real UI regression pass after the v8.1 popup runtime integration.

## Fixed

- Hierarchy row hit-testing now matches the Unity-like visual tree rows.
  - Search/header and scene-root rows no longer offset entity selection.
  - Clicking an entity row selects the entity on that row instead of the entity below it.
- Scene View hit-testing now uses the actual rendered grid rectangle below the Scene toolbar.
  - Toolbar/overlay clicks are less likely to fall through into viewport entity picking.
- Scene View no longer renders duplicate overlay toolbar blocks over the built-in Scene toolbar.
  - The real Scene toolbar owns Scene/Snap/Camera/Shaded/Gizmos.
  - Floating overlays are reserved for transform tools, orientation gizmo, stats/debug overlays.
- Focused panel border no longer draws a full blue rectangle that fights with tab headers.
  - Focus is now indicated by the tab accent and a subtle left panel accent.
- Toolbar play controls are ASCII-only for the current GDI `DrawTextA` path.
  - This avoids mojibake on systems where the active code page does not match UTF-8.
- Popup check/submenu markers are ASCII-only for the current GDI text path.
- Toolbar search text is drawn once instead of twice.
- Command enabled-state updates no longer bump the editor runtime revision every frame when nothing changed.
  - This reduces visual jitter/flicker in toolbar command states such as Focus.

## Notes

This is still a GDI editor shell. A future Vulkan/ImGui shell can restore proper Unicode glyph rendering and richer iconography without relying on the Win32 ANSI text path.
