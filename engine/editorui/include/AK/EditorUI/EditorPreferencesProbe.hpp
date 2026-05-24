#pragma once

#include <AK/EditorUI/EditorPreferences.hpp>

namespace AK
{
    struct EditorPreferencesProbeResult
    {
        EditorPreferencesLoadResult cleanLoad{};
        EditorPreferencesLoadResult repairedLoad{};
        EditorPreferencesDiagnostics diagnostics{};
        EditorStartupPlan startupPlan{};
        usize cleanRecentProjects = 0;
        usize repairedIssues = 0;
        bool duplicateRecentDropped = false;
        bool invalidDpiClamped = false;
        bool invalidAutosaveClamped = false;
        bool invalidThemeRepaired = false;
        bool startupPlanBuilt = false;
        bool ok = false;
        std::string summary;
    };

    EditorPreferencesProbeResult BuildEditorPreferencesProbe();
}
