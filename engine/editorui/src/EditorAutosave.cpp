#include <AK/EditorUI/EditorAutosave.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        double ClampInterval(double value, double minimum)
        {
            if (value < minimum)
            {
                return minimum;
            }
            return value;
        }

        std::string NormalizePath(std::filesystem::path path)
        {
            return path.lexically_normal().generic_string();
        }

        std::filesystem::path JoinPath(const std::string& root, const std::string& path)
        {
            std::filesystem::path p(path);
            if (p.is_absolute())
            {
                return p.lexically_normal();
            }
            return (std::filesystem::path(root) / p).lexically_normal();
        }

        std::string SanitizeStem(std::string_view stem)
        {
            std::string out;
            out.reserve(stem.size());
            for (char c : stem)
            {
                const bool ok = (c >= 'a' && c <= 'z')
                    || (c >= 'A' && c <= 'Z')
                    || (c >= '0' && c <= '9')
                    || c == '_' || c == '-';
                out.push_back(ok ? c : '_');
            }
            if (out.empty())
            {
                out = "scene";
            }
            return out;
        }

        void AddAction(EditorAutosaveTickResult& result,
                       EditorAutosaveActionKind kind,
                       std::string path,
                       std::string reason,
                       u64 revision,
                       bool required = true)
        {
            EditorAutosaveAction action{};
            action.kind = kind;
            action.path = std::move(path);
            action.reason = std::move(reason);
            action.revision = revision;
            action.required = required;
            result.actions.push_back(std::move(action));
        }
    }

    const char* ToString(EditorAutosaveActionKind kind)
    {
        switch (kind)
        {
            case EditorAutosaveActionKind::None: return "none";
            case EditorAutosaveActionKind::SceneAutosave: return "scene-autosave";
            case EditorAutosaveActionKind::SceneBackup: return "scene-backup";
            case EditorAutosaveActionKind::WorkspaceSave: return "workspace-save";
            case EditorAutosaveActionKind::PruneBackup: return "prune-backup";
            default: return "unknown";
        }
    }

    const char* ToString(EditorAutosaveSkipReason reason)
    {
        switch (reason)
        {
            case EditorAutosaveSkipReason::None: return "none";
            case EditorAutosaveSkipReason::Disabled: return "disabled";
            case EditorAutosaveSkipReason::EditorInactive: return "editor-inactive";
            case EditorAutosaveSkipReason::IntervalNotReached: return "interval-not-reached";
            case EditorAutosaveSkipReason::NothingDirty: return "nothing-dirty";
            case EditorAutosaveSkipReason::AlreadySavedRevision: return "already-saved-revision";
            default: return "unknown";
        }
    }

    EditorAutosavePolicy BuildEditorAutosavePolicyFromPreferences(const EditorPreferencesProfile& preferences)
    {
        EditorAutosavePolicy policy{};
        policy.enabled = preferences.autosave.enabled;
        policy.saveScene = true;
        policy.saveWorkspace = preferences.autosave.saveSessionOnExit || preferences.autosave.saveLayoutWithProject;
        policy.createSceneBackup = preferences.safety.createCrashRecoveryBackup;
        policy.intervalSeconds = ClampInterval(static_cast<double>(preferences.autosave.intervalSeconds), policy.minimumIntervalSeconds);
        policy.maxBackupCount = std::max<i32>(1, preferences.autosave.maxBackupCount);
        return policy;
    }

    EditorAutosaveRuntimeState MakeDefaultEditorAutosaveRuntimeState()
    {
        return {};
    }

    EditorAutosavePaths BuildEditorAutosavePaths(const EditorWorkspacePaths& workspacePaths, std::string sceneAutosavePath)
    {
        EditorAutosavePaths paths{};
        if (sceneAutosavePath.empty())
        {
            sceneAutosavePath = NormalizePath(JoinPath(workspacePaths.projectRoot, workspacePaths.autosaveDirectory + "/scene_autosave.akscene"));
        }
        else
        {
            sceneAutosavePath = NormalizePath(std::filesystem::path(sceneAutosavePath));
        }

        paths.sceneAutosavePath = sceneAutosavePath;
        paths.backupDirectory = workspacePaths.backupDirectory;
        paths.workspaceLayoutPath = workspacePaths.layoutPath;
        paths.workspaceSessionPath = workspacePaths.sessionPath;
        paths.workspacePreferencesPath = workspacePaths.preferencesPath;
        paths.workspaceShortcutsPath = workspacePaths.shortcutPath;
        return paths;
    }

    std::string BuildEditorSceneBackupFileName(std::string_view sceneStem, u64 revision, u64 triggerCount)
    {
        std::ostringstream out;
        out << SanitizeStem(sceneStem)
            << "_r" << std::setw(8) << std::setfill('0') << revision
            << "_a" << std::setw(8) << std::setfill('0') << triggerCount
            << ".akscene";
        return out.str();
    }

    std::string BuildEditorSceneBackupPath(const EditorAutosavePaths& paths, std::string_view sceneStem, u64 revision, u64 triggerCount)
    {
        return NormalizePath(std::filesystem::path(paths.backupDirectory) / BuildEditorSceneBackupFileName(sceneStem, revision, triggerCount));
    }

    EditorAutosaveTickResult TickEditorAutosaveRuntime(const EditorAutosavePolicy& policy,
                                                       const EditorAutosavePaths& paths,
                                                       EditorAutosaveRuntimeState& state,
                                                       const EditorAutosaveTickInput& input,
                                                       std::string_view sceneStem)
    {
        EditorAutosaveTickResult result{};
        result.ok = true;

        auto skip = [&](EditorAutosaveSkipReason reason)
        {
            result.skipReason = reason;
            state.lastSkipReason = reason;
            ++state.skippedCount;
            result.summary = FormatEditorAutosaveTickResult(result);
            return result;
        };

        if (!policy.enabled)
        {
            return skip(EditorAutosaveSkipReason::Disabled);
        }
        if (!input.editorActive)
        {
            return skip(EditorAutosaveSkipReason::EditorInactive);
        }

        state.accumulatedSeconds += std::max(0.0, input.deltaSeconds);
        const double interval = ClampInterval(policy.intervalSeconds, policy.minimumIntervalSeconds);
        if (state.accumulatedSeconds < interval)
        {
            return skip(EditorAutosaveSkipReason::IntervalNotReached);
        }

        const bool sceneNeedsSave = policy.saveScene
            && input.sceneDirty
            && input.sceneRevision != 0
            && input.sceneRevision != state.lastSceneRevision;
        const bool workspaceNeedsSave = policy.saveWorkspace
            && input.workspaceDirty
            && input.workspaceRevision != 0
            && input.workspaceRevision != state.lastWorkspaceRevision;

        if (!sceneNeedsSave && !workspaceNeedsSave)
        {
            state.accumulatedSeconds = 0.0;
            return skip(input.sceneDirty || input.workspaceDirty ? EditorAutosaveSkipReason::AlreadySavedRevision : EditorAutosaveSkipReason::NothingDirty);
        }

        ++state.triggerCount;
        state.accumulatedSeconds = 0.0;
        result.triggered = true;

        if (sceneNeedsSave)
        {
            if (policy.createSceneBackup)
            {
                AddAction(result,
                          EditorAutosaveActionKind::SceneBackup,
                          BuildEditorSceneBackupPath(paths, sceneStem, input.sceneRevision, state.triggerCount),
                          "crash-recovery scene backup before autosave",
                          input.sceneRevision);
                result.shouldCreateBackup = true;
                ++state.sceneBackupCount;
            }

            AddAction(result,
                      EditorAutosaveActionKind::SceneAutosave,
                      paths.sceneAutosavePath,
                      "dirty scene revision reached autosave interval",
                      input.sceneRevision);
            result.shouldSaveScene = true;
            ++state.sceneAutosaveCount;
        }

        if (workspaceNeedsSave)
        {
            AddAction(result,
                      EditorAutosaveActionKind::WorkspaceSave,
                      paths.workspaceSessionPath,
                      "dirty editor workspace/session revision reached autosave interval",
                      input.workspaceRevision);
            result.shouldSaveWorkspace = true;
            ++state.workspaceSaveCount;
        }

        result.summary = FormatEditorAutosaveTickResult(result);
        return result;
    }

    void NotifyEditorAutosaveSceneSaved(EditorAutosaveRuntimeState& state, u64 revision)
    {
        state.lastSceneRevision = revision;
    }

    void NotifyEditorAutosaveWorkspaceSaved(EditorAutosaveRuntimeState& state, u64 revision)
    {
        state.lastWorkspaceRevision = revision;
    }

    EditorBackupRotationPlan BuildEditorBackupRotationPlan(std::vector<std::string> existingBackups, i32 maxBackupCount)
    {
        EditorBackupRotationPlan plan{};
        plan.maxBackupCount = std::max<i32>(0, maxBackupCount);
        std::sort(existingBackups.begin(), existingBackups.end());

        if (plan.maxBackupCount == 0)
        {
            plan.prune = std::move(existingBackups);
        }
        else if (existingBackups.size() > static_cast<usize>(plan.maxBackupCount))
        {
            const usize pruneCount = existingBackups.size() - static_cast<usize>(plan.maxBackupCount);
            plan.prune.assign(existingBackups.begin(), existingBackups.begin() + static_cast<std::ptrdiff_t>(pruneCount));
            plan.keep.assign(existingBackups.begin() + static_cast<std::ptrdiff_t>(pruneCount), existingBackups.end());
        }
        else
        {
            plan.keep = std::move(existingBackups);
        }

        plan.ok = plan.keep.size() <= static_cast<usize>(std::max<i32>(0, plan.maxBackupCount));
        plan.summary = FormatEditorBackupRotationPlan(plan);
        return plan;
    }

    std::string FormatEditorAutosavePolicy(const EditorAutosavePolicy& policy)
    {
        std::ostringstream out;
        out << "editor-autosave-policy enabled=" << (policy.enabled ? 1 : 0)
            << " scene=" << (policy.saveScene ? 1 : 0)
            << " workspace=" << (policy.saveWorkspace ? 1 : 0)
            << " backup=" << (policy.createSceneBackup ? 1 : 0)
            << " interval=" << policy.intervalSeconds
            << " maxBackups=" << policy.maxBackupCount;
        return out.str();
    }

    std::string FormatEditorAutosavePaths(const EditorAutosavePaths& paths)
    {
        std::ostringstream out;
        out << "editor-autosave-paths scene=" << paths.sceneAutosavePath
            << " backups=" << paths.backupDirectory
            << " session=" << paths.workspaceSessionPath;
        return out.str();
    }

    std::string FormatEditorAutosaveAction(const EditorAutosaveAction& action)
    {
        std::ostringstream out;
        out << ToString(action.kind)
            << " revision=" << action.revision
            << " required=" << (action.required ? 1 : 0)
            << " path=" << action.path
            << " reason=" << action.reason;
        return out.str();
    }

    std::string FormatEditorAutosaveTickResult(const EditorAutosaveTickResult& result)
    {
        std::ostringstream out;
        out << "editor-autosave-tick triggered=" << (result.triggered ? 1 : 0)
            << " scene=" << (result.shouldSaveScene ? 1 : 0)
            << " backup=" << (result.shouldCreateBackup ? 1 : 0)
            << " workspace=" << (result.shouldSaveWorkspace ? 1 : 0)
            << " actions=" << result.actions.size()
            << " skip=" << ToString(result.skipReason)
            << " ok=" << (result.ok ? 1 : 0);
        return out.str();
    }

    std::string FormatEditorBackupRotationPlan(const EditorBackupRotationPlan& plan)
    {
        std::ostringstream out;
        out << "editor-backup-rotation keep=" << plan.keep.size()
            << " prune=" << plan.prune.size()
            << " max=" << plan.maxBackupCount
            << " ok=" << (plan.ok ? 1 : 0);
        return out.str();
    }

    EditorAutosaveDiagnostics RunEditorAutosaveDiagnostics()
    {
        EditorAutosaveDiagnostics diagnostics{};
        EditorPreferencesProfile prefs = BuildDefaultEditorPreferencesProfile();
        prefs.autosave.intervalSeconds = 2;
        prefs.autosave.maxBackupCount = 3;
        prefs.autosave.saveLayoutWithProject = true;
        prefs.autosave.saveSessionOnExit = true;
        prefs.safety.createCrashRecoveryBackup = true;

        diagnostics.policy = BuildEditorAutosavePolicyFromPreferences(prefs);
        EditorWorkspacePaths workspacePaths = BuildEditorWorkspacePaths("/tmp/ak_editor_autosave_probe", prefs);
        diagnostics.paths = BuildEditorAutosavePaths(workspacePaths, "/tmp/ak_editor_autosave_probe/autosave.akscene");

        EditorAutosaveRuntimeState state{};
        EditorAutosaveTickInput input{};
        input.deltaSeconds = 0.5;
        input.sceneDirty = true;
        input.sceneRevision = 7;
        input.workspaceDirty = true;
        input.workspaceRevision = 11;
        diagnostics.firstTick = TickEditorAutosaveRuntime(diagnostics.policy, diagnostics.paths, state, input, "Sandbox");

        input.deltaSeconds = 2.0;
        diagnostics.secondTick = TickEditorAutosaveRuntime(diagnostics.policy, diagnostics.paths, state, input, "Sandbox");
        diagnostics.sceneAction = diagnostics.secondTick.shouldSaveScene;
        diagnostics.workspaceAction = diagnostics.secondTick.shouldSaveWorkspace;
        diagnostics.backupAction = diagnostics.secondTick.shouldCreateBackup;
        diagnostics.skipOk = diagnostics.firstTick.skipReason == EditorAutosaveSkipReason::IntervalNotReached;

        diagnostics.rotation = BuildEditorBackupRotationPlan({"backup_001.akscene", "backup_002.akscene", "backup_003.akscene", "backup_004.akscene", "backup_005.akscene"}, diagnostics.policy.maxBackupCount);
        diagnostics.rotationOk = diagnostics.rotation.ok && diagnostics.rotation.keep.size() == 3 && diagnostics.rotation.prune.size() == 2 && diagnostics.rotation.prune.front() == "backup_001.akscene";
        diagnostics.ok = diagnostics.sceneAction && diagnostics.workspaceAction && diagnostics.backupAction && diagnostics.skipOk && diagnostics.rotationOk;
        diagnostics.summary = FormatEditorAutosaveDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorAutosaveDiagnostics(const EditorAutosaveDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-autosave scene=" << (diagnostics.sceneAction ? 1 : 0)
            << " workspace=" << (diagnostics.workspaceAction ? 1 : 0)
            << " backup=" << (diagnostics.backupAction ? 1 : 0)
            << " skip=" << (diagnostics.skipOk ? 1 : 0)
            << " rotation=" << (diagnostics.rotationOk ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        return out.str();
    }
}
