#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorActivity.hpp>
#include <AK/EditorUI/EditorAutosave.hpp>
#include <AK/EditorUI/EditorCommandState.hpp>
#include <AK/EditorUI/EditorPanelModels.hpp>
#include <AK/EditorUI/EditorWorkspace.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorDiagnosticsRowKind
    {
        Activity,
        BackgroundTask,
        Console,
        Metric,
        Autosave,
        Workspace,
        CommandState
    };

    enum class EditorDiagnosticsRowSeverity
    {
        Trace,
        Info,
        Success,
        Warning,
        Error
    };

    struct EditorDiagnosticsPanelFilter
    {
        bool showTrace = true;
        bool showInfo = true;
        bool showSuccess = true;
        bool showWarnings = true;
        bool showErrors = true;
        bool showReadNotifications = true;
        bool showCompletedTasks = true;
        bool showCommandRows = true;
        bool showWorkspaceRows = true;
        bool showAutosaveRows = true;
        bool showMetrics = true;
        std::string textFilter;
        usize maxRows = 256;
    };

    struct EditorDiagnosticsPanelRow
    {
        u64 id = 0;
        EditorDiagnosticsRowKind kind = EditorDiagnosticsRowKind::Console;
        EditorDiagnosticsRowSeverity severity = EditorDiagnosticsRowSeverity::Info;
        std::string source;
        std::string category;
        std::string title;
        std::string message;
        std::string detail;
        double timeSeconds = 0.0;
        u32 repeatCount = 1;
        bool unread = false;
        bool sticky = false;
        bool active = false;
        bool selected = false;
        CommandId command = CommandId::Count;
        u64 stableHash = 0;
    };

    struct EditorDiagnosticsPanelMetric
    {
        std::string key;
        std::string label;
        std::string source;
        double value = 0.0;
        EditorMetricUnit unit = EditorMetricUnit::Count;
        EditorDiagnosticsRowSeverity severity = EditorDiagnosticsRowSeverity::Info;
        bool pinned = false;
    };

    struct EditorDiagnosticsPanelState
    {
        EditorDiagnosticsPanelFilter filter{};
        std::vector<EditorDiagnosticsPanelRow> rows;
        std::vector<EditorDiagnosticsPanelMetric> metrics;
        std::size_t selectedRowIndex = 0;
        u32 unreadCount = 0;
        u32 warningCount = 0;
        u32 errorCount = 0;
        u32 activeTaskCount = 0;
        u32 stickyCount = 0;
        u64 revision = 1;
        std::string statusText;
    };

    struct EditorDiagnosticsPanelBuildInput
    {
        const EditorActivityCenterState* activity = nullptr;
        const EditorPanelModelFrame* panelFrame = nullptr;
        const EditorAutosaveRuntimeState* autosaveState = nullptr;
        const EditorAutosavePolicy* autosavePolicy = nullptr;
        const EditorWorkspaceLoadResult* workspaceLoad = nullptr;
        const EditorWorkspaceReport* workspaceReport = nullptr;
        const EditorCommandStateCache* commandStates = nullptr;
        double nowSeconds = 0.0;
    };

    struct EditorDiagnosticsConsoleSyncResult
    {
        usize entriesAdded = 0;
        usize infoEntries = 0;
        usize warningEntries = 0;
        usize errorEntries = 0;
        bool trimmed = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorDiagnosticsPanelDiagnostics
    {
        EditorDiagnosticsPanelState state{};
        EditorDiagnosticsConsoleSyncResult consoleSync{};
        usize activityRows = 0;
        usize taskRows = 0;
        usize consoleRows = 0;
        usize metricRows = 0;
        usize commandRows = 0;
        usize workspaceRows = 0;
        usize autosaveRows = 0;
        bool filterOk = false;
        bool countersOk = false;
        bool frameApplyOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorDiagnosticsRowKind kind);
    const char* ToString(EditorDiagnosticsRowSeverity severity);

    EditorDiagnosticsPanelFilter MakeDefaultEditorDiagnosticsPanelFilter();
    EditorDiagnosticsPanelState BuildEditorDiagnosticsPanelState(const EditorDiagnosticsPanelBuildInput& input,
                                                                 const EditorDiagnosticsPanelFilter& filter = MakeDefaultEditorDiagnosticsPanelFilter());

    bool ShouldShowEditorDiagnosticsRow(const EditorDiagnosticsPanelRow& row, const EditorDiagnosticsPanelFilter& filter);
    void SortEditorDiagnosticsPanelRows(EditorDiagnosticsPanelState& state);
    void RecomputeEditorDiagnosticsPanelState(EditorDiagnosticsPanelState& state);

    EditorDiagnosticsConsoleSyncResult SyncEditorDiagnosticsPanelToConsole(EditorConsoleModel& console,
                                                                           const EditorDiagnosticsPanelState& state,
                                                                           usize maxConsoleEntries = 256);

    bool ApplyEditorDiagnosticsPanelToFrame(EditorPanelModelFrame& frame, const EditorDiagnosticsPanelState& state, usize maxRecentEvents = 32);

    std::string FormatEditorDiagnosticsPanelRow(const EditorDiagnosticsPanelRow& row);
    std::string FormatEditorDiagnosticsPanelMetric(const EditorDiagnosticsPanelMetric& metric);
    std::string FormatEditorDiagnosticsPanelState(const EditorDiagnosticsPanelState& state);
    std::string FormatEditorDiagnosticsConsoleSyncResult(const EditorDiagnosticsConsoleSyncResult& result);

    EditorDiagnosticsPanelDiagnostics RunEditorDiagnosticsPanelDiagnostics();
    std::string BuildEditorDiagnosticsPanelProbeSummary();
}
