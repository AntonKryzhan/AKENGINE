#include <AK/EditorUI/EditorRuntimeProbe.hpp>

#include <sstream>

namespace AK
{
    EditorRuntimeProbeResult BuildEditorRuntimeProbe()
    {
        EditorRuntimeProbeResult result{};
        result.bridge = BuildDefaultEditorRuntimeBridge(1600, 900);

        const EditorPanelDesc* scenePanel = result.bridge.panels.FindByName("scene.viewport");
        const EditorPanelRuntimeState* sceneState = scenePanel ? nullptr : nullptr;
        if (scenePanel)
        {
            for (const EditorPanelRuntimeState& state : result.bridge.plan.panels)
            {
                if (state.panel == scenePanel->id)
                {
                    sceneState = &state;
                    break;
                }
            }
        }

        result.toolbarHit = HitTestEditorRuntime(result.bridge.plan, result.bridge.plan.frame.toolbar.x + 20, result.bridge.plan.frame.toolbar.y + 12);
        if (sceneState)
        {
            result.sceneHit = HitTestEditorRuntime(result.bridge.plan, sceneState->bodyRect.x + 24, sceneState->bodyRect.y + 24);
        }
        result.overlayHit = HitTestEditorRuntime(result.bridge.plan, 24, 24);

        result.pointerCommands = RouteEditorPointerEvent(result.bridge, {result.bridge.plan.frame.toolbar.x + 20, result.bridge.plan.frame.toolbar.y + 12, EditorPointerButton::Left, false, true, false});
        if (scenePanel)
        {
            FocusEditorPanel(result.bridge, scenePanel->id);
        }
        SetEditorCommandEnabled(result.bridge, CommandId::DeleteSelection, true);
        ToggleEditorOverlay(result.bridge, 8, true);
        result.searchResults = QueryEditorRuntime(result.bridge, "scene", 8);
        result.diagnostics = ValidateEditorRuntimeBridge(result.bridge);
        result.serializedLayout = SerializeEditorRuntimeLayout(result.bridge);
        result.deserializedOk = DeserializeEditorRuntimeLayout(result.serializedLayout, 1280, 720).Ok();

        result.ok = result.diagnostics.ok
            && result.toolbarHit.region == EditorHitRegion::Toolbar
            && result.sceneHit.region == EditorHitRegion::DockPanelBody
            && result.pointerCommands.size() == 1
            && !result.searchResults.empty()
            && result.serializedLayout.find("AKEDITORRUNTIME 1") != std::string::npos
            && result.deserializedOk;

        std::ostringstream out;
        out << (result.ok ? "[ ok ]" : "[fail]")
            << " editor docking runtime / gdi bridge foundation"
            << " panels=" << result.diagnostics.panelStateCount
            << " active=" << result.diagnostics.activePanelCount
            << " splitters=" << result.diagnostics.splitterCount
            << " routes=" << result.diagnostics.commandRouteCount
            << " commands=" << result.pointerCommands.size()
            << " search=" << result.searchResults.size()
            << " serialized=" << result.serializedLayout.size()
            << " loaded=" << (result.deserializedOk ? "true" : "false");
        result.summary = out.str();
        return result;
    }
}
