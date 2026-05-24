# AK Engine v9.0 — Editor Session State Persistence / Recovery Foundation

This patch adds the editor session layer that lives above the repaired dock layout.

The layout system stores where panels are placed. The session system stores what the editor was doing:

- active/focused panel identity;
- per-panel scroll offsets;
- selection targets;
- hierarchy/project expansion state;
- inspector foldout state;
- hierarchy/project search queries;
- current project folder;
- safe command palette query state;
- transient-state cleanup after restart.

## Why this exists

A production editor must survive restarts and crashes without restoring unsafe UI state. It is not enough to persist window rectangles and dock nodes. The editor also needs a validated session snapshot so that selected objects, expanded trees, scroll offsets and active tabs return safely.

## Design rules

- Store stable panel names in addition to runtime panel ids.
- Store selections as stable ids, not raw pointers.
- Repair unknown panels and deleted selection targets.
- Clamp invalid scroll offsets.
- Drop transient UI state on load: popup, modal, text edit and drag state are not restart-safe.
- Keep command palette query text, but do not reopen the palette by default.
- Use a deterministic text format: `AKEDITORSESSION 1`.

## Main APIs

```cpp
EditorSessionState session = CaptureEditorSessionState(input, panels);
std::string text = SerializeEditorSessionState(session);
EditorSessionLoadResult loaded = LoadEditorSessionWithRepair(text, panels, validStableIds);
RestoreEditorSessionActiveTabs(frame, loaded.session, panels);
std::vector<EditorScrollState> scrolls = RestoreEditorSessionScrollStates(loaded.session);
EditorSelectionModel selection = RestoreEditorSessionSelection(loaded.session);
```

## Validation and repair

The repair layer handles:

- unknown panel session entries;
- duplicate panel session entries;
- invalid focused panel;
- duplicate selection targets;
- unknown selection targets;
- invalid scroll offsets;
- invalid expansion keys;
- invalid foldout keys;
- transient popup/modal/text/drag state;
- reset-to-default fallback if no valid panels remain.

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_editorsessionprobe
.\build\windows-vs-debug\bin\Debug\ak_editorsessionprobe.exe
```
