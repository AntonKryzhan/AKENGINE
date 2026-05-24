#include <AK/EditorUI/EditorDiagnosticsPanel.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace AK
{
    namespace
    {
        u64 HashText(std::string_view text)
        {
            u64 hash = 1469598103934665603ull;
            for (char c : text)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        std::string LowerCopy(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }

        bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
        {
            if (needle.empty())
            {
                return true;
            }
            return LowerCopy(std::string(haystack)).find(LowerCopy(std::string(needle))) != std::string::npos;
        }

        bool IsActiveTaskPhase(EditorActivityTaskPhase phase)
        {
            return phase == EditorActivityTaskPhase::Queued
                || phase == EditorActivityTaskPhase::Running
                || phase == EditorActivityTaskPhase::Waiting;
        }

        EditorDiagnosticsRowSeverity ConvertSeverity(EditorActivitySeverity severity)
        {
            switch (severity)
            {
                case EditorActivitySeverity::Trace: return EditorDiagnosticsRowSeverity::Trace;
                case EditorActivitySeverity::Info: return EditorDiagnosticsRowSeverity::Info;
                case EditorActivitySeverity::Success: return EditorDiagnosticsRowSeverity::Success;
                case EditorActivitySeverity::Warning: return EditorDiagnosticsRowSeverity::Warning;
                case EditorActivitySeverity::Error: return EditorDiagnosticsRowSeverity::Error;
                default: return EditorDiagnosticsRowSeverity::Info;
            }
        }

        EditorDiagnosticsRowSeverity ConvertSeverity(EditorConsoleSeverity severity)
        {
            switch (severity)
            {
                case EditorConsoleSeverity::Info: return EditorDiagnosticsRowSeverity::Info;
                case EditorConsoleSeverity::Warning: return EditorDiagnosticsRowSeverity::Warning;
                case EditorConsoleSeverity::Error: return EditorDiagnosticsRowSeverity::Error;
                default: return EditorDiagnosticsRowSeverity::Info;
            }
        }

        EditorConsoleSeverity ConvertConsoleSeverity(EditorDiagnosticsRowSeverity severity)
        {
            switch (severity)
            {
                case EditorDiagnosticsRowSeverity::Warning: return EditorConsoleSeverity::Warning;
                case EditorDiagnosticsRowSeverity::Error: return EditorConsoleSeverity::Error;
                default: return EditorConsoleSeverity::Info;
            }
        }

        EditorDiagnosticsRowSeverity ConvertWorkspaceSeverity(EditorWorkspaceIssueSeverity severity)
        {
            switch (severity)
            {
                case EditorWorkspaceIssueSeverity::Info: return EditorDiagnosticsRowSeverity::Info;
                case EditorWorkspaceIssueSeverity::Warning: return EditorDiagnosticsRowSeverity::Warning;
                case EditorWorkspaceIssueSeverity::Error: return EditorDiagnosticsRowSeverity::Error;
                default: return EditorDiagnosticsRowSeverity::Info;
            }
        }

        EditorDiagnosticsRowSeverity SeverityFromMetricWarning(bool warning)
        {
            return warning ? EditorDiagnosticsRowSeverity::Warning : EditorDiagnosticsRowSeverity::Info;
        }

        EditorDiagnosticsPanelRow MakeRow(EditorDiagnosticsRowKind kind,
                                          EditorDiagnosticsRowSeverity severity,
                                          std::string source,
                                          std::string category,
                                          std::string title,
                                          std::string message,
                                          std::string detail = {},
                                          double timeSeconds = 0.0)
        {
            EditorDiagnosticsPanelRow row{};
            row.kind = kind;
            row.severity = severity;
            row.source = std::move(source);
            row.category = std::move(category);
            row.title = std::move(title);
            row.message = std::move(message);
            row.detail = std::move(detail);
            row.timeSeconds = timeSeconds;
            row.stableHash = HashText(row.source + "|" + row.category + "|" + row.title + "|" + row.message + "|" + row.detail);
            row.id = row.stableHash;
            return row;
        }

        void AddMetric(EditorDiagnosticsPanelState& state,
                       std::string key,
                       std::string label,
                       std::string source,
                       double value,
                       EditorMetricUnit unit,
                       EditorDiagnosticsRowSeverity severity = EditorDiagnosticsRowSeverity::Info,
                       bool pinned = false)
        {
            EditorDiagnosticsPanelMetric metric{};
            metric.key = std::move(key);
            metric.label = std::move(label);
            metric.source = std::move(source);
            metric.value = std::isfinite(value) ? value : 0.0;
            metric.unit = unit;
            metric.severity = severity;
            metric.pinned = pinned;
            state.metrics.push_back(std::move(metric));
        }

        std::string MetricValueToText(double value, EditorMetricUnit unit)
        {
            std::ostringstream out;
            switch (unit)
            {
                case EditorMetricUnit::Bytes:
                    if (value >= 1024.0 * 1024.0)
                    {
                        out << static_cast<int>(value / (1024.0 * 1024.0)) << " MiB";
                    }
                    else if (value >= 1024.0)
                    {
                        out << static_cast<int>(value / 1024.0) << " KiB";
                    }
                    else
                    {
                        out << static_cast<int>(value) << " B";
                    }
                    break;
                case EditorMetricUnit::Milliseconds:
                    out << value << " ms";
                    break;
                case EditorMetricUnit::Percent:
                    out << value << "%";
                    break;
                case EditorMetricUnit::Count:
                default:
                    out << static_cast<u64>(value);
                    break;
            }
            return out.str();
        }

        void PushRowIfAccepted(EditorDiagnosticsPanelState& state, EditorDiagnosticsPanelRow row)
        {
            if (ShouldShowEditorDiagnosticsRow(row, state.filter))
            {
                state.rows.push_back(std::move(row));
            }
        }
    }

    const char* ToString(EditorDiagnosticsRowKind kind)
    {
        switch (kind)
        {
            case EditorDiagnosticsRowKind::Activity: return "activity";
            case EditorDiagnosticsRowKind::BackgroundTask: return "task";
            case EditorDiagnosticsRowKind::Console: return "console";
            case EditorDiagnosticsRowKind::Metric: return "metric";
            case EditorDiagnosticsRowKind::Autosave: return "autosave";
            case EditorDiagnosticsRowKind::Workspace: return "workspace";
            case EditorDiagnosticsRowKind::CommandState: return "command";
            default: return "unknown";
        }
    }

    const char* ToString(EditorDiagnosticsRowSeverity severity)
    {
        switch (severity)
        {
            case EditorDiagnosticsRowSeverity::Trace: return "trace";
            case EditorDiagnosticsRowSeverity::Info: return "info";
            case EditorDiagnosticsRowSeverity::Success: return "success";
            case EditorDiagnosticsRowSeverity::Warning: return "warning";
            case EditorDiagnosticsRowSeverity::Error: return "error";
            default: return "unknown";
        }
    }

    EditorDiagnosticsPanelFilter MakeDefaultEditorDiagnosticsPanelFilter()
    {
        return {};
    }

    bool ShouldShowEditorDiagnosticsRow(const EditorDiagnosticsPanelRow& row, const EditorDiagnosticsPanelFilter& filter)
    {
        switch (row.severity)
        {
            case EditorDiagnosticsRowSeverity::Trace: if (!filter.showTrace) return false; break;
            case EditorDiagnosticsRowSeverity::Info: if (!filter.showInfo) return false; break;
            case EditorDiagnosticsRowSeverity::Success: if (!filter.showSuccess) return false; break;
            case EditorDiagnosticsRowSeverity::Warning: if (!filter.showWarnings) return false; break;
            case EditorDiagnosticsRowSeverity::Error: if (!filter.showErrors) return false; break;
            default: break;
        }

        if (row.kind == EditorDiagnosticsRowKind::BackgroundTask && !filter.showCompletedTasks && !row.active)
        {
            return false;
        }
        if (row.kind == EditorDiagnosticsRowKind::CommandState && !filter.showCommandRows)
        {
            return false;
        }
        if (row.kind == EditorDiagnosticsRowKind::Workspace && !filter.showWorkspaceRows)
        {
            return false;
        }
        if (row.kind == EditorDiagnosticsRowKind::Autosave && !filter.showAutosaveRows)
        {
            return false;
        }
        if (row.kind == EditorDiagnosticsRowKind::Metric && !filter.showMetrics)
        {
            return false;
        }
        if (row.kind == EditorDiagnosticsRowKind::Activity && row.unread == false && !filter.showReadNotifications)
        {
            return false;
        }
        if (!filter.textFilter.empty())
        {
            const std::string haystack = row.source + " " + row.category + " " + row.title + " " + row.message + " " + row.detail;
            if (!ContainsCaseInsensitive(haystack, filter.textFilter))
            {
                return false;
            }
        }
        return true;
    }

    void SortEditorDiagnosticsPanelRows(EditorDiagnosticsPanelState& state)
    {
        std::stable_sort(state.rows.begin(), state.rows.end(), [](const EditorDiagnosticsPanelRow& a, const EditorDiagnosticsPanelRow& b)
        {
            if (a.timeSeconds != b.timeSeconds)
            {
                return a.timeSeconds > b.timeSeconds;
            }
            return a.stableHash < b.stableHash;
        });
    }

    void RecomputeEditorDiagnosticsPanelState(EditorDiagnosticsPanelState& state)
    {
        state.unreadCount = 0;
        state.warningCount = 0;
        state.errorCount = 0;
        state.activeTaskCount = 0;
        state.stickyCount = 0;

        for (const EditorDiagnosticsPanelRow& row : state.rows)
        {
            if (row.unread)
            {
                ++state.unreadCount;
            }
            if (row.severity == EditorDiagnosticsRowSeverity::Warning)
            {
                ++state.warningCount;
            }
            if (row.severity == EditorDiagnosticsRowSeverity::Error)
            {
                ++state.errorCount;
            }
            if (row.kind == EditorDiagnosticsRowKind::BackgroundTask && row.active)
            {
                ++state.activeTaskCount;
            }
            if (row.sticky)
            {
                ++state.stickyCount;
            }
        }

        if (state.selectedRowIndex >= state.rows.size())
        {
            state.selectedRowIndex = state.rows.empty() ? 0 : state.rows.size() - 1;
        }

        std::ostringstream out;
        out << "diagnostics rows=" << state.rows.size()
            << " metrics=" << state.metrics.size()
            << " unread=" << state.unreadCount
            << " warnings=" << state.warningCount
            << " errors=" << state.errorCount
            << " tasks=" << state.activeTaskCount;
        state.statusText = out.str();
        ++state.revision;
    }

    EditorDiagnosticsPanelState BuildEditorDiagnosticsPanelState(const EditorDiagnosticsPanelBuildInput& input,
                                                                 const EditorDiagnosticsPanelFilter& filter)
    {
        EditorDiagnosticsPanelState state{};
        state.filter = filter;

        if (input.activity)
        {
            AddMetric(state, "activity.unread", "Unread", "Activity", input.activity->unreadCount, EditorMetricUnit::Count, input.activity->unreadCount ? EditorDiagnosticsRowSeverity::Warning : EditorDiagnosticsRowSeverity::Info, true);
            AddMetric(state, "activity.tasks", "Active Tasks", "Activity", input.activity->activeTaskCount, EditorMetricUnit::Count, input.activity->activeTaskCount ? EditorDiagnosticsRowSeverity::Info : EditorDiagnosticsRowSeverity::Trace, true);
            AddMetric(state, "activity.errors", "Errors", "Activity", input.activity->errorCount, EditorMetricUnit::Count, input.activity->errorCount ? EditorDiagnosticsRowSeverity::Error : EditorDiagnosticsRowSeverity::Info, true);

            for (const EditorActivityNotification& notification : input.activity->notifications)
            {
                if (notification.expired)
                {
                    continue;
                }
                EditorDiagnosticsPanelRow row = MakeRow(EditorDiagnosticsRowKind::Activity,
                                                        ConvertSeverity(notification.severity),
                                                        ToString(notification.source),
                                                        "notification",
                                                        notification.title,
                                                        notification.message,
                                                        notification.repeatCount > 1 ? std::string("repeat=") + std::to_string(notification.repeatCount) : std::string{},
                                                        notification.updatedSeconds);
                row.id = notification.id;
                row.repeatCount = notification.repeatCount;
                row.unread = !notification.read;
                row.sticky = notification.sticky;
                row.command = notification.actions.empty() ? CommandId::Count : notification.actions.front().command;
                PushRowIfAccepted(state, std::move(row));
            }

            for (const EditorBackgroundTask& task : input.activity->tasks)
            {
                if (!task.visible)
                {
                    continue;
                }
                EditorDiagnosticsRowSeverity severity = EditorDiagnosticsRowSeverity::Info;
                if (task.phase == EditorActivityTaskPhase::Failed)
                {
                    severity = EditorDiagnosticsRowSeverity::Error;
                }
                else if (task.phase == EditorActivityTaskPhase::Cancelled)
                {
                    severity = EditorDiagnosticsRowSeverity::Warning;
                }
                else if (task.phase == EditorActivityTaskPhase::Completed)
                {
                    severity = EditorDiagnosticsRowSeverity::Success;
                }

                std::ostringstream detail;
                detail << ToString(task.phase) << " progress=" << static_cast<int>(task.progress * 100.0) << "%";
                EditorDiagnosticsPanelRow row = MakeRow(EditorDiagnosticsRowKind::BackgroundTask,
                                                        severity,
                                                        ToString(task.source),
                                                        "task",
                                                        task.label,
                                                        task.details,
                                                        detail.str(),
                                                        task.updatedSeconds);
                row.id = task.id;
                row.active = IsActiveTaskPhase(task.phase);
                PushRowIfAccepted(state, std::move(row));
            }
        }

        if (input.panelFrame)
        {
            AddMetric(state, "panel.console", "Console Rows", "Panels", static_cast<double>(input.panelFrame->console.entries.size()), EditorMetricUnit::Count);
            AddMetric(state, "panel.metrics", "Diagnostics Metrics", "Panels", static_cast<double>(input.panelFrame->diagnostics.metrics.size()), EditorMetricUnit::Count);

            for (const EditorConsoleEntry& entry : input.panelFrame->console.entries)
            {
                EditorDiagnosticsPanelRow row = MakeRow(EditorDiagnosticsRowKind::Console,
                                                        ConvertSeverity(entry.severity),
                                                        entry.channel,
                                                        "console",
                                                        entry.channel,
                                                        entry.message,
                                                        entry.repeatCount > 1 ? std::string("repeat=") + std::to_string(entry.repeatCount) : std::string{},
                                                        static_cast<double>(entry.frame));
                row.repeatCount = static_cast<u32>(std::min<u64>(entry.repeatCount, static_cast<u64>(0xFFFFFFFFu)));
                PushRowIfAccepted(state, std::move(row));
            }

            for (const EditorMetric& metric : input.panelFrame->diagnostics.metrics)
            {
                AddMetric(state, metric.key, metric.label, "Diagnostics", metric.value, metric.unit, SeverityFromMetricWarning(metric.warning));
                EditorDiagnosticsPanelRow row = MakeRow(EditorDiagnosticsRowKind::Metric,
                                                        SeverityFromMetricWarning(metric.warning),
                                                        "Diagnostics",
                                                        "metric",
                                                        metric.label,
                                                        MetricValueToText(metric.value, metric.unit),
                                                        metric.key,
                                                        input.nowSeconds);
                PushRowIfAccepted(state, std::move(row));
            }
        }

        if (input.autosaveState || input.autosavePolicy)
        {
            if (input.autosaveState)
            {
                AddMetric(state, "autosave.triggers", "Autosave Triggers", "Autosave", static_cast<double>(input.autosaveState->triggerCount), EditorMetricUnit::Count);
                AddMetric(state, "autosave.scene", "Scene Autosaves", "Autosave", static_cast<double>(input.autosaveState->sceneAutosaveCount), EditorMetricUnit::Count);
                AddMetric(state, "autosave.skips", "Autosave Skips", "Autosave", static_cast<double>(input.autosaveState->skippedCount), EditorMetricUnit::Count, input.autosaveState->skippedCount ? EditorDiagnosticsRowSeverity::Warning : EditorDiagnosticsRowSeverity::Info);
            }
            std::string message = "policy";
            if (input.autosavePolicy)
            {
                message = FormatEditorAutosavePolicy(*input.autosavePolicy);
            }
            if (input.autosaveState)
            {
                message += " lastSkip=";
                message += ToString(input.autosaveState->lastSkipReason);
            }
            PushRowIfAccepted(state, MakeRow(EditorDiagnosticsRowKind::Autosave,
                                             input.autosavePolicy && input.autosavePolicy->enabled ? EditorDiagnosticsRowSeverity::Info : EditorDiagnosticsRowSeverity::Warning,
                                             "Autosave",
                                             "runtime",
                                             "Autosave runtime",
                                             message,
                                             {},
                                             input.nowSeconds));
        }

        if (input.workspaceLoad)
        {
            EditorDiagnosticsRowSeverity severity = input.workspaceLoad->ok ? EditorDiagnosticsRowSeverity::Success : EditorDiagnosticsRowSeverity::Error;
            PushRowIfAccepted(state, MakeRow(EditorDiagnosticsRowKind::Workspace,
                                             severity,
                                             "Workspace",
                                             "startup",
                                             "Workspace load",
                                             input.workspaceLoad->summary,
                                             FormatEditorWorkspaceReport(input.workspaceLoad->report),
                                             input.nowSeconds));
        }

        if (input.workspaceReport)
        {
            AddMetric(state, "workspace.issues", "Workspace Issues", "Workspace", static_cast<double>(input.workspaceReport->issues.size()), EditorMetricUnit::Count, input.workspaceReport->issues.empty() ? EditorDiagnosticsRowSeverity::Info : EditorDiagnosticsRowSeverity::Warning);
            for (const EditorWorkspaceIssue& issue : input.workspaceReport->issues)
            {
                PushRowIfAccepted(state, MakeRow(EditorDiagnosticsRowKind::Workspace,
                                                 ConvertWorkspaceSeverity(issue.severity),
                                                 "Workspace",
                                                 ToString(issue.fileKind),
                                                 ToString(issue.code),
                                                 issue.message,
                                                 issue.path,
                                                 input.nowSeconds));
            }
        }

        if (input.commandStates)
        {
            AddMetric(state, "commands.total", "Commands", "Commands", static_cast<double>(input.commandStates->entries.size()), EditorMetricUnit::Count);
            usize disabledCount = 0;
            for (const EditorCommandStateEntry& entry : input.commandStates->entries)
            {
                if (!entry.visible)
                {
                    continue;
                }
                if (!entry.enabled)
                {
                    ++disabledCount;
                }
                EditorDiagnosticsPanelRow row = MakeRow(EditorDiagnosticsRowKind::CommandState,
                                                        entry.enabled ? EditorDiagnosticsRowSeverity::Trace : EditorDiagnosticsRowSeverity::Warning,
                                                        "Commands",
                                                        "state",
                                                        entry.label.empty() ? entry.name : entry.label,
                                                        entry.enabled ? "enabled" : entry.disabledReasonText,
                                                        ToString(entry.disabledReason),
                                                        static_cast<double>(entry.revision));
                row.command = entry.command;
                PushRowIfAccepted(state, std::move(row));
            }
            AddMetric(state, "commands.disabled", "Disabled Commands", "Commands", static_cast<double>(disabledCount), EditorMetricUnit::Count, disabledCount ? EditorDiagnosticsRowSeverity::Warning : EditorDiagnosticsRowSeverity::Info);
        }

        SortEditorDiagnosticsPanelRows(state);
        if (state.rows.size() > state.filter.maxRows)
        {
            state.rows.resize(state.filter.maxRows);
        }
        RecomputeEditorDiagnosticsPanelState(state);
        return state;
    }

    EditorDiagnosticsConsoleSyncResult SyncEditorDiagnosticsPanelToConsole(EditorConsoleModel& console,
                                                                           const EditorDiagnosticsPanelState& state,
                                                                           usize maxConsoleEntries)
    {
        EditorDiagnosticsConsoleSyncResult result{};
        for (const EditorDiagnosticsPanelRow& row : state.rows)
        {
            if (row.kind != EditorDiagnosticsRowKind::Activity
                && row.kind != EditorDiagnosticsRowKind::BackgroundTask
                && row.kind != EditorDiagnosticsRowKind::Autosave
                && row.kind != EditorDiagnosticsRowKind::Workspace)
            {
                continue;
            }

            EditorConsoleEntry entry{};
            entry.severity = ConvertConsoleSeverity(row.severity);
            entry.channel = row.source.empty() ? "Editor" : row.source;
            entry.message = FormatEditorDiagnosticsPanelRow(row);
            entry.frame = row.id;
            entry.repeatCount = row.repeatCount;
            console.entries.push_back(std::move(entry));
            ++result.entriesAdded;
            switch (row.severity)
            {
                case EditorDiagnosticsRowSeverity::Warning: ++result.warningEntries; break;
                case EditorDiagnosticsRowSeverity::Error: ++result.errorEntries; break;
                default: ++result.infoEntries; break;
            }
        }

        if (maxConsoleEntries > 0 && console.entries.size() > maxConsoleEntries)
        {
            const usize removeCount = console.entries.size() - maxConsoleEntries;
            console.entries.erase(console.entries.begin(), console.entries.begin() + static_cast<std::ptrdiff_t>(removeCount));
            result.trimmed = true;
        }
        ++console.revision;
        result.ok = result.entriesAdded > 0;
        result.summary = FormatEditorDiagnosticsConsoleSyncResult(result);
        return result;
    }

    bool ApplyEditorDiagnosticsPanelToFrame(EditorPanelModelFrame& frame, const EditorDiagnosticsPanelState& state, usize maxRecentEvents)
    {
        frame.diagnostics.metrics.clear();
        frame.diagnostics.recentEvents.clear();

        for (const EditorDiagnosticsPanelMetric& metric : state.metrics)
        {
            EditorMetric out{};
            out.key = metric.key;
            out.label = metric.label;
            out.value = metric.value;
            out.unit = metric.unit;
            out.warning = metric.severity == EditorDiagnosticsRowSeverity::Warning || metric.severity == EditorDiagnosticsRowSeverity::Error;
            frame.diagnostics.metrics.push_back(std::move(out));
        }

        const usize limit = std::min<usize>(maxRecentEvents, state.rows.size());
        for (usize i = 0; i < limit; ++i)
        {
            frame.diagnostics.recentEvents.push_back(FormatEditorDiagnosticsPanelRow(state.rows[i]));
        }

        frame.diagnostics.revision++;
        frame.revision++;
        frame.searchIndex = BuildPanelModelSearchIndex(frame, frame.searchIndex);
        return !frame.diagnostics.metrics.empty() || !frame.diagnostics.recentEvents.empty();
    }

    std::string FormatEditorDiagnosticsPanelRow(const EditorDiagnosticsPanelRow& row)
    {
        std::ostringstream out;
        out << '[' << ToString(row.severity) << "] "
            << ToString(row.kind) << ':' << (row.source.empty() ? "Editor" : row.source)
            << " " << row.title;
        if (!row.message.empty())
        {
            out << " - " << row.message;
        }
        if (!row.detail.empty())
        {
            out << " (" << row.detail << ')';
        }
        return out.str();
    }

    std::string FormatEditorDiagnosticsPanelMetric(const EditorDiagnosticsPanelMetric& metric)
    {
        std::ostringstream out;
        out << "metric " << metric.source << ':' << metric.key << '=' << MetricValueToText(metric.value, metric.unit)
            << " severity=" << ToString(metric.severity);
        return out.str();
    }

    std::string FormatEditorDiagnosticsPanelState(const EditorDiagnosticsPanelState& state)
    {
        std::ostringstream out;
        out << "editor-diagnostics rows=" << state.rows.size()
            << " metrics=" << state.metrics.size()
            << " unread=" << state.unreadCount
            << " warnings=" << state.warningCount
            << " errors=" << state.errorCount
            << " activeTasks=" << state.activeTaskCount
            << " sticky=" << state.stickyCount
            << " revision=" << state.revision;
        return out.str();
    }

    std::string FormatEditorDiagnosticsConsoleSyncResult(const EditorDiagnosticsConsoleSyncResult& result)
    {
        std::ostringstream out;
        out << "editor-diagnostics-console entries=" << result.entriesAdded
            << " info=" << result.infoEntries
            << " warnings=" << result.warningEntries
            << " errors=" << result.errorEntries
            << " trimmed=" << result.trimmed
            << " ok=" << result.ok;
        return out.str();
    }

    EditorDiagnosticsPanelDiagnostics RunEditorDiagnosticsPanelDiagnostics()
    {
        EditorDiagnosticsPanelDiagnostics diagnostics{};

        EditorActivityCenterPolicy activityPolicy = MakeDefaultEditorActivityCenterPolicy();
        activityPolicy.maxNotifications = 8;
        EditorActivityCenterState activity = MakeDefaultEditorActivityCenterState();
        EditorActivityPostDesc info{};
        info.source = EditorActivitySource::Workspace;
        info.severity = EditorActivitySeverity::Success;
        info.title = "Workspace loaded";
        info.message = "Layout/session/preferences restored.";
        info.dedupeKey = "workspace-loaded";
        PostEditorActivityNotification(activity, activityPolicy, info, 10.0);

        EditorActivityPostDesc warning{};
        warning.source = EditorActivitySource::Autosave;
        warning.severity = EditorActivitySeverity::Warning;
        warning.title = "Autosave skipped";
        warning.message = "Scene revision already saved.";
        warning.dedupeKey = "autosave-skip";
        warning.sticky = true;
        PostEditorActivityNotification(activity, activityPolicy, warning, 11.0);

        EditorBackgroundTaskUpdate task{};
        task.source = EditorActivitySource::AssetPipeline;
        task.phase = EditorActivityTaskPhase::Running;
        task.label = "Import assets";
        task.details = "Cooking material previews";
        task.progress = 0.5;
        BeginEditorBackgroundTask(activity, activityPolicy, task, 12.0);

        EditorRuntimeBridge bridge = BuildDefaultEditorRuntimeBridge(1366, 768);
        EditorPanelModelFrame frame = BuildDefaultEditorPanelModelFrame(bridge);
        frame.console.entries.push_back({EditorConsoleSeverity::Warning, "Renderer", "FrameGraph diagnostic row", 42, 1});

        EditorAutosavePolicy autosavePolicy{};
        autosavePolicy.enabled = true;
        EditorAutosaveRuntimeState autosaveState{};
        autosaveState.triggerCount = 2;
        autosaveState.sceneAutosaveCount = 1;
        autosaveState.skippedCount = 1;
        autosaveState.lastSkipReason = EditorAutosaveSkipReason::AlreadySavedRevision;

        EditorWorkspaceReport workspaceReport{};
        workspaceReport.ok = true;
        workspaceReport.issues.push_back({EditorWorkspaceIssueCode::DefaultsUsed, EditorWorkspaceIssueSeverity::Warning, EditorWorkspaceFileKind::Session, ".akcache/editor/session.akeditorsession", "Session was rebuilt from defaults."});

        CommandRegistry registry = BuildDefaultEditorCommandRegistry();
        EditorFocusState focus = MakeDefaultEditorFocusState();
        EditorCommandContext context = BuildEditorCommandContext(focus, true, true, false, true, true, true);
        EditorCommandStateCache commandStates = BuildEditorCommandStateCache(registry, context);

        EditorDiagnosticsPanelFilter filter = MakeDefaultEditorDiagnosticsPanelFilter();
        filter.maxRows = 128;
        EditorDiagnosticsPanelBuildInput input{};
        input.activity = &activity;
        input.panelFrame = &frame;
        input.autosaveState = &autosaveState;
        input.autosavePolicy = &autosavePolicy;
        input.workspaceReport = &workspaceReport;
        input.commandStates = &commandStates;
        input.nowSeconds = 13.0;
        diagnostics.state = BuildEditorDiagnosticsPanelState(input, filter);

        for (const EditorDiagnosticsPanelRow& row : diagnostics.state.rows)
        {
            switch (row.kind)
            {
                case EditorDiagnosticsRowKind::Activity: ++diagnostics.activityRows; break;
                case EditorDiagnosticsRowKind::BackgroundTask: ++diagnostics.taskRows; break;
                case EditorDiagnosticsRowKind::Console: ++diagnostics.consoleRows; break;
                case EditorDiagnosticsRowKind::Metric: ++diagnostics.metricRows; break;
                case EditorDiagnosticsRowKind::CommandState: ++diagnostics.commandRows; break;
                case EditorDiagnosticsRowKind::Workspace: ++diagnostics.workspaceRows; break;
                case EditorDiagnosticsRowKind::Autosave: ++diagnostics.autosaveRows; break;
                default: break;
            }
        }

        EditorDiagnosticsPanelFilter warningFilter = filter;
        warningFilter.showTrace = false;
        warningFilter.showInfo = false;
        warningFilter.showSuccess = false;
        EditorDiagnosticsPanelState warningState = BuildEditorDiagnosticsPanelState(input, warningFilter);
        diagnostics.filterOk = !warningState.rows.empty() && std::all_of(warningState.rows.begin(), warningState.rows.end(), [](const EditorDiagnosticsPanelRow& row)
        {
            return row.severity == EditorDiagnosticsRowSeverity::Warning || row.severity == EditorDiagnosticsRowSeverity::Error;
        });

        const u32 countedWarnings = static_cast<u32>(std::count_if(diagnostics.state.rows.begin(), diagnostics.state.rows.end(), [](const EditorDiagnosticsPanelRow& row)
        {
            return row.severity == EditorDiagnosticsRowSeverity::Warning;
        }));
        const u32 countedErrors = static_cast<u32>(std::count_if(diagnostics.state.rows.begin(), diagnostics.state.rows.end(), [](const EditorDiagnosticsPanelRow& row)
        {
            return row.severity == EditorDiagnosticsRowSeverity::Error;
        }));
        diagnostics.countersOk = countedWarnings == diagnostics.state.warningCount && countedErrors == diagnostics.state.errorCount;

        diagnostics.consoleSync = SyncEditorDiagnosticsPanelToConsole(frame.console, diagnostics.state, 128);
        diagnostics.frameApplyOk = ApplyEditorDiagnosticsPanelToFrame(frame, diagnostics.state, 16);

        diagnostics.ok = diagnostics.activityRows > 0
            && diagnostics.taskRows > 0
            && diagnostics.consoleRows > 0
            && diagnostics.metricRows > 0
            && diagnostics.commandRows > 0
            && diagnostics.workspaceRows > 0
            && diagnostics.autosaveRows > 0
            && diagnostics.consoleSync.ok
            && diagnostics.filterOk
            && diagnostics.countersOk
            && diagnostics.frameApplyOk;

        std::ostringstream out;
        out << "editor-diagnostics-panel rows=" << diagnostics.state.rows.size()
            << " activity=" << diagnostics.activityRows
            << " tasks=" << diagnostics.taskRows
            << " console=" << diagnostics.consoleRows
            << " metrics=" << diagnostics.metricRows
            << " commands=" << diagnostics.commandRows
            << " workspace=" << diagnostics.workspaceRows
            << " autosave=" << diagnostics.autosaveRows
            << " sync=" << diagnostics.consoleSync.entriesAdded
            << " ok=" << diagnostics.ok;
        diagnostics.summary = out.str();
        return diagnostics;
    }

}
