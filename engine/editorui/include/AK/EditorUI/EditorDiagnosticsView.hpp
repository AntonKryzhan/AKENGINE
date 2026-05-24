#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDiagnosticsPanel.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorDiagnosticsViewBadgeKind
    {
        All,
        Unread,
        Warnings,
        Errors,
        Tasks,
        Autosave,
        Workspace,
        Commands,
        Search
    };

    struct EditorDiagnosticsViewBadge
    {
        EditorDiagnosticsViewBadgeKind kind = EditorDiagnosticsViewBadgeKind::All;
        std::string id;
        std::string label;
        u32 value = 0;
        bool active = false;
        bool warning = false;
        bool error = false;
    };

    struct EditorDiagnosticsViewMetricTile
    {
        std::string key;
        std::string label;
        std::string valueText;
        std::string source;
        EditorDiagnosticsRowSeverity severity = EditorDiagnosticsRowSeverity::Info;
        bool pinned = false;
    };

    struct EditorDiagnosticsViewRowSurface
    {
        u64 id = 0;
        EditorDiagnosticsRowKind kind = EditorDiagnosticsRowKind::Console;
        EditorDiagnosticsRowSeverity severity = EditorDiagnosticsRowSeverity::Info;
        std::string icon;
        std::string sourceLabel;
        std::string title;
        std::string subtitle;
        std::string detail;
        std::string badges;
        bool unread = false;
        bool sticky = false;
        bool active = false;
        bool selected = false;
        u32 rowHeightPixels = 44;
        u64 stableHash = 0;
    };

    struct EditorDiagnosticsViewBuildInput
    {
        const EditorDiagnosticsPanelState* panelState = nullptr;
        i32 viewportWidth = 0;
        i32 viewportHeight = 0;
        i32 scrollOffsetPixels = 0;
        usize maxMetricTiles = 6;
        bool compactRows = false;
    };

    struct EditorDiagnosticsViewState
    {
        EditorDiagnosticsPanelFilter filter{};
        std::vector<EditorDiagnosticsViewBadge> badges;
        std::vector<EditorDiagnosticsViewMetricTile> metrics;
        std::vector<EditorDiagnosticsViewRowSurface> rows;
        usize selectedRowIndex = 0;
        usize firstVisibleRow = 0;
        usize visibleRowCount = 0;
        i32 scrollOffsetPixels = 0;
        i32 contentHeightPixels = 0;
        i32 viewportHeightPixels = 0;
        u32 unreadCount = 0;
        u32 warningCount = 0;
        u32 errorCount = 0;
        u32 activeTaskCount = 0;
        u32 stickyCount = 0;
        u64 stableHash = 0;
        u64 revision = 1;
        bool ok = false;
        std::string summary;
    };

    struct EditorDiagnosticsViewDiagnostics
    {
        EditorDiagnosticsViewState view{};
        bool badgesOk = false;
        bool rowsOk = false;
        bool metricsOk = false;
        bool virtualizationOk = false;
        bool hashOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorDiagnosticsViewBadgeKind kind);
    std::string IconForEditorDiagnosticsRow(EditorDiagnosticsRowKind kind, EditorDiagnosticsRowSeverity severity);
    std::string BuildEditorDiagnosticsRowSubtitle(const EditorDiagnosticsPanelRow& row);
    std::string BuildEditorDiagnosticsRowBadges(const EditorDiagnosticsPanelRow& row);

    EditorDiagnosticsViewState BuildEditorDiagnosticsViewState(const EditorDiagnosticsViewBuildInput& input);
    bool ValidateEditorDiagnosticsViewState(const EditorDiagnosticsViewState& view);
    std::string FormatEditorDiagnosticsViewState(const EditorDiagnosticsViewState& view);

    EditorDiagnosticsViewDiagnostics RunEditorDiagnosticsViewDiagnostics();
    std::string BuildEditorDiagnosticsViewProbeSummary();
}
