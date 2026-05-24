#include <AK/EditorUI/EditorDiagnosticsView.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        u64 HashAppend(u64 hash, std::string_view text)
        {
            for (char c : text)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        u64 HashAppendU64(u64 hash, u64 value)
        {
            for (int i = 0; i < 8; ++i)
            {
                hash ^= static_cast<unsigned char>((value >> (i * 8)) & 0xffu);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        u64 HashView(const EditorDiagnosticsViewState& view)
        {
            u64 hash = 1469598103934665603ull;
            hash = HashAppendU64(hash, static_cast<u64>(view.rows.size()));
            hash = HashAppendU64(hash, static_cast<u64>(view.metrics.size()));
            hash = HashAppendU64(hash, static_cast<u64>(view.badges.size()));
            for (const EditorDiagnosticsViewRowSurface& row : view.rows)
            {
                hash = HashAppendU64(hash, row.id);
                hash = HashAppend(hash, row.title);
                hash = HashAppend(hash, row.subtitle);
                hash = HashAppend(hash, row.detail);
            }
            for (const EditorDiagnosticsViewMetricTile& metric : view.metrics)
            {
                hash = HashAppend(hash, metric.key);
                hash = HashAppend(hash, metric.valueText);
            }
            return hash;
        }

        std::string MetricValueText(const EditorDiagnosticsPanelMetric& metric)
        {
            return FormatEditorDiagnosticsPanelMetric(metric);
        }

        void PushBadge(EditorDiagnosticsViewState& view,
                       EditorDiagnosticsViewBadgeKind kind,
                       std::string id,
                       std::string label,
                       u32 value,
                       bool active,
                       bool warning = false,
                       bool error = false)
        {
            EditorDiagnosticsViewBadge badge{};
            badge.kind = kind;
            badge.id = std::move(id);
            badge.label = std::move(label);
            badge.value = value;
            badge.active = active;
            badge.warning = warning;
            badge.error = error;
            view.badges.push_back(std::move(badge));
        }

        EditorDiagnosticsViewRowSurface BuildRowSurface(const EditorDiagnosticsPanelRow& row, bool selected, bool compact)
        {
            EditorDiagnosticsViewRowSurface surface{};
            surface.id = row.id;
            surface.kind = row.kind;
            surface.severity = row.severity;
            surface.icon = IconForEditorDiagnosticsRow(row.kind, row.severity);
            surface.sourceLabel = row.source.empty() ? ToString(row.kind) : row.source;
            surface.title = row.title;
            surface.subtitle = BuildEditorDiagnosticsRowSubtitle(row);
            surface.detail = row.detail;
            surface.badges = BuildEditorDiagnosticsRowBadges(row);
            surface.unread = row.unread;
            surface.sticky = row.sticky;
            surface.active = row.active;
            surface.selected = selected;
            surface.rowHeightPixels = compact ? 34u : (row.detail.empty() ? 42u : 52u);
            surface.stableHash = row.stableHash;
            return surface;
        }

        usize CountRowsOfKind(const EditorDiagnosticsPanelState& state, EditorDiagnosticsRowKind kind)
        {
            usize count = 0;
            for (const EditorDiagnosticsPanelRow& row : state.rows)
            {
                if (row.kind == kind)
                {
                    ++count;
                }
            }
            return count;
        }
    }

    const char* ToString(EditorDiagnosticsViewBadgeKind kind)
    {
        switch (kind)
        {
            case EditorDiagnosticsViewBadgeKind::All: return "all";
            case EditorDiagnosticsViewBadgeKind::Unread: return "unread";
            case EditorDiagnosticsViewBadgeKind::Warnings: return "warnings";
            case EditorDiagnosticsViewBadgeKind::Errors: return "errors";
            case EditorDiagnosticsViewBadgeKind::Tasks: return "tasks";
            case EditorDiagnosticsViewBadgeKind::Autosave: return "autosave";
            case EditorDiagnosticsViewBadgeKind::Workspace: return "workspace";
            case EditorDiagnosticsViewBadgeKind::Commands: return "commands";
            case EditorDiagnosticsViewBadgeKind::Search: return "search";
            default: return "unknown";
        }
    }

    std::string IconForEditorDiagnosticsRow(EditorDiagnosticsRowKind kind, EditorDiagnosticsRowSeverity severity)
    {
        if (severity == EditorDiagnosticsRowSeverity::Error)
        {
            return "!";
        }
        if (severity == EditorDiagnosticsRowSeverity::Warning)
        {
            return "^";
        }
        if (severity == EditorDiagnosticsRowSeverity::Success)
        {
            return "+";
        }

        switch (kind)
        {
            case EditorDiagnosticsRowKind::Activity: return "A";
            case EditorDiagnosticsRowKind::BackgroundTask: return "T";
            case EditorDiagnosticsRowKind::Console: return "C";
            case EditorDiagnosticsRowKind::Metric: return "M";
            case EditorDiagnosticsRowKind::Autosave: return "S";
            case EditorDiagnosticsRowKind::Workspace: return "W";
            case EditorDiagnosticsRowKind::CommandState: return "K";
            default: return "?";
        }
    }

    std::string BuildEditorDiagnosticsRowSubtitle(const EditorDiagnosticsPanelRow& row)
    {
        std::ostringstream out;
        out << row.source;
        if (!row.category.empty())
        {
            out << " / " << row.category;
        }
        out << " / " << ToString(row.kind) << " / " << ToString(row.severity);
        if (row.timeSeconds > 0.0)
        {
            out << " / t=" << static_cast<int>(row.timeSeconds) << "s";
        }
        return out.str();
    }

    std::string BuildEditorDiagnosticsRowBadges(const EditorDiagnosticsPanelRow& row)
    {
        std::ostringstream out;
        bool first = true;
        auto push = [&](std::string_view text)
        {
            if (!first)
            {
                out << " ";
            }
            out << "[" << text << "]";
            first = false;
        };

        if (row.unread)
        {
            push("unread");
        }
        if (row.sticky)
        {
            push("sticky");
        }
        if (row.active)
        {
            push("active");
        }
        if (row.repeatCount > 1)
        {
            push("x" + std::to_string(row.repeatCount));
        }
        return out.str();
    }

    EditorDiagnosticsViewState BuildEditorDiagnosticsViewState(const EditorDiagnosticsViewBuildInput& input)
    {
        EditorDiagnosticsViewState view{};
        view.viewportHeightPixels = std::max<i32>(0, input.viewportHeight);
        view.scrollOffsetPixels = std::max<i32>(0, input.scrollOffsetPixels);

        if (!input.panelState)
        {
            view.summary = "editor-diagnostics-view missing panel state";
            return view;
        }

        const EditorDiagnosticsPanelState& panel = *input.panelState;
        view.filter = panel.filter;
        view.selectedRowIndex = panel.selectedRowIndex;
        view.unreadCount = panel.unreadCount;
        view.warningCount = panel.warningCount;
        view.errorCount = panel.errorCount;
        view.activeTaskCount = panel.activeTaskCount;
        view.stickyCount = panel.stickyCount;
        view.revision = panel.revision;

        PushBadge(view, EditorDiagnosticsViewBadgeKind::All, "all", "All", static_cast<u32>(panel.rows.size()), true);
        PushBadge(view, EditorDiagnosticsViewBadgeKind::Unread, "unread", "Unread", panel.unreadCount, panel.unreadCount > 0, panel.unreadCount > 0);
        PushBadge(view, EditorDiagnosticsViewBadgeKind::Warnings, "warnings", "Warnings", panel.warningCount, panel.warningCount > 0, panel.warningCount > 0);
        PushBadge(view, EditorDiagnosticsViewBadgeKind::Errors, "errors", "Errors", panel.errorCount, panel.errorCount > 0, false, panel.errorCount > 0);
        PushBadge(view, EditorDiagnosticsViewBadgeKind::Tasks, "tasks", "Tasks", panel.activeTaskCount, panel.activeTaskCount > 0);
        PushBadge(view, EditorDiagnosticsViewBadgeKind::Autosave, "autosave", "Autosave", static_cast<u32>(CountRowsOfKind(panel, EditorDiagnosticsRowKind::Autosave)), panel.filter.showAutosaveRows);
        PushBadge(view, EditorDiagnosticsViewBadgeKind::Workspace, "workspace", "Workspace", static_cast<u32>(CountRowsOfKind(panel, EditorDiagnosticsRowKind::Workspace)), panel.filter.showWorkspaceRows);
        PushBadge(view, EditorDiagnosticsViewBadgeKind::Commands, "commands", "Commands", static_cast<u32>(CountRowsOfKind(panel, EditorDiagnosticsRowKind::CommandState)), panel.filter.showCommandRows);

        std::vector<EditorDiagnosticsPanelMetric> metrics = panel.metrics;
        std::stable_sort(metrics.begin(), metrics.end(), [](const EditorDiagnosticsPanelMetric& a, const EditorDiagnosticsPanelMetric& b)
        {
            if (a.pinned != b.pinned)
            {
                return a.pinned && !b.pinned;
            }
            if (a.severity != b.severity)
            {
                return static_cast<int>(a.severity) > static_cast<int>(b.severity);
            }
            return a.key < b.key;
        });

        const usize metricLimit = std::min<usize>(metrics.size(), input.maxMetricTiles);
        for (usize i = 0; i < metricLimit; ++i)
        {
            const EditorDiagnosticsPanelMetric& metric = metrics[i];
            EditorDiagnosticsViewMetricTile tile{};
            tile.key = metric.key;
            tile.label = metric.label;
            tile.valueText = MetricValueText(metric);
            tile.source = metric.source;
            tile.severity = metric.severity;
            tile.pinned = metric.pinned;
            view.metrics.push_back(std::move(tile));
        }

        view.rows.reserve(panel.rows.size());
        i32 contentHeight = 0;
        for (usize i = 0; i < panel.rows.size(); ++i)
        {
            EditorDiagnosticsViewRowSurface surface = BuildRowSurface(panel.rows[i], i == panel.selectedRowIndex, input.compactRows || input.viewportWidth < 620);
            contentHeight += static_cast<i32>(surface.rowHeightPixels);
            view.rows.push_back(std::move(surface));
        }
        view.contentHeightPixels = contentHeight;

        if (view.contentHeightPixels <= view.viewportHeightPixels)
        {
            view.scrollOffsetPixels = 0;
        }
        else
        {
            view.scrollOffsetPixels = std::min<i32>(view.scrollOffsetPixels, view.contentHeightPixels - view.viewportHeightPixels);
        }

        i32 y = 0;
        view.firstVisibleRow = 0;
        for (usize i = 0; i < view.rows.size(); ++i)
        {
            const i32 next = y + static_cast<i32>(view.rows[i].rowHeightPixels);
            if (next > view.scrollOffsetPixels)
            {
                view.firstVisibleRow = i;
                break;
            }
            y = next;
        }

        i32 visiblePixels = 0;
        view.visibleRowCount = 0;
        for (usize i = view.firstVisibleRow; i < view.rows.size(); ++i)
        {
            visiblePixels += static_cast<i32>(view.rows[i].rowHeightPixels);
            ++view.visibleRowCount;
            if (visiblePixels >= view.viewportHeightPixels + 64)
            {
                break;
            }
        }

        view.stableHash = HashView(view);
        view.ok = ValidateEditorDiagnosticsViewState(view);
        view.summary = FormatEditorDiagnosticsViewState(view);
        return view;
    }

    bool ValidateEditorDiagnosticsViewState(const EditorDiagnosticsViewState& view)
    {
        if (view.badges.empty())
        {
            return false;
        }
        if (view.viewportHeightPixels < 0 || view.contentHeightPixels < 0 || view.scrollOffsetPixels < 0)
        {
            return false;
        }
        if (view.firstVisibleRow > view.rows.size())
        {
            return false;
        }
        if (view.visibleRowCount > view.rows.size())
        {
            return false;
        }
        for (const EditorDiagnosticsViewRowSurface& row : view.rows)
        {
            if (row.id == 0 || row.title.empty() || row.rowHeightPixels == 0)
            {
                return false;
            }
        }
        for (const EditorDiagnosticsViewMetricTile& metric : view.metrics)
        {
            if (metric.key.empty() || metric.label.empty())
            {
                return false;
            }
        }
        return true;
    }

    std::string FormatEditorDiagnosticsViewState(const EditorDiagnosticsViewState& view)
    {
        std::ostringstream out;
        out << "editor-diagnostics-view rows=" << view.rows.size()
            << " visible=" << view.visibleRowCount
            << " first=" << view.firstVisibleRow
            << " metrics=" << view.metrics.size()
            << " badges=" << view.badges.size()
            << " unread=" << view.unreadCount
            << " warnings=" << view.warningCount
            << " errors=" << view.errorCount
            << " scroll=" << view.scrollOffsetPixels
            << " ok=" << (view.ok ? 1 : 0);
        return out.str();
    }

    EditorDiagnosticsViewDiagnostics RunEditorDiagnosticsViewDiagnostics()
    {
        EditorDiagnosticsPanelDiagnostics panelDiagnostics = RunEditorDiagnosticsPanelDiagnostics();
        EditorDiagnosticsViewBuildInput input{};
        input.panelState = &panelDiagnostics.state;
        input.viewportWidth = 840;
        input.viewportHeight = 188;
        input.scrollOffsetPixels = 44;
        input.maxMetricTiles = 5;

        EditorDiagnosticsViewDiagnostics diagnostics{};
        diagnostics.view = BuildEditorDiagnosticsViewState(input);
        diagnostics.badgesOk = diagnostics.view.badges.size() >= 6 && diagnostics.view.badges[0].value == diagnostics.view.rows.size();
        diagnostics.rowsOk = !diagnostics.view.rows.empty() && diagnostics.view.rows[0].id != 0 && !diagnostics.view.rows[0].icon.empty();
        diagnostics.metricsOk = !diagnostics.view.metrics.empty() && diagnostics.view.metrics.size() <= input.maxMetricTiles;
        diagnostics.virtualizationOk = diagnostics.view.firstVisibleRow < diagnostics.view.rows.size()
            && diagnostics.view.visibleRowCount > 0
            && diagnostics.view.visibleRowCount <= diagnostics.view.rows.size();
        const EditorDiagnosticsViewState second = BuildEditorDiagnosticsViewState(input);
        diagnostics.hashOk = diagnostics.view.stableHash != 0 && diagnostics.view.stableHash == second.stableHash;
        diagnostics.ok = diagnostics.view.ok && diagnostics.badgesOk && diagnostics.rowsOk && diagnostics.metricsOk && diagnostics.virtualizationOk && diagnostics.hashOk;

        std::ostringstream out;
        out << "editor-diagnostics-view-probe rows=" << diagnostics.view.rows.size()
            << " visible=" << diagnostics.view.visibleRowCount
            << " metrics=" << diagnostics.view.metrics.size()
            << " badges=" << diagnostics.view.badges.size()
            << " hash=0x" << std::hex << diagnostics.view.stableHash << std::dec
            << " ok=" << (diagnostics.ok ? 1 : 0);
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string BuildEditorDiagnosticsViewProbeSummary()
    {
        return RunEditorDiagnosticsViewDiagnostics().summary;
    }
}
