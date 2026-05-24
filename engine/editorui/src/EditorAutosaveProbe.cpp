#include <AK/EditorUI/EditorAutosaveProbe.hpp>

#include <sstream>

namespace AK
{
    EditorAutosaveProbeResult BuildEditorAutosaveProbe()
    {
        EditorAutosaveProbeResult result{};
        result.diagnostics = RunEditorAutosaveDiagnostics();

        EditorPreferencesProfile prefs = BuildDefaultEditorPreferencesProfile();
        prefs.autosave.intervalSeconds = 1;
        prefs.autosave.maxBackupCount = 2;
        EditorAutosavePolicy policy = BuildEditorAutosavePolicyFromPreferences(prefs);
        EditorWorkspacePaths workspacePaths = BuildEditorWorkspacePaths("/tmp/ak_editor_autosave_probe", prefs);
        EditorAutosavePaths paths = BuildEditorAutosavePaths(workspacePaths, "/tmp/ak_editor_autosave_probe/autosave.akscene");
        result.state = MakeDefaultEditorAutosaveRuntimeState();

        EditorAutosaveTickInput input{};
        input.deltaSeconds = 0.25;
        input.sceneDirty = true;
        input.sceneRevision = 101;
        input.workspaceDirty = true;
        input.workspaceRevision = 201;
        const EditorAutosaveTickResult skipped = TickEditorAutosaveRuntime(policy, paths, result.state, input, "Scene With Spaces");
        result.firstSkipped = skipped.skipReason == EditorAutosaveSkipReason::IntervalNotReached;

        input.deltaSeconds = 1.0;
        const EditorAutosaveTickResult triggered = TickEditorAutosaveRuntime(policy, paths, result.state, input, "Scene With Spaces");
        result.triggered = triggered.triggered;
        result.sceneAutosave = triggered.shouldSaveScene;
        result.workspaceSave = triggered.shouldSaveWorkspace;
        result.backupCreated = triggered.shouldCreateBackup;

        NotifyEditorAutosaveSceneSaved(result.state, input.sceneRevision);
        NotifyEditorAutosaveWorkspaceSaved(result.state, input.workspaceRevision);
        result.notifyOk = result.state.lastSceneRevision == input.sceneRevision && result.state.lastWorkspaceRevision == input.workspaceRevision;

        const EditorBackupRotationPlan rotation = BuildEditorBackupRotationPlan({"a.akscene", "b.akscene", "c.akscene"}, 2);
        result.rotationPrunesOldest = rotation.ok && rotation.prune.size() == 1 && rotation.prune.front() == "a.akscene";

        result.ok = result.diagnostics.ok
            && result.firstSkipped
            && result.triggered
            && result.sceneAutosave
            && result.workspaceSave
            && result.backupCreated
            && result.rotationPrunesOldest
            && result.notifyOk;

        std::ostringstream summary;
        summary << "editor-autosave-probe triggered=" << (result.triggered ? 1 : 0)
                << " scene=" << (result.sceneAutosave ? 1 : 0)
                << " workspace=" << (result.workspaceSave ? 1 : 0)
                << " backup=" << (result.backupCreated ? 1 : 0)
                << " rotation=" << (result.rotationPrunesOldest ? 1 : 0)
                << " notify=" << (result.notifyOk ? 1 : 0)
                << " ok=" << (result.ok ? 1 : 0);
        result.summary = summary.str();
        return result;
    }

    std::string BuildEditorAutosaveProbeSummary()
    {
        return BuildEditorAutosaveProbe().summary;
    }
}
