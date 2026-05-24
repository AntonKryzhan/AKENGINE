#include <AK/EditorUI/EditorLayoutPersistenceProbe.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool HasIssue(const EditorLayoutRepairReport& report, EditorLayoutRepairCode code)
        {
            return std::any_of(report.issues.begin(), report.issues.end(), [code](const EditorLayoutRepairIssue& issue) { return issue.code == code; });
        }
    }

    EditorLayoutPersistenceProbeResult BuildEditorLayoutPersistenceProbe()
    {
        EditorLayoutPersistenceProbeResult result{};
        const EditorPanelRegistry panels = BuildDefaultEditorPanelRegistry();
        const EditorLayoutPersistencePolicy policy = MakeDefaultEditorLayoutPersistencePolicy();
        const std::vector<EditorMonitorBounds> monitors = MakeDefaultEditorMonitorSet(1920, 1040);

        EditorFrameLayout frame = BuildDefaultEditorFrameLayout(panels, 1600, 900);
        EditorWindowPlacement placement = MakeDefaultEditorWindowPlacement(1600, 900);
        placement.rect = {120, 90, 1600, 900};
        const std::string document = SerializeEditorLayoutDocument(frame, placement);
        result.cleanLoad = LoadEditorLayoutWithRepair(document, panels, 1280, 720, monitors, policy);

        EditorFrameLayout corrupted = frame;
        for (EditorDockNode& node : corrupted.dock.nodes)
        {
            if (node.kind == EditorDockNodeKind::Split)
            {
                node.splitWeight = 1.35f;
                break;
            }
        }
        for (EditorDockNode& node : corrupted.dock.nodes)
        {
            if (node.kind == EditorDockNodeKind::Stack && !node.tabs.empty())
            {
                node.activeTab = 99;
                node.tabs.push_back(node.tabs.front());
                EditorDockTab unknown = node.tabs.front();
                unknown.panel.value = 999999u;
                unknown.title = "Missing Panel";
                node.tabs.push_back(unknown);
                break;
            }
        }
        corrupted.width = 300;
        corrupted.height = 120;
        result.corruptedRepair = RepairEditorFrameLayout(corrupted, panels, policy);
        result.duplicateRemoved = HasIssue(result.corruptedRepair, EditorLayoutRepairCode::DuplicatePanel);
        result.unknownRemoved = HasIssue(result.corruptedRepair, EditorLayoutRepairCode::UnknownPanel);
        result.activeTabClamped = HasIssue(result.corruptedRepair, EditorLayoutRepairCode::ActiveTabOutOfRange);
        result.splitWeightClamped = HasIssue(result.corruptedRepair, EditorLayoutRepairCode::InvalidSplitWeight);

        const std::string legacy = SerializeEditorDockLayout(frame);
        result.legacyLoad = LoadEditorLayoutWithRepair(legacy, panels, 1600, 900, monitors, policy);

        EditorWindowPlacement offscreen{};
        offscreen.rect = {50000, -24000, 2000, 1400};
        EditorLayoutRepairReport clampReport{};
        result.clampedPlacement = ClampEditorWindowPlacementToMonitors(offscreen, monitors, policy, &clampReport);
        result.windowClamped = HasIssue(clampReport, EditorLayoutRepairCode::OffscreenWindow) || HasIssue(clampReport, EditorLayoutRepairCode::ClampedWindow);

        const EditorLayoutLoadResult broken = LoadEditorLayoutWithRepair("BROKEN 9\n", panels, 1600, 900, monitors, policy);
        result.resetFallback = broken.repair.resetToDefault || HasIssue(broken.repair, EditorLayoutRepairCode::ParseFailed);

        result.issueCount = result.corruptedRepair.issues.size() + clampReport.issues.size() + broken.repair.issues.size();
        result.ok = result.cleanLoad.ok
            && result.legacyLoad.ok
            && result.corruptedRepair.ok
            && result.duplicateRemoved
            && result.unknownRemoved
            && result.activeTabClamped
            && result.splitWeightClamped
            && result.windowClamped
            && result.resetFallback;

        std::ostringstream out;
        out << "editor-layout-persistence clean=" << (result.cleanLoad.ok ? 1 : 0)
            << " legacy=" << (result.legacyLoad.ok ? 1 : 0)
            << " issues=" << result.issueCount
            << " duplicate=" << (result.duplicateRemoved ? 1 : 0)
            << " unknown=" << (result.unknownRemoved ? 1 : 0)
            << " active=" << (result.activeTabClamped ? 1 : 0)
            << " split=" << (result.splitWeightClamped ? 1 : 0)
            << " window=" << (result.windowClamped ? 1 : 0)
            << " reset=" << (result.resetFallback ? 1 : 0)
            << " ok=" << (result.ok ? 1 : 0);
        result.summary = out.str();
        return result;
    }
}
