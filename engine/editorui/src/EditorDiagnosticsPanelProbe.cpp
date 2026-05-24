#include <AK/EditorUI/EditorDiagnosticsPanelProbe.hpp>

#include <sstream>

namespace AK
{
    EditorDiagnosticsPanelProbeResult BuildEditorDiagnosticsPanelProbe()
    {
        EditorDiagnosticsPanelProbeResult result{};
        result.diagnostics = RunEditorDiagnosticsPanelDiagnostics();
        result.activityRows = result.diagnostics.activityRows > 0;
        result.consoleRows = result.diagnostics.consoleRows > 0;
        result.metricRows = result.diagnostics.metricRows > 0;
        result.commandRows = result.diagnostics.commandRows > 0;
        result.workspaceRows = result.diagnostics.workspaceRows > 0;
        result.autosaveRows = result.diagnostics.autosaveRows > 0;
        result.consoleSynced = result.diagnostics.consoleSync.ok;
        result.ok = result.diagnostics.ok
            && result.activityRows
            && result.consoleRows
            && result.metricRows
            && result.commandRows
            && result.workspaceRows
            && result.autosaveRows
            && result.consoleSynced;

        std::ostringstream out;
        out << "editor-diagnostics-panel-probe activity=" << result.activityRows
            << " console=" << result.consoleRows
            << " metrics=" << result.metricRows
            << " commands=" << result.commandRows
            << " workspace=" << result.workspaceRows
            << " autosave=" << result.autosaveRows
            << " synced=" << result.consoleSynced
            << " ok=" << result.ok;
        result.summary = out.str();
        return result;
    }

    std::string BuildEditorDiagnosticsPanelProbeSummary()
    {
        return BuildEditorDiagnosticsPanelProbe().summary;
    }
}
