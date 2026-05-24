#include <AK/EditorUI/EditorDiagnosticsPanelProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorDiagnosticsPanelProbeResult result = AK::BuildEditorDiagnosticsPanelProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor diagnostics panel / activity console integration foundation\n"
                  << result.summary << '\n'
                  << result.diagnostics.summary << '\n'
                  << result.diagnostics.consoleSync.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor diagnostics panel / activity console integration foundation rows=" << result.diagnostics.state.rows.size()
              << " activity=" << result.activityRows
              << " tasks=" << result.diagnostics.taskRows
              << " console=" << result.consoleRows
              << " metrics=" << result.metricRows
              << " commands=" << result.commandRows
              << " workspace=" << result.workspaceRows
              << " autosave=" << result.autosaveRows << '\n';
    std::cout << result.summary << '\n';
    std::cout << result.diagnostics.summary << '\n';
    std::cout << result.diagnostics.consoleSync.summary << '\n';
    std::cout << AK::FormatEditorDiagnosticsPanelState(result.diagnostics.state) << '\n';
    return 0;
}
