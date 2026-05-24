#include <AK/EditorUI/EditorUXRuntimeProbe.hpp>

#include <AK/EditorUI/EditorUXRuntime.hpp>

#include <sstream>

namespace AK
{
    EditorUXRuntimeProbeResult RunEditorUXRuntimeProbe()
    {
        EditorUXRuntime runtime{};
        const bool opened = OpenEditorUXPopup(runtime, BuildHierarchyContextWorkflow(MakeEditorRect(64, 144, 1, 1)), 1600, 900);
        const EditorUXRuntimeDiagnostics openDiagnostics = ValidateEditorUXRuntime(runtime);
        const EditorUXHitResult hit = HitTestEditorUXPopup(runtime, runtime.surface.rect.x + 16, runtime.surface.rect.y + 16);
        const EditorUXActivationResult firstActivation = ActivateEditorUXPopupItem(runtime, runtime.surface.rect.x + 16, runtime.surface.rect.y + 16);

        const bool reopened = OpenEditorUXPopup(runtime, BuildGridSnapWorkflow(MakeEditorRect(520, 108, 78, 22)), 1600, 900);
        const EditorUXRuntimeDiagnostics popupDiagnostics = ValidateEditorUXRuntime(runtime);
        const EditorUXActivationResult outsideActivation = ActivateEditorUXPopupItem(runtime, 2, 2);

        EditorUXRuntimeProbeResult result{};
        result.ok = opened
            && reopened
            && openDiagnostics.ok
            && popupDiagnostics.ok
            && hit.insidePopup
            && hit.insideItem
            && firstActivation.consumed
            && firstActivation.closePopup
            && outsideActivation.consumed
            && outsideActivation.closePopup
            && runtime.lastCloseReason == EditorUXPopupCloseReason::OutsideClick;

        std::ostringstream out;
        out << "editor-ux-runtime opened=" << (opened ? "true" : "false")
            << " reopened=" << (reopened ? "true" : "false")
            << " firstItem=" << firstActivation.itemId
            << " firstCommand=" << ToString(firstActivation.invocation.id)
            << " hitItem=" << hit.itemId
            << " menuItems=" << openDiagnostics.surfaceItemCount
            << " popupRows=" << popupDiagnostics.surfaceItemCount
            << " close=" << ToString(runtime.lastCloseReason)
            << " ok=" << (result.ok ? "true" : "false");
        result.summary = out.str();
        return result;
    }

    std::string BuildEditorUXRuntimeProbeSummary()
    {
        return RunEditorUXRuntimeProbe().summary;
    }
}
