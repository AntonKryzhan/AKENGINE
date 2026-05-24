# AK Engine v8.6 — Editor Inspector Property Editing / Undo Transaction Foundation

This patch adds a dedicated inspector property editing layer for the editor UI.

## Goals

- keep text/numeric/Vec3 editing separate from raw panel drawing;
- support `begin -> preview -> commit -> cancel` edit workflow;
- generate one undo transaction per committed property edit;
- prevent live preview from spamming undo history;
- validate numeric input before writing it to the model;
- restore the old value on cancel;
- report scene dirty / proxy rebuild flags through a property change request.

## Added layer

```text
engine/editorui/include/AK/EditorUI/EditorPropertyEditing.hpp
engine/editorui/src/EditorPropertyEditing.cpp
```

Key types:

```text
EditorInspectorPropertyEditState
EditorPropertyEditPolicy
EditorPropertyValidationResult
EditorPropertyCommitResult
EditorUndoTransaction
EditorPropertyEditingDiagnostics
```

## Workflow

```text
BeginEditorInspectorPropertyEdit
        ↓
PreviewEditorInspectorPropertyText / PreviewEditorInspectorPropertyValue
        ↓
CommitEditorInspectorPropertyText / CommitEditorInspectorPropertyValue
        ↓
EditorPropertyChangeRequest + EditorUndoTransaction
```

Cancel path:

```text
Begin
  ↓
Preview
  ↓
CancelEditorInspectorPropertyEdit
  ↓
restore original property value
```

## Validation

The new layer validates:

- property exists;
- property is writable;
- value type matches;
- numeric fields parse correctly;
- numeric values are finite;
- Vec3/Color have the correct component count;
- optional range clamp policy.

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_editorpropertyprobe
.\build\windows-vs-debug\bin\Debug\ak_editorpropertyprobe.exe
```
