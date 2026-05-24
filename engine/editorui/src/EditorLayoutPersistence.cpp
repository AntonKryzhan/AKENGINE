#include <AK/EditorUI/EditorLayoutPersistence.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_set>

namespace AK
{
    namespace
    {
        constexpr i32 DefaultMenuHeight = 22;
        constexpr i32 DefaultToolbarHeight = 38;
        constexpr i32 DefaultStatusHeight = 22;
        constexpr i32 DefaultDockGap = 4;

        void AddIssue(EditorLayoutRepairReport& report, EditorLayoutRepairCode code, EditorLayoutRepairSeverity severity, std::string message, EditorDockNodeId node = {}, EditorPanelId panel = {})
        {
            EditorLayoutRepairIssue issue{};
            issue.code = code;
            issue.severity = severity;
            issue.node = node;
            issue.panel = panel;
            issue.message = std::move(message);
            report.issues.push_back(std::move(issue));
            if (code != EditorLayoutRepairCode::None)
            {
                report.repaired = true;
            }
        }

        i32 VisibleArea(EditorRect a, EditorRect b)
        {
            const i32 x0 = std::max(a.x, b.x);
            const i32 y0 = std::max(a.y, b.y);
            const i32 x1 = std::min(a.x + a.width, b.x + b.width);
            const i32 y1 = std::min(a.y + a.height, b.y + b.height);
            return std::max(0, x1 - x0) * std::max(0, y1 - y0);
        }

        bool IsFiniteWeight(float value)
        {
            return std::isfinite(value);
        }

        void RebuildFrameRects(EditorFrameLayout& frame, i32 minWidth, i32 minHeight)
        {
            frame.width = std::max(minWidth, frame.width);
            frame.height = std::max(minHeight, frame.height);
            frame.menuBar = {0, 0, frame.width, DefaultMenuHeight};
            frame.toolbar = {0, DefaultMenuHeight, frame.width, DefaultToolbarHeight};
            frame.statusBar = {0, frame.height - DefaultStatusHeight, frame.width, DefaultStatusHeight};
            frame.dockSpace = {
                DefaultDockGap,
                DefaultMenuHeight + DefaultToolbarHeight + DefaultDockGap,
                std::max(0, frame.width - 2 * DefaultDockGap),
                std::max(0, frame.height - DefaultMenuHeight - DefaultToolbarHeight - DefaultStatusHeight - 2 * DefaultDockGap)
            };
        }

        EditorFrameLayout ResetToDefault(EditorFrameLayout& frame, const EditorPanelRegistry& panels, const EditorLayoutPersistencePolicy& policy, EditorLayoutRepairReport& report, std::string reason)
        {
            AddIssue(report, EditorLayoutRepairCode::ResetToDefault, EditorLayoutRepairSeverity::Error, std::move(reason));
            report.resetToDefault = true;
            frame = BuildDefaultEditorFrameLayout(panels, std::max(policy.minWindowWidth, frame.width), std::max(policy.minWindowHeight, frame.height));
            return frame;
        }

