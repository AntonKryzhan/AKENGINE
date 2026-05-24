# AK Engine v7.8 — Editor Visual Shell / Unity-like GDI Skin Foundation

This patch improves the first layout-driven Win32/GDI editor shell pass from a technical schematic view into a more editor-like visual shell.

## Goals

- Keep the existing `EditorUI` runtime architecture intact.
- Do not migrate to Vulkan/ImGui yet.
- Make the current GDI bridge visually closer to a production editor layout.
- Reduce debug clutter in the title bar and viewport overlays.
- Improve panel chrome, tabs, toolbar rhythm, status bar, hierarchy/project search rows and inspector header.

## Changes

- Unity-style menu strip labels: File, Edit, Assets, GameObject, Component, Window, Help.
- Dark editor toolbar with grouped command buttons.
- Center play/pause/step controls when enough horizontal room exists.
- Search box placeholder on the toolbar.
- Panel stack frames around active dock areas.
- Cleaner active/inactive tabs and focused tab accent line.
- Cleaner hierarchy/project search rows.
- Inspector selected-object header band.
- Less intrusive viewport overlays.
- Transform tool overlay now draws a compact Q/W/E/R tool column instead of clipped text.
- Orientation gizmo now draws X/Y/Z axes instead of a plain rectangle.
- Frame stats overlay is hidden by default to avoid blocking the scene view.
- Window title is now compact: `AK Engine Editor v7.8 - <Scene>`.

## Notes

This is still the GDI bridge. It is not the final Vulkan/ImGui editor. The patch is intentionally visual-only and does not change editor command contracts, scene editing behavior, asset database, render graph, or runtime systems.
