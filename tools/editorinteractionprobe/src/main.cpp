#include <AK/EditorUI/EditorInteractionProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorInteractionProbeResult probe = AK::BuildEditorInteractionProbe();
    std::cout << probe.summary << '\n';
    std::cout << AK::FormatEditorInteractionDiagnostics(probe.diagnostics) << '\n';
    std::cout << probe.paletteSummary << '\n';
    std::cout << probe.propertySummary << '\n';
    if (!probe.hierarchySelection.selectionChanges.empty())
    {
        std::cout << AK::FormatEditorSelectionChangeRequest(probe.hierarchySelection.selectionChanges.front()) << '\n';
    }
    if (!probe.paletteAccept.commands.empty())
    {
        std::cout << AK::FormatCommandInvocation(probe.context.bridge.commands, probe.paletteAccept.commands.front()) << '\n';
    }
    return probe.ok ? 0 : 1;
}
