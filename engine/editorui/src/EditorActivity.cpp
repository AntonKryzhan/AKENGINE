#include <AK/EditorUI/EditorActivity.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace AK
{
    namespace
    {
        bool IsActiveTaskPhase(EditorActivityTaskPhase phase)
        {
            return phase == EditorActivityTaskPhase::Queued
                || phase == EditorActivityTaskPhase::Running
                || phase == EditorActivityTaskPhase::Waiting;
        }

        bool IsCompletedTaskPhase(EditorActivityTaskPhase phase)
        {
            return phase == EditorActivityTaskPhase::Completed
                || phase == EditorActivityTaskPhase::Failed
                || phase == EditorActivityTaskPhase::Cancelled;
        }

        EditorActivitySeverity MaxSeverity(EditorActivitySeverity a, EditorActivitySeverity b)
        {
            return static_cast<int>(a) >= static_cast<int>(b) ? a : b;
        }

        double Clamp01(double value)
        {
            if (!std::isfinite(value) || value < 0.0)
            {
                return 0.0;
            }
            if (value > 1.0)
            {
                return 1.0;
            }
            return value;
        }

        bool IsEmpty(std::string_view value)
        {
            return value.empty();
        }

        std::string BuildDedupeKey(const EditorActivityPostDesc& desc)
        {
            if (!desc.dedupeKey.empty())
            {
                return desc.dedupeKey;
            }
            std::ostringstream out;
            out << ToString(desc.source) << ':' << ToString(desc.severity) << ':' << desc.title << ':' << desc.message;
            return out.str();
        }

        EditorActivityPostDesc CompletionNotificationForTask(const EditorBackgroundTask& task)
        {
            EditorActivityPostDesc desc{};
            desc.source = task.source;
            desc.dedupeKey = "task-complete:" + std::to_string(task.id);
            desc.dismissible = true;
            desc.ttlSeconds = 5.0;

            switch (task.phase)
            {
                case EditorActivityTaskPhase::Completed:
                    desc.severity = EditorActivitySeverity::Success;
                    desc.title = task.label.empty() ? "Task completed" : task.label;
                    desc.message = task.details.empty() ? "Background task completed." : task.details;
                    break;
                case EditorActivityTaskPhase::Failed:
                    desc.severity = EditorActivitySeverity::Error;
                    desc.title = task.label.empty() ? "Task failed" : task.label;
                    desc.message = task.details.empty() ? "Background task failed." : task.details;
                    desc.sticky = true;
                    break;
                case EditorActivityTaskPhase::Cancelled:
                    desc.severity = EditorActivitySeverity::Warning;
                    desc.title = task.label.empty() ? "Task cancelled" : task.label;
                    desc.message = task.details.empty() ? "Background task was cancelled." : task.details;
                    break;
                default:
                    desc.severity = EditorActivitySeverity::Info;
                    desc.title = task.label.empty() ? "Task updated" : task.label;
                    desc.message = task.details;
                    break;
            }
            return desc;
        }
    }

    const char* ToString(EditorActivitySeverity severity)
    {
        switch (severity)
        {
            case EditorActivitySeverity::Trace: return "trace";
            case EditorActivitySeverity::Info: return "info";
            case EditorActivitySeverity::Success: return "success";
            case EditorActivitySeverity::Warning: return "warning";
            case EditorActivitySeverity::Error: return "error";
            default: return "unknown";
        }
    }

    const char* ToString(EditorActivitySource source)
    {
        switch (source)
        {
            case EditorActivitySource::Editor: return "editor";
            case EditorActivitySource::Workspace: return "workspace";
            case EditorActivitySource::Autosave: return "autosave";
            case EditorActivitySource::AssetPipeline: return "asset-pipeline";
            case EditorActivitySource::Renderer: return "renderer";
            case EditorActivitySource::Streaming: return "streaming";
            case EditorActivitySource::Build: return "build";
            case EditorActivitySource::Diagnostics: return "diagnostics";
            default: return "unknown";
        }
    }

    const char* ToString(EditorActivityTaskPhase phase)
    {
        switch (phase)
        {
            case EditorActivityTaskPhase::Queued: return "queued";
            case EditorActivityTaskPhase::Running: return "running";
            case EditorActivityTaskPhase::Waiting: return "waiting";
            case EditorActivityTaskPhase::Completed: return "completed";
            case EditorActivityTaskPhase::Failed: return "failed";
            case EditorActivityTaskPhase::Cancelled: return "cancelled";
            default: return "unknown";
        }
    }

    const char* ToString(EditorActivityExpireReason reason)
    {
        switch (reason)
        {
            case EditorActivityExpireReason::None: return "none";
            case EditorActivityExpireReason::TimeToLive: return "ttl";
            case EditorActivityExpireReason::Capacity: return "capacity";
            case EditorActivityExpireReason::Dismissed: return "dismissed";
            default: return "unknown";
        }
    }

    EditorActivityCenterPolicy MakeDefaultEditorActivityCenterPolicy()
    {
        return {};
    }

    EditorActivityCenterState MakeDefaultEditorActivityCenterState()
    {
        EditorActivityCenterState state{};
        RecomputeEditorActivityCenter(state);
        return state;
    }

    u64 PostEditorActivityNotification(EditorActivityCenterState& state,
                                       const EditorActivityCenterPolicy& policy,
                                       const EditorActivityPostDesc& desc,
                                       double nowSeconds)
    {
        const std::string dedupeKey = BuildDedupeKey(desc);
        if (policy.mergeDuplicates && !dedupeKey.empty())
        {
            for (EditorActivityNotification& notification : state.notifications)
            {
                const bool withinMergeWindow = nowSeconds - notification.updatedSeconds <= policy.mergeWindowSeconds;
                if (!notification.expired && notification.dedupeKey == dedupeKey && withinMergeWindow)
                {
                    notification.message = desc.message;
                    notification.updatedSeconds = nowSeconds;
                    notification.ttlSeconds = desc.ttlSeconds > 0.0 ? desc.ttlSeconds : policy.defaultTtlSeconds;
                    notification.sticky = notification.sticky || desc.sticky;
                    notification.read = false;
                    ++notification.repeatCount;
                    ++state.revision;
                    RecomputeEditorActivityCenter(state);
                    return notification.id;
                }
            }
        }

        EditorActivityNotification notification{};
        notification.id = state.nextNotificationId++;
        notification.severity = desc.severity;
        notification.source = desc.source;
        notification.title = IsEmpty(desc.title) ? std::string("Notification") : desc.title;
        notification.message = desc.message;
        notification.dedupeKey = dedupeKey;
        notification.actions = desc.actions;
        notification.createdSeconds = nowSeconds;
        notification.updatedSeconds = nowSeconds;
        notification.ttlSeconds = desc.ttlSeconds > 0.0 ? desc.ttlSeconds : policy.defaultTtlSeconds;
        notification.sticky = desc.sticky;
        notification.dismissible = desc.dismissible;
        state.notifications.push_back(std::move(notification));

        while (state.notifications.size() > policy.maxNotifications)
        {
            auto oldest = std::min_element(state.notifications.begin(), state.notifications.end(), [](const auto& a, const auto& b)
            {
                return a.updatedSeconds < b.updatedSeconds;
            });
            if (oldest == state.notifications.end())
            {
                break;
            }
            oldest->expired = true;
            oldest->expireReason = EditorActivityExpireReason::Capacity;
            state.notifications.erase(oldest);
        }

        ++state.revision;
        RecomputeEditorActivityCenter(state);
        return state.notifications.empty() ? 0 : state.notifications.back().id;
    }

    u64 BeginEditorBackgroundTask(EditorActivityCenterState& state,
                                  const EditorActivityCenterPolicy& policy,
                                  EditorBackgroundTaskUpdate update,
                                  double nowSeconds)
    {
        EditorBackgroundTask task{};
        task.id = state.nextTaskId++;
        task.source = update.source;
        task.phase = update.phase;
        task.label = update.label.empty() ? "Background task" : update.label;
        task.details = update.details;
        task.progress = Clamp01(update.progress);
        task.createdSeconds = nowSeconds;
        task.updatedSeconds = nowSeconds;
        task.determinate = update.determinate;
        task.canCancel = update.canCancel;
        task.visible = update.visible;
        state.tasks.push_back(std::move(task));

        while (state.tasks.size() > policy.maxTasks)
        {
            auto removable = std::find_if(state.tasks.begin(), state.tasks.end(), [](const EditorBackgroundTask& item)
            {
                return IsCompletedTaskPhase(item.phase);
            });
            if (removable == state.tasks.end())
            {
                removable = state.tasks.begin();
            }
            state.tasks.erase(removable);
        }

        ++state.revision;
        RecomputeEditorActivityCenter(state);
        return state.nextTaskId - 1;
    }

    bool UpdateEditorBackgroundTask(EditorActivityCenterState& state,
                                    const EditorActivityCenterPolicy& policy,
                                    EditorBackgroundTaskUpdate update,
                                    double nowSeconds)
    {
        if (update.id == 0)
        {
            update.id = BeginEditorBackgroundTask(state, policy, update, nowSeconds);
            return update.id != 0;
        }

        auto it = std::find_if(state.tasks.begin(), state.tasks.end(), [&](const EditorBackgroundTask& task)
        {
            return task.id == update.id;
        });
        if (it == state.tasks.end())
        {
            return false;
        }

        const EditorActivityTaskPhase oldPhase = it->phase;
        it->source = update.source;
        it->phase = update.phase;
        if (!update.label.empty())
        {
            it->label = update.label;
        }
        it->details = update.details;
        it->progress = Clamp01(update.progress);
        it->updatedSeconds = nowSeconds;
        it->determinate = update.determinate;
        it->canCancel = update.canCancel;
        it->visible = update.visible;

        if (update.postCompletionNotification && !IsCompletedTaskPhase(oldPhase) && IsCompletedTaskPhase(update.phase))
        {
            const EditorActivityPostDesc desc = CompletionNotificationForTask(*it);
            it->completionNotificationId = PostEditorActivityNotification(state, policy, desc, nowSeconds);
        }

        ++state.revision;
        RecomputeEditorActivityCenter(state);
        return true;
    }

    bool DismissEditorActivityNotification(EditorActivityCenterState& state, u64 notificationId)
    {
        auto it = std::find_if(state.notifications.begin(), state.notifications.end(), [&](const EditorActivityNotification& notification)
        {
            return notification.id == notificationId;
        });
        if (it == state.notifications.end() || !it->dismissible)
        {
            return false;
        }
        it->expired = true;
        it->expireReason = EditorActivityExpireReason::Dismissed;
        state.notifications.erase(it);
        ++state.revision;
        RecomputeEditorActivityCenter(state);
        return true;
    }

    u32 MarkEditorActivityNotificationsRead(EditorActivityCenterState& state)
    {
        u32 changed = 0;
        for (EditorActivityNotification& notification : state.notifications)
        {
            if (!notification.read)
            {
                notification.read = true;
                ++changed;
            }
        }
        if (changed != 0)
        {
            ++state.revision;
        }
        RecomputeEditorActivityCenter(state);
        return changed;
    }

    EditorActivityTickResult TickEditorActivityCenter(EditorActivityCenterState& state,
                                                      const EditorActivityCenterPolicy& policy,
                                                      const EditorActivityTickInput& input)
    {
        EditorActivityTickResult result{};
        result.ok = true;

        const double now = input.nowSeconds;
        for (auto it = state.notifications.begin(); it != state.notifications.end();)
        {
            const bool expiredByTtl = !it->sticky
                && it->ttlSeconds > 0.0
                && now - it->updatedSeconds >= it->ttlSeconds;
            if (expiredByTtl)
            {
                it->expired = true;
                it->expireReason = EditorActivityExpireReason::TimeToLive;
                it = state.notifications.erase(it);
                ++result.expiredNotifications;
                result.changed = true;
            }
            else
            {
                ++it;
            }
        }

        while (state.notifications.size() > policy.maxNotifications)
        {
            state.notifications.erase(state.notifications.begin());
            ++result.capacityPrunedNotifications;
            result.changed = true;
        }

        if (policy.autoExpireCompletedTasks)
        {
            for (EditorBackgroundTask& task : state.tasks)
            {
                if (task.visible && IsCompletedTaskPhase(task.phase) && now - task.updatedSeconds > 2.0)
                {
                    task.visible = false;
                    ++result.completedTasksHidden;
                    result.changed = true;
                }
            }
        }

        if (result.changed)
        {
            ++state.revision;
        }
        RecomputeEditorActivityCenter(state);
        result.summary = FormatEditorActivityTickResult(result);
        return result;
    }

    void RecomputeEditorActivityCenter(EditorActivityCenterState& state)
    {
        state.unreadCount = 0;
        state.activeTaskCount = 0;
        state.errorCount = 0;
        state.warningCount = 0;
        state.highestSeverity = EditorActivitySeverity::Trace;

        for (const EditorActivityNotification& notification : state.notifications)
        {
            if (!notification.read)
            {
                ++state.unreadCount;
            }
            if (notification.severity == EditorActivitySeverity::Error)
            {
                ++state.errorCount;
            }
            if (notification.severity == EditorActivitySeverity::Warning)
            {
                ++state.warningCount;
            }
            state.highestSeverity = MaxSeverity(state.highestSeverity, notification.severity);
        }

        for (const EditorBackgroundTask& task : state.tasks)
        {
            if (task.visible && IsActiveTaskPhase(task.phase))
            {
                ++state.activeTaskCount;
            }
            if (task.visible && task.phase == EditorActivityTaskPhase::Failed)
            {
                ++state.errorCount;
                state.highestSeverity = MaxSeverity(state.highestSeverity, EditorActivitySeverity::Error);
            }
        }

        state.statusText = FormatEditorActivityCenterState(state);
    }

    std::string FormatEditorActivityNotification(const EditorActivityNotification& notification)
    {
        std::ostringstream out;
        out << "notification#" << notification.id
            << " " << ToString(notification.severity)
            << " source=" << ToString(notification.source)
            << " repeats=" << notification.repeatCount
            << " title='" << notification.title << "'";
        if (notification.sticky)
        {
            out << " sticky=1";
        }
        return out.str();
    }

    std::string FormatEditorBackgroundTask(const EditorBackgroundTask& task)
    {
        std::ostringstream out;
        out << "task#" << task.id
            << " " << ToString(task.phase)
            << " progress=" << task.progress
            << " label='" << task.label << "'";
        return out.str();
    }

    std::string FormatEditorActivityCenterState(const EditorActivityCenterState& state)
    {
        std::ostringstream out;
        out << "editor-activity notifications=" << state.notifications.size()
            << " unread=" << state.unreadCount
            << " tasks=" << state.tasks.size()
            << " active=" << state.activeTaskCount
            << " warnings=" << state.warningCount
            << " errors=" << state.errorCount
            << " highest=" << ToString(state.highestSeverity)
            << " revision=" << state.revision;
        return out.str();
    }

    std::string FormatEditorActivityTickResult(const EditorActivityTickResult& result)
    {
        std::ostringstream out;
        out << "editor-activity-tick expired=" << result.expiredNotifications
            << " capacityPruned=" << result.capacityPrunedNotifications
            << " tasksHidden=" << result.completedTasksHidden
            << " changed=" << result.changed
            << " ok=" << result.ok;
        return out.str();
    }

    EditorActivityDiagnostics RunEditorActivityDiagnostics()
    {
        EditorActivityDiagnostics diagnostics{};
        EditorActivityCenterPolicy policy = MakeDefaultEditorActivityCenterPolicy();
        policy.maxNotifications = 4;
        diagnostics.state = MakeDefaultEditorActivityCenterState();

        EditorActivityAction openLog{};
        openLog.id = "open-console";
        openLog.label = "Open Console";
        openLog.command = CommandId::RescanAssets;
        openLog.primary = true;

        EditorActivityPostDesc autosave{};
        autosave.severity = EditorActivitySeverity::Info;
        autosave.source = EditorActivitySource::Autosave;
        autosave.title = "Autosave complete";
        autosave.message = "Sandbox scene autosaved.";
        autosave.dedupeKey = "autosave:scene";
        autosave.actions.push_back(openLog);
        const u64 firstAutosave = PostEditorActivityNotification(diagnostics.state, policy, autosave, 1.0);
        const u64 secondAutosave = PostEditorActivityNotification(diagnostics.state, policy, autosave, 1.5);
        diagnostics.dedupeMerged = firstAutosave == secondAutosave && !diagnostics.state.notifications.empty() && diagnostics.state.notifications.front().repeatCount == 2;
        diagnostics.actionCommandBacked = !diagnostics.state.notifications.empty()
            && !diagnostics.state.notifications.front().actions.empty()
            && diagnostics.state.notifications.front().actions.front().command != CommandId::Count;

        EditorActivityPostDesc warning{};
        warning.severity = EditorActivitySeverity::Warning;
        warning.source = EditorActivitySource::Workspace;
        warning.title = "Layout repaired";
        warning.message = "Invalid splitter repaired.";
        PostEditorActivityNotification(diagnostics.state, policy, warning, 2.0);

        EditorActivityPostDesc error{};
        error.severity = EditorActivitySeverity::Error;
        error.source = EditorActivitySource::Renderer;
        error.title = "Shader fallback";
        error.message = "Shader fallback was used.";
        error.sticky = true;
        PostEditorActivityNotification(diagnostics.state, policy, error, 3.0);
        diagnostics.warningTracked = diagnostics.state.warningCount >= 1;
        diagnostics.errorTracked = diagnostics.state.errorCount >= 1;

        EditorBackgroundTaskUpdate import{};
        import.source = EditorActivitySource::AssetPipeline;
        import.phase = EditorActivityTaskPhase::Running;
        import.label = "Import assets";
        import.details = "Scanning project assets";
        import.progress = 0.25;
        const u64 taskId = BeginEditorBackgroundTask(diagnostics.state, policy, import, 4.0);
        import.id = taskId;
        import.phase = EditorActivityTaskPhase::Completed;
        import.details = "Imported 12 assets";
        import.progress = 1.0;
        UpdateEditorBackgroundTask(diagnostics.state, policy, import, 5.0);

        EditorActivityPostDesc capacity{};
        capacity.source = EditorActivitySource::Diagnostics;
        capacity.severity = EditorActivitySeverity::Info;
        capacity.title = "Capacity test";
        capacity.message = "notification";
        for (int i = 0; i < 6; ++i)
        {
            capacity.dedupeKey = "capacity:" + std::to_string(i);
            PostEditorActivityNotification(diagnostics.state, policy, capacity, 10.0 + static_cast<double>(i));
        }

        diagnostics.tick = TickEditorActivityCenter(diagnostics.state, policy, {20.0, 0.016, true});
        diagnostics.taskCompleted = std::any_of(diagnostics.state.tasks.begin(), diagnostics.state.tasks.end(), [](const EditorBackgroundTask& task)
        {
            return task.phase == EditorActivityTaskPhase::Completed && task.progress >= 1.0;
        });
        diagnostics.capacityPruned = diagnostics.state.notifications.size() <= policy.maxNotifications;
        diagnostics.ok = diagnostics.dedupeMerged
            && diagnostics.warningTracked
            && diagnostics.errorTracked
            && diagnostics.taskCompleted
            && diagnostics.capacityPruned
            && diagnostics.actionCommandBacked;
        diagnostics.summary = FormatEditorActivityCenterState(diagnostics.state);
        return diagnostics;
    }

}
