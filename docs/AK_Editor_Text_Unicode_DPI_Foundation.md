# AK Engine v8.2 — Editor Text / Unicode / DPI Foundation

This patch adds the first dedicated text layer for the AK Editor UI.

## Goals

- Keep UTF-8 as the internal editor string format.
- Convert to UTF-16/wide text at the Win32 boundary.
- Stop relying on ANSI `DrawTextA` for editor labels.
- Allow Cyrillic entity names, asset paths and project labels to render correctly.
- Introduce a DPI scale model before scroll, text editing and virtualized lists are added.
- Add codepoint-safe ellipsis helpers for long hierarchy, inspector and asset names.

## Added systems

- `EditorDpiScale`
- UTF-8 validation and decoding
- UTF-8 <-> wide string conversion
- codepoint counting
- end and middle ellipsis helpers
- approximate text measurement
- `EditorTextLayoutResult`
- `ak_editortextprobe`

## Win32 editor impact

The GDI editor text path now draws through wide text conversion instead of ANSI text calls. This is required before real text fields, localized names and Russian asset paths become reliable.

Window `WM_CHAR` input now appends UTF-8 text, including non-ASCII BMP characters and surrogate-pair input.

## Next steps

- Actual Win32 text measurement cache.
- Editor font cache.
- Text field editing state.
- Selection/caret support.
- IME composition support.
- DPI-aware full layout scaling.
