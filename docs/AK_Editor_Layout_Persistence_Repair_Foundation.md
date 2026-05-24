# AK Engine v8.9 — Editor Layout Persistence / Repair Foundation

This patch adds a safe persistence layer around the editor docking layout. The goal is to prevent broken saved layouts from making the editor unusable after panel registry changes, monitor changes, DPI changes, or engine upgrades.

## Added

- `EditorLayoutPersistence` module.
- `AKEDITORLAYOUT 1` wrapper format over the existing `AKLAYOUT 1` dock data.
- Window placement persistence.
- Monitor/work-area clamping.
- Layout repair report with issue codes and severities.
- Repair path for:
  - invalid frame sizes;
  - invalid split weights;
  - unknown panel tabs;
  - duplicate panel tabs;
  - invalid active tab indices;
  - offscreen window placements.
- Reset-to-default fallback for unrecoverable dock graphs.
- Legacy raw `AKLAYOUT 1` loading support.
- Probe target: `ak_editorlayoutprobe`.

## Why this matters

Editor layouts are user data. A corrupted layout must never block the editor from launching. Old layouts must survive panel renames, removed panels, changed default proportions, monitor changes and future schema upgrades.

The safe policy is:

```text
load layout document
  -> parse wrapper or legacy AKLAYOUT
  -> deserialize dock layout
  -> repair soft problems
  -> validate diagnostics
  -> clamp window to monitor bounds
  -> reset to default only for unrecoverable graphs
```

This keeps the editor robust before real detachable windows, ImGui/Vulkan docking, multi-monitor workflows and user-customizable layouts are added.
