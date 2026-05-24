#include <AK/EditorUI/EditorFocusProbe.hpp>

#include <AK/EditorUI/EditorFocus.hpp>

#include <sstream>

namespace AK
{
    EditorFocusProbeResult RunEditorFocusProbe()
    {
        EditorFocusProbeResult result{};
        EditorFocusState state = MakeDefaultEditorFocusState();

        EditorHitTestResult hierarchyHit{};
        hierarchyHit.region = EditorHitRegion::DockPanelBody;
        hierarchyHit.panel = {1u};
        hierarchyHit.label = "HierarchyRow:Entity_12";

        EditorInputRoutingInput click{};
        click.hit = hierarchyHit;
        click.mouseLeftPressed = true;
        click.mouseLeftDown = true;
        const EditorInputRoutingDecision hierarchyDecision = RouteEditorInputFocus(state, click);
        const bool hierarchyFocusOk = state.focused.scope == EditorFocusScope::Hierarchy && hierarchyDecision.route == EditorInputRoute::Panel;

        EditorHitTestResult sceneHit{};
        sceneHit.region = EditorHitRegion::DockPanelBody;
        sceneHit.panel = {3u};
        sceneHit.label = "SceneView";
        EditorInputRoutingInput sceneMove{};
        sceneMove.hit = sceneHit;
        sceneMove.mouseLeftDown = false;
        const EditorInputRoutingDecision sceneDecision = RouteEditorInputFocus(state, sceneMove);
        const bool viewportRouteOk = sceneDecision.route == EditorInputRoute::Viewport && sceneDecision.allowViewportInput;

        const bool beginTextOk = BeginEditorTextEdit(state, "Inspector.Transform.Position.X", "0.00");
        const bool appendOk = AppendEditorTextInput(state, "-12.5", 32);
        const bool textCaptureOk = EditorHasTextCapture(state) && !EditorAllowsGlobalShortcuts(state) && state.textEdit.workingText == "0.00-12.5";
        const bool backspaceOk = BackspaceEditorTextInput(state);
        const bool commitOk = CommitEditorTextEdit(state) && !EditorHasTextCapture(state);

        EditorInputRoutingInput popupInput{};
        popupInput.popupOpen = true;
        popupInput.cancelPressed = true;
        const EditorInputRoutingDecision popupDecision = RouteEditorInputFocus(state, popupInput);
        const bool popupOk = popupDecision.route == EditorInputRoute::Popup && popupDecision.closePopup && !popupDecision.allowGlobalShortcuts;

        const EditorFocusDiagnostics diagnostics = ValidateEditorFocusState(state);
        result.ok = hierarchyFocusOk && viewportRouteOk && beginTextOk && appendOk && textCaptureOk && backspaceOk && commitOk && popupOk && diagnostics.ok;

        std::ostringstream summary;
        summary << "[ ok ] editor input capture / focus model foundation hierarchy=" << hierarchyFocusOk
                << " viewport=" << viewportRouteOk
                << " text=" << textCaptureOk
                << " popup=" << popupOk << '\n'
                << diagnostics.summary;
        result.summary = summary.str();
        return result;
    }

    std::string BuildEditorFocusProbeSummary()
    {
        return RunEditorFocusProbe().summary;
    }
}
