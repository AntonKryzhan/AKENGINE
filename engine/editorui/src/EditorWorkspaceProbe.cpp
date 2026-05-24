#include <AK/EditorUI/EditorWorkspaceProbe.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace AK
{
    EditorWorkspaceProbeResult BuildEditorWorkspaceProbe()
    {
        EditorWorkspaceProbeResult result{};
        const EditorPanelRegistry panels = BuildDefaultEditorPanelRegistry();
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "ak_editor_workspace_probe";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);

        EditorPreferencesProfile prefs = BuildDefaultEditorPreferencesProfile();
        prefs.projectRoot = root.generic_string();
        prefs.recentProjects.push_back("projects/Sandbox");
        const EditorWorkspacePaths paths = BuildEditorWorkspacePaths(root.generic_string(), prefs);

        result.firstLoad = LoadEditorWorkspace(paths, panels, 1440, 810);
        result.directoriesCreated = result.firstLoad.report.ok;
        result.defaultsUsed = result.firstLoad.report.usedDefaults;

        EditorFrameLayout frame = BuildDefaultEditorFrameLayout(panels, 1440, 810);
        EditorWindowPlacement placement = MakeDefaultEditorWindowPlacement(1440, 810);
        EditorSessionState session = MakeDefaultEditorSessionState(panels, "Sandbox", "projects/Sandbox/scene.akscene");
        EditorShortcutProfile shortcuts = BuildDefaultEditorShortcutProfile();

        EditorWorkspaceSaveInput saveInput{};
        saveInput.paths = result.firstLoad.paths;
        saveInput.preferences = &prefs;
        saveInput.shortcuts = &shortcuts;
        saveInput.layout = &frame;
        saveInput.placement = &placement;
        saveInput.session = &session;
        result.save = SaveEditorWorkspace(saveInput);
        result.filesWritten = result.save.filesWritten >= 4;

        result.secondLoad = LoadEditorWorkspace(result.firstLoad.paths, panels, 1440, 810, MakeDefaultEditorMonitorSet(1920, 1080), {"entity:Camera", "asset:CubeMesh"});
        result.reloadOk = result.secondLoad.ok && result.secondLoad.loadedPreferences && result.secondLoad.loadedShortcuts && result.secondLoad.loadedLayout && result.secondLoad.loadedSession;

        {
            std::ofstream corrupt(result.firstLoad.paths.sessionPath, std::ios::binary | std::ios::trunc);
            corrupt << "AKEDITORSESSION 1\n"
                    << "project \"Sandbox\"\n"
                    << "scene \"projects/Sandbox/scene.akscene\"\n"
                    << "panel \"missing.panel\" 9999999 -9999999 1 1 0\n"
                    << "transient 1 1 1 1\n";
        }

        result.repairedLoad = LoadEditorWorkspace(result.firstLoad.paths, panels, 1440, 810, MakeDefaultEditorMonitorSet(1920, 1080), {"entity:Camera"});
        result.repairOk = result.repairedLoad.ok && result.repairedLoad.report.repaired;
        result.diagnostics = RunEditorWorkspaceDiagnostics();
        result.issueCount = result.firstLoad.report.issues.size() + result.secondLoad.report.issues.size() + result.repairedLoad.report.issues.size();
        result.ok = result.directoriesCreated && result.defaultsUsed && result.filesWritten && result.reloadOk && result.repairOk && result.diagnostics.ok;

        std::ostringstream summary;
        summary << "editor-workspace-probe defaults=" << (result.defaultsUsed ? 1 : 0)
                << " files=" << result.save.filesWritten
                << " reload=" << (result.reloadOk ? 1 : 0)
                << " repaired=" << (result.repairOk ? 1 : 0)
                << " issues=" << result.issueCount
                << " ok=" << (result.ok ? 1 : 0);
        result.summary = summary.str();

        std::filesystem::remove_all(root, ec);
        return result;
    }
}
