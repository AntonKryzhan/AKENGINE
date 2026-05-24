#pragma once

#include <AK/EditorUI/EditorDiagnosticsPanel.hpp>

#include <string>

namespace AK
{
    struct EditorDiagnosticsPanelProbeResult
    {
        EditorDiagnosticsPanelDiagnostics diagnostics{};
        bool activityRows = false;
        bool consoleRows = false;
        bool metricRows = false;
        bool commandRows = false;
        bool workspaceRows = false;
        bool autosaveRows = false;
        bool consoleSynced = false;
        bool ok = false;
        std::string summary;
    };

    EditorDiagnosticsPanelProbeResult BuildEditorDiagnosticsPanelProbe();
    std::string BuildEditorDiagnosticsPanelProbeSummary();
}
