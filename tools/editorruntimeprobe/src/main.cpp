#include <AK/EditorUI/EditorRuntimeProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorRuntimeProbeResult probe = AK::BuildEditorRuntimeProbe();
    std::cout << probe.summary << '\n';
    std::cout << probe.diagnostics.summary << '\n';
    std::cout << AK::FormatEditorHitTestResult(probe.toolbarHit) << '\n';
    std::cout << AK::FormatEditorHitTestResult(probe.sceneHit) << '\n';
    std::cout << AK::FormatEditorHitTestResult(probe.overlayHit) << '\n';
    if (!probe.bridge.plan.commandRoutes.empty())
    {
        std::cout << AK::FormatEditorCommandRoute(probe.bridge.plan.commandRoutes.front()) << '\n';
    }
    if (!probe.bridge.plan.panels.empty())
    {
        std::cout << AK::FormatEditorPanelRuntimeState(probe.bridge.plan.panels.front()) << '\n';
    }
    if (!probe.searchResults.empty())
    {
        std::cout << AK::FormatEditorSearchResult(probe.searchResults.front()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