        bool HasCorePanels(const EditorFrameLayout& frame, const EditorPanelRegistry& panels)
        {
            const char* required[] = {"scene.hierarchy", "scene.viewport", "inspector", "assets.browser", "console"};
            for (const char* panelName : required)
            {
                const EditorPanelDesc* panel = panels.FindByName(panelName);
                if (!panel)
                {
                    return false;
                }

                bool found = false;
                for (const EditorDockNode& node : frame.dock.nodes)
                {
                    for (const EditorDockTab& tab : node.tabs)
                    {
                        if (tab.panel == panel->id)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (found)
                    {
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }
            }
            return true;
        }

        bool ParseEditorLayoutDocument(std::string_view text, std::string& dockText, EditorWindowPlacement& placement, bool& legacy)
        {
            legacy = false;
            if (text.substr(0, 8) == "AKLAYOUT")
            {
                dockText.assign(text.begin(), text.end());
                return true;
            }

            std::istringstream input{std::string(text)};
            std::string magic;
            u32 version = 0;
            input >> magic >> version;
            if (magic != "AKEDITORLAYOUT" || version != 1)
            {
                return false;
            }

            std::string tag;
            input >> tag;
            if (tag == "window")
            {
                input >> placement.rect.x >> placement.rect.y >> placement.rect.width >> placement.rect.height >> placement.monitorIndex >> placement.maximized;
            }
            else
            {
                return false;
            }

            std::string line;
            std::getline(input, line);
            bool inDock = false;
            std::ostringstream dockOut;
            while (std::getline(input, line))
            {
                if (line == "dock_begin")
                {
                    inDock = true;
                    continue;
                }
                if (line == "dock_end")
                {
                    break;
                }
                if (inDock)
                {
                    dockOut << line << '\n';
                }
            }

            dockText = dockOut.str();
            legacy = false;
            return !dockText.empty();
        }
    }

    const char* ToString(EditorLayoutRepairCode code)
    {
        switch (code)
        {
            case EditorLayoutRepairCode::None: return "None";
            case EditorLayoutRepairCode::UnsupportedVersion: return "UnsupportedVersion";
            case EditorLayoutRepairCode::ParseFailed: return "ParseFailed";
            case EditorLayoutRepairCode::MissingRoot: return "MissingRoot";
            case EditorLayoutRepairCode::InvalidSplitReference: return "InvalidSplitReference";
            case EditorLayoutRepairCode::InvalidSplitWeight: return "InvalidSplitWeight";
            case EditorLayoutRepairCode::EmptyStack: return "EmptyStack";
            case EditorLayoutRepairCode::UnknownPanel: return "UnknownPanel";
            case EditorLayoutRepairCode::DuplicatePanel: return "DuplicatePanel";
            case EditorLayoutRepairCode::ActiveTabOutOfRange: return "ActiveTabOutOfRange";
            case EditorLayoutRepairCode::InvalidFrameSize: return "InvalidFrameSize";
            case EditorLayoutRepairCode::InvalidDockSpace: return "InvalidDockSpace";
            case EditorLayoutRepairCode::OffscreenWindow: return "OffscreenWindow";
            case EditorLayoutRepairCode::ClampedWindow: return "ClampedWindow";
            case EditorLayoutRepairCode::ResetToDefault: return "ResetToDefault";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorLayoutRepairSeverity severity)
    {
        switch (severity)
        {
            case EditorLayoutRepairSeverity::Info: return "info";
            case EditorLayoutRepairSeverity::Warning: return "warning";
            case EditorLayoutRepairSeverity::Error: return "error";
            default: return "unknown";
        }
    }

    EditorLayoutPersistencePolicy MakeDefaultEditorLayoutPersistencePolicy()
    {
        return {};
    }

    std::vector<EditorMonitorBounds> MakeDefaultEditorMonitorSet(i32 width, i32 height)
    {
        EditorMonitorBounds monitor{};
        monitor.workArea = {0, 0, std::max(640, width), std::max(360, height)};
        monitor.dpiScale = 1.0f;
        monitor.primary = true;
        monitor.name = "Primary";
        return {monitor};
    }

    EditorWindowPlacement MakeDefaultEditorWindowPlacement(i32 width, i32 height)
    {
        EditorWindowPlacement placement{};
        placement.rect = {80, 80, std::max(900, width), std::max(540, height)};
        placement.monitorIndex = 0;
        placement.maximized = false;
        return placement;
    }

    EditorWindowPlacement ClampEditorWindowPlacementToMonitors(EditorWindowPlacement placement, const std::vector<EditorMonitorBounds>& monitors, const EditorLayoutPersistencePolicy& policy, EditorLayoutRepairReport* report)
    {
        if (!policy.clampWindowToMonitors || monitors.empty())
        {
            return placement;
        }

        placement.rect.width = std::max(policy.minWindowWidth, placement.rect.width);
        placement.rect.height = std::max(policy.minWindowHeight, placement.rect.height);

        i32 bestMonitor = 0;
        i32 bestArea = -1;
        for (std::size_t i = 0; i < monitors.size(); ++i)
        {
            const i32 area = VisibleArea(placement.rect, monitors[i].workArea);
            if (area > bestArea)
            {
                bestArea = area;
                bestMonitor = static_cast<i32>(i);
            }
        }

        if (bestArea < policy.minVisibleWindowPixels * policy.minVisibleWindowPixels)
        {
            const EditorMonitorBounds& monitor = monitors[0];
            placement.rect.x = monitor.workArea.x + 40;
            placement.rect.y = monitor.workArea.y + 40;
            placement.monitorIndex = 0;
            if (report)
            {
                AddIssue(*report, EditorLayoutRepairCode::OffscreenWindow, EditorLayoutRepairSeverity::Warning, "Window placement was outside visible monitor work areas");
            }
        }
        else
        {
            placement.monitorIndex = bestMonitor;
        }

        const EditorRect work = monitors[static_cast<std::size_t>(std::clamp(placement.monitorIndex, 0, static_cast<i32>(monitors.size() - 1)))].workArea;
        const EditorRect before = placement.rect;
        placement.rect.width = std::min(placement.rect.width, work.width);
        placement.rect.height = std::min(placement.rect.height, work.height);
        placement.rect.x = std::clamp(placement.rect.x, work.x, work.x + std::max(0, work.width - placement.rect.width));
        placement.rect.y = std::clamp(placement.rect.y, work.y, work.y + std::max(0, work.height - placement.rect.height));

        if (report && (before.x != placement.rect.x || before.y != placement.rect.y || before.width != placement.rect.width || before.height != placement.rect.height))
        {
            AddIssue(*report, EditorLayoutRepairCode::ClampedWindow, EditorLayoutRepairSeverity::Info, "Window placement was clamped to monitor bounds");
        }
        return placement;
    }

    EditorLayoutRepairReport RepairEditorFrameLayout(EditorFrameLayout& frame, const EditorPanelRegistry& panels, const EditorLayoutPersistencePolicy& policy)
    {
        EditorLayoutRepairReport report{};

        if (frame.width < policy.minWindowWidth || frame.height < policy.minWindowHeight)
        {
            AddIssue(report, EditorLayoutRepairCode::InvalidFrameSize, EditorLayoutRepairSeverity::Warning, "Frame size was below editor minimum");
            frame.width = std::max(frame.width, policy.minWindowWidth);
            frame.height = std::max(frame.height, policy.minWindowHeight);
        }

        if (!frame.dock.root.IsValid() || !FindDockNode(frame.dock, frame.dock.root))
        {
            ResetToDefault(frame, panels, policy, report, "Dock root was missing or referenced an unknown node");
            report.ok = true;
            report.summary = FormatEditorLayoutRepairReport(report);
            return report;
        }

        std::unordered_set<u32> nodeIds;
        bool hardInvalid = false;
        for (EditorDockNode& node : frame.dock.nodes)
        {
            if (!node.id.IsValid() || !nodeIds.insert(node.id.value).second)
            {
                hardInvalid = true;
            }

            if (node.kind == EditorDockNodeKind::Split)
            {
                if (!FindDockNode(frame.dock, node.first) || !FindDockNode(frame.dock, node.second))
                {
                    AddIssue(report, EditorLayoutRepairCode::InvalidSplitReference, EditorLayoutRepairSeverity::Error, "Split node referenced a missing child", node.id);
                    hardInvalid = true;
                }
                const float clamped = std::clamp(IsFiniteWeight(node.splitWeight) ? node.splitWeight : 0.5f, policy.minSplitWeight, policy.maxSplitWeight);
                if (node.splitWeight != clamped)
                {
                    AddIssue(report, EditorLayoutRepairCode::InvalidSplitWeight, EditorLayoutRepairSeverity::Warning, "Split weight was clamped", node.id);
                    node.splitWeight = clamped;
                }
            }
        }

        if (hardInvalid)
        {
            ResetToDefault(frame, panels, policy, report, "Dock graph contained unrecoverable duplicate nodes or invalid split references");
            report.ok = true;
            report.summary = FormatEditorLayoutRepairReport(report);
            return report;
        }

        std::unordered_set<u32> seenPanels;
        for (EditorDockNode& node : frame.dock.nodes)
        {
            if (node.kind != EditorDockNodeKind::Stack)
            {
                continue;
            }

            std::vector<EditorDockTab> repairedTabs;
            repairedTabs.reserve(node.tabs.size());
            for (const EditorDockTab& tab : node.tabs)
            {
                if (!panels.Has(tab.panel))
                {
                    AddIssue(report, EditorLayoutRepairCode::UnknownPanel, EditorLayoutRepairSeverity::Warning, "Removed tab for unknown panel", node.id, tab.panel);
                    if (policy.removeUnknownPanels)
                    {
                        continue;
                    }
                }

                if (!seenPanels.insert(tab.panel.value).second)
                {
                    AddIssue(report, EditorLayoutRepairCode::DuplicatePanel, EditorLayoutRepairSeverity::Warning, "Removed duplicate panel tab", node.id, tab.panel);
                    if (policy.removeDuplicatePanels)
                    {
                        continue;
                    }
                }

                repairedTabs.push_back(tab);
            }

            node.tabs = std::move(repairedTabs);
            if (node.tabs.empty())
            {
                AddIssue(report, EditorLayoutRepairCode::EmptyStack, EditorLayoutRepairSeverity::Error, "Stack became empty after repair", node.id);
            }
            if (!node.tabs.empty() && node.activeTab >= node.tabs.size())
            {
                AddIssue(report, EditorLayoutRepairCode::ActiveTabOutOfRange, EditorLayoutRepairSeverity::Warning, "Active tab index was clamped", node.id);
                node.activeTab = node.tabs.size() - 1;
            }
        }

        if (policy.resetIfCorePanelsMissing && !HasCorePanels(frame, panels))
        {
            ResetToDefault(frame, panels, policy, report, "Required core editor panels were missing after layout repair");
            report.ok = true;
            report.summary = FormatEditorLayoutRepairReport(report);
            return report;
        }

        RebuildFrameRects(frame, policy.minWindowWidth, policy.minWindowHeight);
        ComputeEditorDockRects(frame);
        const EditorDockDiagnostics diagnostics = ValidateEditorDockLayout(frame, panels);
        if (!diagnostics.ok)
        {
            ResetToDefault(frame, panels, policy, report, "Dock diagnostics failed after repair: " + diagnostics.summary);
            ComputeEditorDockRects(frame);
        }

        report.ok = true;
        report.summary = FormatEditorLayoutRepairReport(report);
        return report;
    }

    EditorLayoutLoadResult LoadEditorLayoutWithRepair(std::string_view text, const EditorPanelRegistry& panels, i32 fallbackWidth, i32 fallbackHeight, const std::vector<EditorMonitorBounds>& monitors, const EditorLayoutPersistencePolicy& policy)
    {
        EditorLayoutLoadResult result{};
        result.frame = BuildDefaultEditorFrameLayout(panels, fallbackWidth, fallbackHeight);
        result.placement = MakeDefaultEditorWindowPlacement(fallbackWidth, fallbackHeight);

        std::string dockText;
        bool legacy = false;
        if (!ParseEditorLayoutDocument(text, dockText, result.placement, legacy))
        {
            AddIssue(result.repair, EditorLayoutRepairCode::ParseFailed, EditorLayoutRepairSeverity::Error, "Layout document header could not be parsed");
            result.repair.resetToDefault = true;
            result.repair.repaired = true;
            result.repair.ok = true;
            result.ok = true;
            result.summary = FormatEditorLayoutRepairReport(result.repair);
            return result;
        }

        result.loadedFromText = true;
        result.usedLegacyAkLayout = text.substr(0, 8) == "AKLAYOUT";
        Result<EditorFrameLayout> loaded = DeserializeEditorDockLayout(dockText, panels);
        if (loaded.Ok())
        {
            result.frame = loaded.Value();
            result.repair = RepairEditorFrameLayout(result.frame, panels, policy);
        }
        else
        {
            AddIssue(result.repair, EditorLayoutRepairCode::ParseFailed, EditorLayoutRepairSeverity::Error, loaded.GetError().message);
            result.frame = BuildDefaultEditorFrameLayout(panels, fallbackWidth, fallbackHeight);
            result.repair.repaired = true;
            result.repair.resetToDefault = true;
            result.repair.ok = true;
            result.repair.summary = FormatEditorLayoutRepairReport(result.repair);
        }

        result.placement = ClampEditorWindowPlacementToMonitors(result.placement, monitors, policy, &result.repair);
        result.repair.ok = true;
        result.repair.summary = FormatEditorLayoutRepairReport(result.repair);
        result.ok = true;
        result.summary = result.repair.summary;
        return result;
    }

    std::string SerializeEditorLayoutDocument(const EditorFrameLayout& frame, const EditorWindowPlacement& placement)
    {
        std::ostringstream out;
        out << "AKEDITORLAYOUT 1\n";
        out << "window " << placement.rect.x << ' ' << placement.rect.y << ' ' << placement.rect.width << ' ' << placement.rect.height << ' ' << placement.monitorIndex << ' ' << (placement.maximized ? 1 : 0) << "\n";
        out << "dock_begin\n";
        out << SerializeEditorDockLayout(frame);
        out << "dock_end\n";
        return out.str();
    }

    std::string FormatEditorLayoutRepairIssue(const EditorLayoutRepairIssue& issue)
    {
        std::ostringstream out;
        out << ToString(issue.severity) << ':' << ToString(issue.code);
        if (issue.node.IsValid())
        {
            out << " node=" << issue.node.value;
        }
        if (issue.panel.IsValid())
        {
            out << " panel=" << issue.panel.value;
        }
        if (!issue.message.empty())
        {
            out << " " << issue.message;
        }
        return out.str();
    }

    std::string FormatEditorLayoutRepairReport(const EditorLayoutRepairReport& report)
    {
        std::size_t errors = 0;
        std::size_t warnings = 0;
        for (const EditorLayoutRepairIssue& issue : report.issues)
        {
            if (issue.severity == EditorLayoutRepairSeverity::Error)
            {
                ++errors;
            }
            else if (issue.severity == EditorLayoutRepairSeverity::Warning)
            {
                ++warnings;
            }
        }

        std::ostringstream out;
        out << "layout-repair issues=" << report.issues.size()
            << " warnings=" << warnings
            << " errors=" << errors
            << " repaired=" << (report.repaired ? "true" : "false")
            << " reset=" << (report.resetToDefault ? "true" : "false")
            << " ok=" << (report.ok ? "true" : "false");
        return out.str();
    }
}
