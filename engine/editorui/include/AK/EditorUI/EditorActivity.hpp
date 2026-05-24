#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorActivitySeverity
    {
        Trace,
        Info,
        Success,
        Warning,
        Error
    };

    enum class EditorActivitySource
    {
        Editor,
        Workspace,
        Autosave,
        AssetPipeline,
        Renderer,
        Streaming,
        Build,
        Diagnostics
    };

    enum class EditorActivityTaskPhase
    {
        Queued,
        Running,
        Waiting,
        Completed,
        Failed,
        Cancelled
    };

    enum class EditorActivityExpireReason
    {
        None,
        TimeToLive,
        Capacity,
        Dismissed
    };

    struct EditorActivityAction
    {
        std::string id;
        std::string label;
        CommandId command = CommandId::Count;
        bool primary = false;
        bool destructive = false;
        bool enabled = true;
    };

    struct EditorActivityNotification
    {
        u64 id = 0;
        EditorActivitySeverity severity = EditorActivitySeverity::Info;
        EditorActivitySource source = EditorActivitySource::Editor;
        std::string title;
        std::string message;
        std::string dedupeKey;
        std::vector<EditorActivityAction> actions;
        double createdSeconds = 0.0;
        double updatedSeconds = 0.0;
        double ttlSeconds = 6.0;
        u32 repeatCount = 1;
        bool sticky = false;
        bool read = false;
        bool dismissible = true;
        bool expired = false;
        EditorActivityExpireReason expireReason = EditorActivityExpireReason::None;
    };

    struct EditorBackgroundTask
    {
        u64 id = 0;
        EditorActivitySource source = EditorActivitySource::Editor;
        EditorActivityTaskPhase phase = EditorActivityTaskPhase::Queued;
        std::string label;
        std::string details;
        double progress = 0.0;
        double createdSeconds = 0.0;
        double updatedSeconds = 0.0;
        bool determinate = true;
        bool canCancel = false;
        bool visible = true;
        u64 completionNotificationId = 0;
    };

    struct EditorActivityPostDesc
    {
        EditorActivitySeverity severity = EditorActivitySeverity::Info;
        EditorActivitySource source = EditorActivitySource::Editor;
        std::string title;
        std::string message;
        std::string dedupeKey;
        std::vector<EditorActivityAction> actions;
        double ttlSeconds = 6.0;
        bool sticky = false;
        bool dismissible = true;
    };

    struct EditorBackgroundTaskUpdate
    {
        u64 id = 0;
        EditorActivitySource source = EditorActivitySource::Editor;
        EditorActivityTaskPhase phase = EditorActivityTaskPhase::Running;
        std::string label;
        std::string details;
        double progress = 0.0;
        bool determinate = true;
        bool canCancel = false;
        bool visible = true;
        bool postCompletionNotification = true;
    };

    struct EditorActivityCenterState
    {
        std::vector<EditorActivityNotification> notifications;
        std::vector<EditorBackgroundTask> tasks;
        u64 nextNotificationId = 1;
        u64 nextTaskId = 1;
        u64 revision = 1;
        u32 unreadCount = 0;
        u32 activeTaskCount = 0;
        u32 errorCount = 0;
        u32 warningCount = 0;
        EditorActivitySeverity highestSeverity = EditorActivitySeverity::Trace;
        std::string statusText;
    };

    struct EditorActivityCenterPolicy
    {
        usize maxNotifications = 64;
        usize maxTasks = 32;
        double defaultTtlSeconds = 6.0;
        double mergeWindowSeconds = 2.0;
        bool mergeDuplicates = true;
        bool autoExpireCompletedTasks = false;
    };

    struct EditorActivityTickInput
    {
        double nowSeconds = 0.0;
        double deltaSeconds = 0.0;
        bool editorFocused = true;
    };

    struct EditorActivityTickResult
    {
        u32 expiredNotifications = 0;
        u32 capacityPrunedNotifications = 0;
        u32 completedTasksHidden = 0;
        bool changed = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorActivityDiagnostics
    {
        EditorActivityCenterState state{};
        EditorActivityTickResult tick{};
        bool dedupeMerged = false;
        bool warningTracked = false;
        bool errorTracked = false;
        bool taskCompleted = false;
        bool capacityPruned = false;
        bool actionCommandBacked = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorActivitySeverity severity);
    const char* ToString(EditorActivitySource source);
    const char* ToString(EditorActivityTaskPhase phase);
    const char* ToString(EditorActivityExpireReason reason);

    EditorActivityCenterPolicy MakeDefaultEditorActivityCenterPolicy();
    EditorActivityCenterState MakeDefaultEditorActivityCenterState();

    u64 PostEditorActivityNotification(EditorActivityCenterState& state,
                                       const EditorActivityCenterPolicy& policy,
                                       const EditorActivityPostDesc& desc,
                                       double nowSeconds);

    u64 BeginEditorBackgroundTask(EditorActivityCenterState& state,
                                  const EditorActivityCenterPolicy& policy,
                                  EditorBackgroundTaskUpdate update,
                                  double nowSeconds);

    bool UpdateEditorBackgroundTask(EditorActivityCenterState& state,
                                    const EditorActivityCenterPolicy& policy,
                                    EditorBackgroundTaskUpdate update,
                                    double nowSeconds);

    bool DismissEditorActivityNotification(EditorActivityCenterState& state, u64 notificationId);
    u32 MarkEditorActivityNotificationsRead(EditorActivityCenterState& state);

    EditorActivityTickResult TickEditorActivityCenter(EditorActivityCenterState& state,
                                                      const EditorActivityCenterPolicy& policy,
                                                      const EditorActivityTickInput& input);

    void RecomputeEditorActivityCenter(EditorActivityCenterState& state);

    std::string FormatEditorActivityNotification(const EditorActivityNotification& notification);
    std::string FormatEditorBackgroundTask(const EditorBackgroundTask& task);
    std::string FormatEditorActivityCenterState(const EditorActivityCenterState& state);
    std::string FormatEditorActivityTickResult(const EditorActivityTickResult& result);

    EditorActivityDiagnostics RunEditorActivityDiagnostics();
    std::string BuildEditorActivityProbeSummary();
}
