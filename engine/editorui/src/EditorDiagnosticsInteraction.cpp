#include <AK/EditorUI/EditorDiagnosticsInteraction.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool IsCompletedTask(EditorActivityTaskPhase phase)
        {
            return phase == EditorActivityTaskPhase::Completed
                || phase == EditorActivityTaskPhase::Failed
                || phase == EditorActivityTaskPhase::Cancelled;
        }

        i32 ClampScroll(i32 value, i32 contentHeight, i32 viewportHeight)
        {
            if (contentHeight <= viewportHeight)
            {
                return 0;
            }
            return std::max<i32>(0, std::min<i32>(value, contentHeight - viewportHeight));
        }

        void Bump(EditorDiagnosticsInteractionState& state)
        {
            ++state.revision;
            state.statusText = FormatEditorDiagnosticsInteractionState(state);
        }

        void SetResultStatus(EditorDiagnosticsInteractionResult& result, const EditorDiagnosticsInteractionState& state)
        {
            result.statusText = FormatEditorDiagnosticsInteractionState(state);
        }

        bool Toggle(bool& value)
        {
            value = !value;
            return true;
        }

        EditorDiagnosticsPanelFilter& MutableFilter(EditorDiagnosticsInteractionState& state)
        {
            state.filter.textFilter = state.searchQuery;
            return state.filter;
        }

        u32 DismissByRowId(EditorActivityCenterState& activity, u64 rowId)
        {
            if (rowId == 0)
            {
                return 0;
            }
            return DismissEditorActivityNotification(activity, rowId) ? 1u : 0u;
        }

        u32 HideCompletedTasks(EditorActivityCenterState& activity)
        {
            u32 hidden = 0;
            for (EditorBackgroundTask& task : activity.tasks)
            {
                if (task.visible && IsCompletedTask(task.phase))
                {
                    task.visible = false;
                    ++hidden;
                }
            }
            if (hidden != 0)
            {
                ++activity.revision;
                RecomputeEditorActivityCenter(activity);
            }
            return hidden;
        }
    }

    const char* ToString(EditorDiagnosticsInteractionAction action)
    {
        switch (action)
        {
            case EditorDiagnosticsInteractionAction::None: return "none";
            case EditorDiagnosticsInteractionAction::ToggleUnread: return "toggle-unread";
            case EditorDiagnosticsInteractionAction::ToggleWarnings: return "toggle-warnings";
            case EditorDiagnosticsInteractionAction::ToggleErrors: return "toggle-errors";
            case EditorDiagnosticsInteractionAction::ToggleTasks: return "toggle-tasks";
            case EditorDiagnosticsInteractionAction::ToggleAutosave: return "toggle-autosave";
            case EditorDiagnosticsInteractionAction::ToggleWorkspace: return "toggle-workspace";
            case EditorDiagnosticsInteractionAction::ToggleCommands: return "toggle-commands";
            case EditorDiagnosticsInteractionAction::ToggleCompletedTasks: return "toggle-completed-tasks";
            case EditorDiagnosticsInteractionAction::ToggleReadNotifications: return "toggle-read-notifications";
            case EditorDiagnosticsInteractionAction::SetSearchQuery: return "set-search";
            case EditorDiagnosticsInteractionAction::ClearSearchQuery: return "clear-search";
            case EditorDiagnosticsInteractionAction::SelectRow: return "select-row";
            case EditorDiagnosticsInteractionAction::MarkAllRead: return "mark-all-read";
            case EditorDiagnosticsInteractionAction::DismissSelectedNotification: return "dismiss-selected";
            case EditorDiagnosticsInteractionAction::ClearCompletedTasks: return "clear-completed-tasks";
            case EditorDiagnosticsInteractionAction::InvokeSelectedCommand: return "invoke-selected-command";
            case EditorDiagnosticsInteractionAction::ScrollRows: return "scroll-rows";
            case EditorDiagnosticsInteractionAction::ResetFilters: return "reset-filters";
            default: return "unknown";
        }
    }

    const char* ToString(EditorDiagnosticsInteractionTarget target)
    {
        switch (target)
        {
            case EditorDiagnosticsInteractionTarget::None: return "none";
            case EditorDiagnosticsInteractionTarget::Badge: return "badge";
            case EditorDiagnosticsInteractionTarget::SearchBox: return "search";
            case EditorDiagnosticsInteractionTarget::Row: return "row";
            case EditorDiagnosticsInteractionTarget::ToolbarAction: return "toolbar-action";
            case EditorDiagnosticsInteractionTarget::ListViewport: return "list";
            default: return "unknown";
        }
    }

    EditorDiagnosticsInteractionState MakeDefaultEditorDiagnosticsInteractionState()
    {
        EditorDiagnosticsInteractionState state{};
        state.filter = MakeDefaultEditorDiagnosticsPanelFilter();
        state.searchQuery = state.filter.textFilter;
        state.statusText = FormatEditorDiagnosticsInteractionState(state);
        return state;
    }

    EditorDiagnosticsPanelFilter BuildEditorDiagnosticsPanelFilterFromInteraction(const EditorDiagnosticsInteractionState& state)
    {
        EditorDiagnosticsPanelFilter filter = state.filter;
        filter.textFilter = state.searchQuery;
        return filter;
    }

    void SyncEditorDiagnosticsInteractionFromPanel(EditorDiagnosticsInteractionState& state, const EditorDiagnosticsPanelState& panel)
    {
        if (panel.rows.empty())
        {
            state.selectedRowIndex = 0;
            state.selectedRowId = 0;
            return;
        }
        if (state.selectedRowId != 0)
        {
            auto it = std::find_if(panel.rows.begin(), panel.rows.end(), [&](const EditorDiagnosticsPanelRow& row)
            {
                return row.id == state.selectedRowId;
            });
            if (it != panel.rows.end())
            {
                state.selectedRowIndex = static_cast<usize>(std::distance(panel.rows.begin(), it));
                return;
            }
        }
        if (state.selectedRowIndex >= panel.rows.size())
        {
            state.selectedRowIndex = panel.rows.size() - 1;
        }
        state.selectedRowId = panel.rows[state.selectedRowIndex].id;
    }

    EditorDiagnosticsInteractionResult ApplyEditorDiagnosticsInteraction(EditorDiagnosticsInteractionState& state,
                                                                         EditorActivityCenterState* activity,
                                                                         const EditorDiagnosticsPanelState& panel,
                                                                         const EditorDiagnosticsInteractionInput& input)
    {
        EditorDiagnosticsInteractionResult result{};
        result.command.id = CommandId::Count;
        result.command.source = CommandSource::Programmatic;

        EditorDiagnosticsPanelFilter& filter = MutableFilter(state);

        switch (input.action)
        {
            case EditorDiagnosticsInteractionAction::ToggleUnread:
                result.handled = Toggle(filter.showReadNotifications);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleWarnings:
                result.handled = Toggle(filter.showWarnings);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleErrors:
                result.handled = Toggle(filter.showErrors);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleTasks:
                result.handled = Toggle(filter.showCompletedTasks);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleAutosave:
                result.handled = Toggle(filter.showAutosaveRows);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleWorkspace:
                result.handled = Toggle(filter.showWorkspaceRows);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleCommands:
                result.handled = Toggle(filter.showCommandRows);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleCompletedTasks:
                result.handled = Toggle(filter.showCompletedTasks);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::ToggleReadNotifications:
                result.handled = Toggle(filter.showReadNotifications);
                result.filterChanged = true;
                state.filterDirty = true;
                break;
            case EditorDiagnosticsInteractionAction::SetSearchQuery:
            {
                const std::string next = input.appendText ? state.searchQuery + input.text : input.text;
                if (next != state.searchQuery)
                {
                    state.searchQuery = next;
                    filter.textFilter = state.searchQuery;
                    result.filterChanged = true;
                    state.filterDirty = true;
                }
                result.handled = true;
                break;
            }
            case EditorDiagnosticsInteractionAction::ClearSearchQuery:
                if (!state.searchQuery.empty() || !filter.textFilter.empty())
                {
                    state.searchQuery.clear();
                    filter.textFilter.clear();
                    result.filterChanged = true;
                    state.filterDirty = true;
                }
                result.handled = true;
                break;
            case EditorDiagnosticsInteractionAction::SelectRow:
                if (!panel.rows.empty())
                {
                    const usize index = std::min<usize>(input.rowIndex, panel.rows.size() - 1);
                    if (state.selectedRowIndex != index || state.selectedRowId != panel.rows[index].id)
                    {
                        state.selectedRowIndex = index;
                        state.selectedRowId = panel.rows[index].id;
                        result.selectionChanged = true;
                        state.selectionDirty = true;
                    }
                    result.handled = true;
                }
                break;
            case EditorDiagnosticsInteractionAction::MarkAllRead:
                if (activity)
                {
                    result.notificationsMarkedRead = MarkEditorActivityNotificationsRead(*activity);
                    result.activityChanged = result.notificationsMarkedRead != 0;
                    state.activityDirty = state.activityDirty || result.activityChanged;
                }
                result.handled = true;
                break;
            case EditorDiagnosticsInteractionAction::DismissSelectedNotification:
                if (activity)
                {
                    u64 rowId = state.selectedRowId;
                    if (rowId == 0 && state.selectedRowIndex < panel.rows.size())
                    {
                        rowId = panel.rows[state.selectedRowIndex].id;
                    }
                    result.notificationsDismissed = DismissByRowId(*activity, rowId);
                    result.activityChanged = result.notificationsDismissed != 0;
                    state.activityDirty = state.activityDirty || result.activityChanged;
                }
                result.handled = true;
                break;
            case EditorDiagnosticsInteractionAction::ClearCompletedTasks:
                if (activity)
                {
                    result.completedTasksCleared = HideCompletedTasks(*activity);
                    result.activityChanged = result.completedTasksCleared != 0;
                    state.activityDirty = state.activityDirty || result.activityChanged;
                }
                result.handled = true;
                break;
            case EditorDiagnosticsInteractionAction::InvokeSelectedCommand:
                if (state.selectedRowIndex < panel.rows.size() && panel.rows[state.selectedRowIndex].command != CommandId::Count)
                {
                    result.command.id = panel.rows[state.selectedRowIndex].command;
                    result.command.source = CommandSource::Programmatic;
                    result.commandQueued = true;
                    state.commandQueued = true;
                }
                result.handled = true;
                break;
            case EditorDiagnosticsInteractionAction::ScrollRows:
            {
                const i32 rowHeight = std::max<i32>(1, input.rowHeightPixels);
                const i32 before = state.scrollOffsetPixels;
                state.scrollOffsetPixels = ClampScroll(state.scrollOffsetPixels - (input.wheelDelta / 120) * rowHeight * 3,
                                                       input.contentHeightPixels,
                                                       input.viewportHeightPixels);
                result.scrollChanged = before != state.scrollOffsetPixels;
                state.scrollDirty = state.scrollDirty || result.scrollChanged;
                result.handled = true;
                break;
            }
            case EditorDiagnosticsInteractionAction::ResetFilters:
                state.filter = MakeDefaultEditorDiagnosticsPanelFilter();
                state.searchQuery.clear();
                result.filterChanged = true;
                state.filterDirty = true;
                result.handled = true;
                break;
            case EditorDiagnosticsInteractionAction::None:
            default:
                break;
        }

        if (result.filterChanged || result.selectionChanged || result.activityChanged || result.scrollChanged || result.commandQueued)
        {
            Bump(state);
        }
        SetResultStatus(result, state);
        return result;
    }

    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsBadgeToggle(EditorDiagnosticsViewBadgeKind badge)
    {
        EditorDiagnosticsInteractionInput input{};
        input.target = EditorDiagnosticsInteractionTarget::Badge;
        input.badge = badge;
        switch (badge)
        {
            case EditorDiagnosticsViewBadgeKind::Unread: input.action = EditorDiagnosticsInteractionAction::ToggleReadNotifications; break;
            case EditorDiagnosticsViewBadgeKind::Warnings: input.action = EditorDiagnosticsInteractionAction::ToggleWarnings; break;
            case EditorDiagnosticsViewBadgeKind::Errors: input.action = EditorDiagnosticsInteractionAction::ToggleErrors; break;
            case EditorDiagnosticsViewBadgeKind::Tasks: input.action = EditorDiagnosticsInteractionAction::ToggleCompletedTasks; break;
            case EditorDiagnosticsViewBadgeKind::Autosave: input.action = EditorDiagnosticsInteractionAction::ToggleAutosave; break;
            case EditorDiagnosticsViewBadgeKind::Workspace: input.action = EditorDiagnosticsInteractionAction::ToggleWorkspace; break;
            case EditorDiagnosticsViewBadgeKind::Commands: input.action = EditorDiagnosticsInteractionAction::ToggleCommands; break;
            default: input.action = EditorDiagnosticsInteractionAction::ResetFilters; break;
        }
        return input;
    }

    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsSearchChange(std::string query)
    {
        EditorDiagnosticsInteractionInput input{};
        input.action = EditorDiagnosticsInteractionAction::SetSearchQuery;
        input.target = EditorDiagnosticsInteractionTarget::SearchBox;
        input.text = std::move(query);
        return input;
    }

    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsRowSelect(usize rowIndex)
    {
        EditorDiagnosticsInteractionInput input{};
        input.action = EditorDiagnosticsInteractionAction::SelectRow;
        input.target = EditorDiagnosticsInteractionTarget::Row;
        input.rowIndex = rowIndex;
        return input;
    }

    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsScroll(i32 wheelDelta, i32 rowHeightPixels, i32 viewportHeightPixels, i32 contentHeightPixels)
    {
        EditorDiagnosticsInteractionInput input{};
        input.action = EditorDiagnosticsInteractionAction::ScrollRows;
        input.target = EditorDiagnosticsInteractionTarget::ListViewport;
        input.wheelDelta = wheelDelta;
        input.rowHeightPixels = rowHeightPixels;
        input.viewportHeightPixels = viewportHeightPixels;
        input.contentHeightPixels = contentHeightPixels;
        return input;
    }

    bool ValidateEditorDiagnosticsInteractionState(const EditorDiagnosticsInteractionState& state)
    {
        if (state.scrollOffsetPixels < 0)
        {
            return false;
        }
        if (state.filter.maxRows == 0)
        {
            return false;
        }
        if (state.searchQuery != state.filter.textFilter)
        {
            return false;
        }
        return true;
    }

    std::string FormatEditorDiagnosticsInteractionState(const EditorDiagnosticsInteractionState& state)
    {
        std::ostringstream out;
        out << "editor-diagnostics-interaction selected=" << state.selectedRowIndex
            << " rowId=" << state.selectedRowId
            << " scroll=" << state.scrollOffsetPixels
            << " search='" << state.searchQuery << "'"
            << " warnings=" << (state.filter.showWarnings ? 1 : 0)
            << " errors=" << (state.filter.showErrors ? 1 : 0)
            << " read=" << (state.filter.showReadNotifications ? 1 : 0)
            << " tasks=" << (state.filter.showCompletedTasks ? 1 : 0)
            << " workspace=" << (state.filter.showWorkspaceRows ? 1 : 0)
            << " autosave=" << (state.filter.showAutosaveRows ? 1 : 0)
            << " commands=" << (state.filter.showCommandRows ? 1 : 0)
            << " revision=" << state.revision;
        return out.str();
    }

    std::string FormatEditorDiagnosticsInteractionResult(const EditorDiagnosticsInteractionResult& result)
    {
        std::ostringstream out;
        out << "editor-diagnostics-interaction-result handled=" << (result.handled ? 1 : 0)
            << " filter=" << (result.filterChanged ? 1 : 0)
            << " selection=" << (result.selectionChanged ? 1 : 0)
            << " activity=" << (result.activityChanged ? 1 : 0)
            << " scroll=" << (result.scrollChanged ? 1 : 0)
            << " command=" << (result.commandQueued ? 1 : 0)
            << " read=" << result.notificationsMarkedRead
            << " dismissed=" << result.notificationsDismissed
            << " clearedTasks=" << result.completedTasksCleared;
        return out.str();
    }

    EditorDiagnosticsInteractionDiagnostics RunEditorDiagnosticsInteractionDiagnostics()
    {
        EditorDiagnosticsInteractionDiagnostics diagnostics{};
        diagnostics.activity = RunEditorActivityDiagnostics().state;
        diagnostics.state = MakeDefaultEditorDiagnosticsInteractionState();

        EditorPanelModelFrame panelFrame{};
        EditorConsoleEntry console{};
        console.severity = EditorConsoleSeverity::Warning;
        console.channel = "Editor";
        console.message = "diagnostics warning row";
        console.frame = 10;
        panelFrame.console.entries.push_back(console);

        EditorMetric metric{};
        metric.key = "frame.ms";
        metric.label = "Frame";
        metric.value = 19.0;
        metric.unit = EditorMetricUnit::Milliseconds;
        metric.warning = true;
        panelFrame.diagnostics.metrics.push_back(metric);

        EditorDiagnosticsPanelBuildInput build{};
        build.activity = &diagnostics.activity;
        build.panelFrame = &panelFrame;
        build.nowSeconds = 42.0;
        diagnostics.panel = BuildEditorDiagnosticsPanelState(build, BuildEditorDiagnosticsPanelFilterFromInteraction(diagnostics.state));
        SyncEditorDiagnosticsInteractionFromPanel(diagnostics.state, diagnostics.panel);

        diagnostics.toggleResult = ApplyEditorDiagnosticsInteraction(diagnostics.state,
                                                                     &diagnostics.activity,
                                                                     diagnostics.panel,
                                                                     MakeEditorDiagnosticsBadgeToggle(EditorDiagnosticsViewBadgeKind::Workspace));
        diagnostics.toggleOk = diagnostics.toggleResult.handled && diagnostics.toggleResult.filterChanged && !diagnostics.state.filter.showWorkspaceRows;

        diagnostics.searchResult = ApplyEditorDiagnosticsInteraction(diagnostics.state,
                                                                     &diagnostics.activity,
                                                                     diagnostics.panel,
                                                                     MakeEditorDiagnosticsSearchChange("editor"));
        diagnostics.searchOk = diagnostics.searchResult.handled && diagnostics.searchResult.filterChanged && diagnostics.state.searchQuery == "editor";

        diagnostics.panel = BuildEditorDiagnosticsPanelState(build, BuildEditorDiagnosticsPanelFilterFromInteraction(diagnostics.state));
        SyncEditorDiagnosticsInteractionFromPanel(diagnostics.state, diagnostics.panel);
        diagnostics.selectResult = ApplyEditorDiagnosticsInteraction(diagnostics.state,
                                                                     &diagnostics.activity,
                                                                     diagnostics.panel,
                                                                     MakeEditorDiagnosticsRowSelect(0));
        diagnostics.selectOk = diagnostics.selectResult.handled && diagnostics.state.selectedRowId != 0;

        diagnostics.markReadResult = ApplyEditorDiagnosticsInteraction(diagnostics.state,
                                                                       &diagnostics.activity,
                                                                       diagnostics.panel,
                                                                       []{ EditorDiagnosticsInteractionInput input{}; input.action = EditorDiagnosticsInteractionAction::MarkAllRead; input.target = EditorDiagnosticsInteractionTarget::ToolbarAction; return input; }());
        diagnostics.markReadOk = diagnostics.markReadResult.handled && diagnostics.markReadResult.notificationsMarkedRead > 0;

        diagnostics.dismissResult = ApplyEditorDiagnosticsInteraction(diagnostics.state,
                                                                     &diagnostics.activity,
                                                                     diagnostics.panel,
                                                                     []{ EditorDiagnosticsInteractionInput input{}; input.action = EditorDiagnosticsInteractionAction::DismissSelectedNotification; input.target = EditorDiagnosticsInteractionTarget::ToolbarAction; return input; }());
        diagnostics.dismissOk = diagnostics.dismissResult.handled;

        diagnostics.clearTasksResult = ApplyEditorDiagnosticsInteraction(diagnostics.state,
                                                                        &diagnostics.activity,
                                                                        diagnostics.panel,
                                                                        []{ EditorDiagnosticsInteractionInput input{}; input.action = EditorDiagnosticsInteractionAction::ClearCompletedTasks; input.target = EditorDiagnosticsInteractionTarget::ToolbarAction; return input; }());
        diagnostics.clearTasksOk = diagnostics.clearTasksResult.handled && diagnostics.clearTasksResult.completedTasksCleared >= 1;

        diagnostics.panel = BuildEditorDiagnosticsPanelState(build, BuildEditorDiagnosticsPanelFilterFromInteraction(diagnostics.state));
        EditorDiagnosticsViewBuildInput viewInput{};
        viewInput.panelState = &diagnostics.panel;
        viewInput.viewportWidth = 900;
        viewInput.viewportHeight = 180;
        viewInput.scrollOffsetPixels = diagnostics.state.scrollOffsetPixels;
        diagnostics.view = BuildEditorDiagnosticsViewState(viewInput);
        diagnostics.rebuildOk = diagnostics.view.ok && diagnostics.view.filter.textFilter == diagnostics.state.searchQuery;

        diagnostics.ok = ValidateEditorDiagnosticsInteractionState(diagnostics.state)
            && diagnostics.toggleOk
            && diagnostics.searchOk
            && diagnostics.selectOk
            && diagnostics.markReadOk
            && diagnostics.dismissOk
            && diagnostics.clearTasksOk
            && diagnostics.rebuildOk;

        std::ostringstream out;
        out << "editor-diagnostics-interaction-probe toggle=" << (diagnostics.toggleOk ? 1 : 0)
            << " search=" << (diagnostics.searchOk ? 1 : 0)
            << " select=" << (diagnostics.selectOk ? 1 : 0)
            << " markRead=" << diagnostics.markReadResult.notificationsMarkedRead
            << " dismiss=" << diagnostics.dismissResult.notificationsDismissed
            << " clearTasks=" << diagnostics.clearTasksResult.completedTasksCleared
            << " rows=" << diagnostics.panel.rows.size()
            << " ok=" << (diagnostics.ok ? 1 : 0);
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string BuildEditorDiagnosticsInteractionProbeSummary()
    {
        return RunEditorDiagnosticsInteractionDiagnostics().summary;
    }
}
