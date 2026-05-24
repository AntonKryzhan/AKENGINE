# AK Engine v8.4 — Editor Input Capture / Focus Model Foundation

## Goal

This patch adds a dedicated editor focus/capture layer so UI input is no longer routed by ad-hoc `renameActive`, popup, viewport and panel checks only.

The model separates:

- hovered owner;
- focused owner;
- mouse capture;
- keyboard capture;
- scroll capture;
- drag capture;
- text edit capture;
- popup modal capture.

This prevents common editor bugs where viewport hotkeys trigger while typing, Escape closes the editor instead of a popup, wheel events scroll the wrong panel, or mouse drags continue after the owning widget changed.

## New module pieces

```text
engine/editorui/include/AK/EditorUI/EditorFocus.hpp
engine/editorui/include/AK/EditorUI/EditorFocusProbe.hpp
engine/editorui/src/EditorFocus.cpp
engine/editorui/src/EditorFocusProbe.cpp
tools/editorfocusprobe
```

## Runtime concepts

```text
EditorFocusOwner
EditorCaptureOwner
EditorTextEditCapture
EditorFocusState
EditorInputRoutingInput
EditorInputRoutingDecision
```

The routing decision answers whether the current input should go to:

```text
GlobalShortcut
TextEdit
Popup
Viewport
Panel
Blocked
```

and exposes explicit flags:

```text
allowGlobalShortcuts
allowTextInput
allowViewportInput
allowPanelScroll
closePopup
commitText
cancelText
```

## Editor integration

The current GDI editor now creates `EditorFocusState` and uses it to:

- select `InputContext::TextEntry` when text capture is active;
- block global shortcut dispatch while text capture/popup capture is active;
- start text capture when rename mode begins;
- commit/cancel text capture when rename commits/cancels;
- keep popup modal state out of normal viewport/panel input.

This is still a foundation layer. Real Inspector text fields and numeric widgets will build on it in later patches.

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_editorfocusprobe
.\build\windows-vs-debug\bin\Debug\ak_editorfocusprobe.exe
```

Expected:

```text
[ ok ] editor input capture / focus model foundation
```

## Next step

The natural follow-up is `v8.5 Editor Drag & Drop Foundation`, using this capture model for asset/entity/component drag payloads and drop targets.
