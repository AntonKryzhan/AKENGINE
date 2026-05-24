#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorActivity.hpp>
#include <AK/EditorUI/EditorDiagnosticsPanel.hpp>
#include <AK/EditorUI/EditorDiagnosticsView.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorDiagnosticsInteractionAction
    {
        None,
        ToggleUnread,
        ToggleWarnings,
        ToggleErrors,
        ToggleTasks,
        ToggleAutosave,
        ToggleWorkspace,
        ToggleCommands,
        ToggleCompletedTasks,
        ToggleReadNotifications,
        SetSearchQuery,
        ClearSearchQuery,
        SelectRow,
        MarkAllRead,
        DismissSelectedNotification,
        ClearCompletedTasks,
        InvokeSelectedCommand,
        ScrollRows,
        ResetFilters
    };

    enum class EditorDiagnosticsInteractionTarget
    {
        None,
        Badge,
        SearchBox,
        Row,
        ToolbarAction,
        ListViewport
    };

    struct EditorDiagnosticsInteractionState
    {
        EditorDiagnosticsPanelFilter filter{};
        std::string searchQuery;
        usize selectedRowIndex = 0;
        i32 scrollOffsetPixels = 0;
        u64 selectedRowId = 0;
        u64 revision = 1;
        bool filterDirty = false;
        bool selectionDirty = false;
        bool activityDirty = false;
        bool scrollDirty = false;
        bool commandQueued = false;
        std::string statusText;
    };

    struct EditorDiagnosticsInteractionInput
    {
        EditorDiagnosticsInteractionAction action = EditorDiagnosticsInteractionAction::None;
        EditorDiagnosticsInteractionTarget target = EditorDiagnosticsInteractionTarget::None;
        EditorDiagnosticsViewBadgeKind badge = EditorDiagnosticsViewBadgeKind::All;
        usize rowIndex = 0;
        i32 wheelDelta = 0;
        i32 rowHeightPixels = 48;
        i32 viewportHeightPixels = 0;
        i32 contentHeightPixels = 0;
        std::string text;
        bool appendText = false;
        bool shiftDown = false;
        bool ctrlDown = false;
    };

    struct EditorDiagnosticsInteractionResult
    {
        bool handled = false;
        bool filterChanged = false;
        bool selectionChanged = false;
        bool activityChanged = false;
        bool scrollChanged = false;
        bool commandQueued = false;
        u32 notificationsMarkedRead = 0;
        u32 notificationsDismissed = 0;
        u32 completedTasksCleared = 0;
        CommandInvocation command{};
        std::string statusText;
    };

    struct EditorDiagnosticsInteractionDiagnostics
    {
        EditorDiagnosticsInteractionState state{};
        EditorDiagnosticsPanelState panel{};
        EditorDiagnosticsViewState view{};
        EditorActivityCenterState activity{};
        EditorDiagnosticsInteractionResult toggleResult{};
        EditorDiagnosticsInteractionResult searchResult{};
        EditorDiagnosticsInteractionResult selectResult{};
        EditorDiagnosticsInteractionResult markReadResult{};
        EditorDiagnosticsInteractionResult dismissResult{};
        EditorDiagnosticsInteractionResult clearTasksResult{};
        bool toggleOk = false;
        bool searchOk = false;
        bool selectOk = false;
        bool markReadOk = false;
        bool dismissOk = false;
        bool clearTasksOk = false;
        bool rebuildOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorDiagnosticsInteractionAction action);
    const char* ToString(EditorDiagnosticsInteractionTarget target);

    EditorDiagnosticsInteractionState MakeDefaultEditorDiagnosticsInteractionState();
    EditorDiagnosticsPanelFilter BuildEditorDiagnosticsPanelFilterFromInteraction(const EditorDiagnosticsInteractionState& state);
    void SyncEditorDiagnosticsInteractionFromPanel(EditorDiagnosticsInteractionState& state, const EditorDiagnosticsPanelState& panel);

    EditorDiagnosticsInteractionResult ApplyEditorDiagnosticsInteraction(EditorDiagnosticsInteractionState& state,
                                                                         EditorActivityCenterState* activity,
                                                                         const EditorDiagnosticsPanelState& panel,
                                                                         const EditorDiagnosticsInteractionInput& input);

    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsBadgeToggle(EditorDiagnosticsViewBadgeKind badge);
    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsSearchChange(std::string query);
    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsRowSelect(usize rowIndex);
    EditorDiagnosticsInteractionInput MakeEditorDiagnosticsScroll(i32 wheelDelta, i32 rowHeightPixels, i32 viewportHeightPixels, i32 contentHeightPixels);

    bool ValidateEditorDiagnosticsInteractionState(const EditorDiagnosticsInteractionState& state);
    std::string FormatEditorDiagnosticsInteractionState(const EditorDiagnosticsInteractionState& state);
    std::string FormatEditorDiagnosticsInteractionResult(const EditorDiagnosticsInteractionResult& result);

    EditorDiagnosticsInteractionDiagnostics RunEditorDiagnosticsInteractionDiagnostics();
    std::string BuildEditorDiagnosticsInteractionProbeSummary();
}
