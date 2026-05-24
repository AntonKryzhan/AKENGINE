# AK Engine v8.7 — Editor Command State / Tooltip / Modal Foundation

This patch adds the editor-side command state layer that prevents toolbar/menu/shortcut state from being recalculated in ad-hoc places.

## Goals

- Centralize `enabled / checked / visible / tooltip / disabled reason` for every editor command.
- Prevent unsafe commands while text fields, popups, or modal dialogs own input.
- Keep selection-dependent commands stable without frame-to-frame flicker.
- Add tooltip timing and surface placement foundation.
- Add modal dialog stack foundation for delete/save/error workflows.

## Added systems

- `EditorCommandContext`
- `EditorCommandStateCache`
- `EditorCommandStateEntry`
- `EditorTooltipState`
- `EditorModalDialog`
- `EditorModalStack`
- `ak_editorcommandstateprobe`

## Immediate use

The layer is safe to connect incrementally to the current GDI editor shell:

1. Build a command context from selection/focus/undo/popup/modal state.
2. Build `EditorCommandStateCache` once when the context hash changes.
3. Apply it to toolbar/menu models.
4. Route shortcuts only if command state is enabled.
5. Show disabled reasons as tooltips/status messages.

## Why this matters

Without a command-state cache, UI bugs appear as flickering buttons, stale context menu actions, shortcuts firing while a text field is active, and destructive actions being available during modal workflows.
