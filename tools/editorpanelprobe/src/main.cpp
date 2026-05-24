#include <AK/EditorUI/EditorPanelProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorPanelProbeResult probe = AK::BuildEditorPanelProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::FormatEditorPanelModelDiagnostics(probe.diagnostics) << '\n';
    std::cout << probe.firstProperty << '\n';
    std::cout << probe.firstAsset << '\n';
    std::cout << probe.firstConsoleEntry << '\n';
    if (!probe.propertySearchResults.empty())
    {
        std::cout << AK::FormatEditorSearchResult(probe.propertySearchResults.front()) << '\n';
    }
    if (!probe.assetSearchResults.empty())
    {
        std::cout << AK::FormatEditorSearchResult(probe.assetSearchResults.front()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
