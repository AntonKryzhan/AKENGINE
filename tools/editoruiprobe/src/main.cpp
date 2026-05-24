#include <AK/EditorUI/EditorUIProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorUIProbeResult probe = AK::BuildEditorUIProbe();
    std::cout << probe.summary << '\n';
    std::cout << probe.panelDiagnostics.summary << '\n';
    std::cout << probe.dockDiagnostics.summary << '\n';
    std::cout << probe.chromeDiagnostics.summary << '\n';
    std::cout << probe.selectionDiagnostics.summary << '\n';
    std::cout << probe.searchDiagnostics.summary << '\n';
    if (!probe.frame.dock.nodes.empty())
    {
        std::cout << AK::FormatEditorDockNode(probe.frame.dock.nodes.front()) << '\n';
        std::cout << AK::FormatEditorDockNode(probe.frame.dock.nodes.back()) << '\n';
    }
    if (!probe.overlays.empty())
    {
        std::cout << AK::FormatEditorOverlay(probe.overlays.front()) << '\n';
    }
    if (!probe.commandSearchResults.empty())
    {
        std::cout << AK::FormatEditorSearchResult(probe.commandSearchResults.front()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
