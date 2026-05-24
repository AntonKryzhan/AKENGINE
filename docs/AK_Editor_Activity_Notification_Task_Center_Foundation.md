# AK Engine v9.5 — Editor Activity / Notification / Task Center Foundation

This patch adds the editor activity center foundation used to route transient notifications, sticky warnings/errors, and long-running background task progress through one deterministic model.

The activity center is intentionally independent from the current GDI rendering path. It can be drawn later as a status bar badge, toast stack, activity popover, progress center, or modal build/import panel.

## Goals

- Avoid writing autosave/import/render/build messages directly into unrelated UI code.
- Keep deduplicated notifications for repeated events like autosave or layout repair.
- Track warning/error counts for a future status indicator.
- Track background task progress for asset import, package build, shader compilation, and streaming jobs.
- Provide command-backed notification actions without raw callbacks.
- Expire normal notifications by TTL while preserving sticky errors.
- Enforce capacity limits so UI state cannot grow without bounds.

## Added concepts

```text
EditorActivityCenterState
EditorActivityCenterPolicy
EditorActivityNotification
EditorBackgroundTask
EditorActivityAction
EditorActivityTickResult
```

Notification severities:

```text
Trace
Info
Success
Warning
Error
```

Activity sources:

```text
Editor
Workspace
Autosave
AssetPipeline
Renderer
Streaming
Build
Diagnostics
```

Task phases:

```text
Queued
Running
Waiting
Completed
Failed
Cancelled
```

## Runtime behavior

Repeated events may be merged through a `dedupeKey` within the merge window. The notification repeat counter is incremented instead of adding a new row every frame.

Normal notifications expire through TTL. Sticky notifications stay until dismissed or explicitly cleared. Capacity pruning removes old entries when the notification list exceeds the configured maximum.

Background tasks can be started and then updated by id. Completed, failed, or cancelled tasks can post completion notifications.

## Future use

This layer is intended for:

- autosave status toasts;
- asset import progress;
- shader compilation progress;
- build/package progress;
- workspace repair warnings;
- renderer fallback warnings;
- async streaming diagnostics;
- a Unity-like lower-right activity indicator.
