#include <AK/EditorUI/EditorSessionStateProbe.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool HasIssue(const EditorSessionRepairReport& report, EditorSessionRepairCode code)
        {
            return std::any_of(report.issues.begin(), report.issues.end(), [code](const EditorSessionRepairIssue& issue) { return issue.code == code; });
        }
    }

    EditorSessionStateProbeResult BuildEditorSessionStateProbe()
    {
        EditorSessionStateProbeResult result{};
        const EditorPanelRegistry panels = BuildDefaultEditorPanelRegistry();
        EditorFrameLayout frame = BuildDefaultEditorFrameLayout(panels, 1600, 900);
        ComputeEditorDockRects(frame);

        EditorSelectionModel selection{};
        EditorSelectionItem camera{};
        camera.domain = EditorSelectionDomain::SceneEntity;
        camera.stableId = "entity:camera";
        camera.displayName = "Main Camera";
        SetSelection(selection, camera);

        EditorSelectionItem cube{};
        cube.domain = EditorSelectionDomain::SceneEntity;
        cube.stableId = "entity:cube";
        cube.displayName = "Cube";
        AddSelection(selection, cube);

        std::vector<EditorScrollState> scrollStates;
        scrollStates.push_back(MakeEditorScrollState(EditorScrollPanelKind::Hierarchy, 3000, 320));
        scrollStates.back().offsetPixels = 220;
        scrollStates.push_back(MakeEditorScrollState(EditorScrollPanelKind::Inspector, 2800, 520));
        scrollStates.back().offsetPixels = 440;
        scrollStates.push_back(MakeEditorScrollState(EditorScrollPanelKind::ProjectGrid, 1800, 280));
        scrollStates.back().offsetPixels = 330;

        EditorSessionCaptureInput capture{};
        capture.frame = &frame;
        capture.selection = &selection;
        capture.scrollStates = &scrollStates;
        capture.focusedPanel = panels.FindByName("inspector") ? panels.FindByName("inspector")->id : EditorPanelId{};
        capture.projectName = "Sandbox";
        capture.scenePath = "projects/Sandbox/scene.akscene";
        capture.hierarchySearch = "Cam";
        capture.assetSearch = "mat";
        capture.expansions = {{"hierarchy", "scene:Sandbox", true}, {"hierarchy", "entity:camera", true}, {"project", "Assets/Materials", true}};
        capture.foldouts = {{"inspector.transform", true}, {"inspector.camera", true}, {"inspector.bounds", false}};

        EditorSessionState session = CaptureEditorSessionState(capture, panels);
        session.commandPaletteOpen = true;
        session.commandPaletteQuery = "open profiler";
        session.popupOpen = true;
        session.modalOpen = true;
        session.textEditActive = true;
        session.dragActive = true;
        const std::string cleanText = SerializeEditorSessionState(session);
        const std::vector<std::string> validIds = {"entity:camera", "entity:cube", "scene:Sandbox", "Assets/Materials"};
        result.cleanLoad = LoadEditorSessionWithRepair(cleanText, panels, validIds);

        EditorSessionState corrupted = session;
        EditorPanelSessionState unknownPanel{};
        unknownPanel.panel.value = 999999u;
        unknownPanel.panelName = "missing.panel";
        unknownPanel.scrollY = -400;
        corrupted.panels.push_back(unknownPanel);
        if (!corrupted.panels.empty())
        {
            corrupted.panels.push_back(corrupted.panels.front());
            corrupted.panels.front().scrollY = 99'999'999;
        }
        corrupted.focusedPanelName = "missing.focus";
        corrupted.selection.push_back(corrupted.selection.front());
        EditorSessionSelectionEntry missingSelection{};
        missingSelection.domain = EditorSelectionDomain::SceneEntity;
        missingSelection.stableId = "entity:deleted";
        missingSelection.displayName = "Deleted";
        missingSelection.active = true;
        corrupted.selection.push_back(missingSelection);
        corrupted.expansions.push_back({"", "", true});
        corrupted.foldouts.push_back({"", true});
        corrupted.popupOpen = true;
        corrupted.textEditActive = true;
        corrupted.dragActive = true;

        result.repairedLoad = LoadEditorSessionWithRepair(SerializeEditorSessionState(corrupted), panels, validIds);
        result.unknownPanelDropped = HasIssue(result.repairedLoad.repair, EditorSessionRepairCode::UnknownPanel);
        result.unknownSelectionDropped = HasIssue(result.repairedLoad.repair, EditorSessionRepairCode::UnknownSelection);
        result.duplicateSelectionDropped = HasIssue(result.repairedLoad.repair, EditorSessionRepairCode::DuplicateSelection);
        result.scrollClamped = HasIssue(result.repairedLoad.repair, EditorSessionRepairCode::InvalidScrollOffset);
        result.transientDropped = HasIssue(result.repairedLoad.repair, EditorSessionRepairCode::TransientStateDropped);

        EditorFrameLayout restoredFrame = frame;
        result.activeTabsRestored = RestoreEditorSessionActiveTabs(restoredFrame, result.repairedLoad.session, panels);
        result.diagnostics = RunEditorSessionDiagnostics();
        result.ok = result.cleanLoad.ok
            && result.repairedLoad.ok
            && result.unknownPanelDropped
            && result.unknownSelectionDropped
            && result.duplicateSelectionDropped
            && result.scrollClamped
            && result.transientDropped
            && result.activeTabsRestored
            && result.diagnostics.ok;

        std::ostringstream out;
        out << "editor-session-probe clean=" << (result.cleanLoad.ok ? 1 : 0)
            << " repaired=" << (result.repairedLoad.ok ? 1 : 0)
            << " unknownPanel=" << (result.unknownPanelDropped ? 1 : 0)
            << " unknownSelection=" << (result.unknownSelectionDropped ? 1 : 0)
            << " duplicateSelection=" << (result.duplicateSelectionDropped ? 1 : 0)
            << " scroll=" << (result.scrollClamped ? 1 : 0)
            << " transient=" << (result.transientDropped ? 1 : 0)
            << " activeTabs=" << (result.activeTabsRestored ? 1 : 0)
            << " ok=" << (result.ok ? 1 : 0);
        result.summary = out.str();
        return result;
    }
}
