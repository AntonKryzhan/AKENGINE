#include <AK/EditorUI/EditorPreferencesProbe.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool HasIssue(const EditorPreferencesRepairReport& report, EditorPreferencesRepairCode code)
        {
            return std::any_of(report.issues.begin(), report.issues.end(), [code](const EditorPreferencesRepairIssue& issue)
            {
                return issue.code == code;
            });
        }
    }

    EditorPreferencesProbeResult BuildEditorPreferencesProbe()
    {
        EditorPreferencesProbeResult result{};

        EditorPreferencesProfile clean = BuildDefaultEditorPreferencesProfile();
        clean.profileName = "Sandbox Developer";
        clean.ui.dpiScale = 1.25f;
        clean.ui.themeProfile = EditorThemeProfile::UnityLikeDark;
        clean.autosave.intervalSeconds = 90;
        clean.viewport.cameraLookSensitivity = 1.5f;
        AddRecentEditorProject(clean, "D:/AKENGINE/projects/Sandbox");
        result.cleanRecentProjects = clean.recentProjects.size();

        const std::string cleanText = SerializeEditorPreferencesProfile(clean);
        result.cleanLoad = LoadEditorPreferencesProfileWithRepair(cleanText);
        result.startupPlan = BuildEditorStartupPlan(result.cleanLoad.profile);
        result.startupPlanBuilt = result.startupPlan.ok;

        EditorPreferencesProfile corrupted = clean;
        corrupted.ui.themeProfile = static_cast<EditorThemeProfile>(999);
        corrupted.startupMode = static_cast<EditorStartupMode>(99);
        corrupted.ui.dpiScale = 99.0f;
        corrupted.ui.mouseWheelLines = 0;
        corrupted.ui.assetIconScale = -2.0f;
        corrupted.autosave.intervalSeconds = 1;
        corrupted.autosave.maxBackupCount = 999;
        corrupted.viewport.cameraLookSensitivity = -1.0f;
        corrupted.recentProjects.push_back("");
        corrupted.recentProjects.push_back("D:\\AKENGINE\\projects\\Sandbox");
        corrupted.recentProjects.push_back("D:/AKENGINE/projects/Sandbox");

        result.repairedLoad = LoadEditorPreferencesProfileWithRepair(SerializeEditorPreferencesProfile(corrupted));
        result.repairedIssues = result.repairedLoad.repair.issues.size();
        result.duplicateRecentDropped = HasIssue(result.repairedLoad.repair, EditorPreferencesRepairCode::DuplicateRecentProject);
        result.invalidDpiClamped = HasIssue(result.repairedLoad.repair, EditorPreferencesRepairCode::InvalidDpiScale);
        result.invalidAutosaveClamped = HasIssue(result.repairedLoad.repair, EditorPreferencesRepairCode::InvalidAutosaveInterval)
            && HasIssue(result.repairedLoad.repair, EditorPreferencesRepairCode::InvalidBackupCount);
        result.invalidThemeRepaired = HasIssue(result.repairedLoad.repair, EditorPreferencesRepairCode::InvalidThemeProfile);
        result.diagnostics = RunEditorPreferencesDiagnostics();

        result.ok = result.cleanLoad.ok
            && result.cleanRecentProjects >= 2
            && result.startupPlanBuilt
            && result.repairedLoad.ok
            && result.repairedIssues >= 6
            && result.duplicateRecentDropped
            && result.invalidDpiClamped
            && result.invalidAutosaveClamped
            && result.invalidThemeRepaired
            && result.diagnostics.ok;

        std::ostringstream out;
        out << "editor-preferences-probe clean=" << (result.cleanLoad.ok ? 1 : 0)
            << " repaired=" << (result.repairedLoad.ok ? 1 : 0)
            << " issues=" << result.repairedIssues
            << " duplicateRecent=" << (result.duplicateRecentDropped ? 1 : 0)
            << " dpi=" << (result.invalidDpiClamped ? 1 : 0)
            << " autosave=" << (result.invalidAutosaveClamped ? 1 : 0)
            << " theme=" << (result.invalidThemeRepaired ? 1 : 0)
            << " startup=" << (result.startupPlanBuilt ? 1 : 0)
            << " ok=" << (result.ok ? 1 : 0);
        result.summary = out.str();
        return result;
    }
}
