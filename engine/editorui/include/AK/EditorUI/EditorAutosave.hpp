#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorPreferences.hpp>
#include <AK/EditorUI/EditorWorkspace.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorAutosaveActionKind
    {
        None,
        SceneAutosave,
        SceneBackup,
        WorkspaceSave,
        PruneBackup
    };

    enum class EditorAutosaveSkipReason
    {
        None,
        Disabled,
        EditorInactive,
        IntervalNotReached,
        NothingDirty,
        AlreadySavedRevision
    };

    struct EditorAutosavePolicy
    {
        bool enabled = true;
        bool saveScene = true;
        bool saveWorkspace = true;
        bool createSceneBackup = true;
        double intervalSeconds = 120.0;
        i32 maxBackupCount = 8;
        double minimumIntervalSeconds = 1.0;
    };

    struct EditorAutosavePaths
    {
        std::string sceneAutosavePath;
        std::string backupDirectory;
        std::string workspaceLayoutPath;
        std::string workspaceSessionPath;
        std::string workspacePreferencesPath;
        std::string workspaceShortcutsPath;
    };

    struct EditorAutosaveRuntimeState
    {
        double accumulatedSeconds = 0.0;
        u64 lastSceneRevision = 0;
        u64 lastWorkspaceRevision = 0;
        u64 triggerCount = 0;
        u64 sceneAutosaveCount = 0;
        u64 sceneBackupCount = 0;
        u64 workspaceSaveCount = 0;
        u64 skippedCount = 0;
        EditorAutosaveSkipReason lastSkipReason = EditorAutosaveSkipReason::None;
    };

    struct EditorAutosaveTickInput
    {
        double deltaSeconds = 0.0;
        bool editorActive = true;
        bool sceneDirty = false;
        u64 sceneRevision = 0;
        bool workspaceDirty = false;
        u64 workspaceRevision = 0;
    };

    struct EditorAutosaveAction
    {
        EditorAutosaveActionKind kind = EditorAutosaveActionKind::None;
        std::string path;
        std::string reason;
        u64 revision = 0;
        bool required = false;
    };

    struct EditorAutosaveTickResult
    {
        std::vector<EditorAutosaveAction> actions;
        EditorAutosaveSkipReason skipReason = EditorAutosaveSkipReason::None;
        bool triggered = false;
        bool shouldSaveScene = false;
        bool shouldCreateBackup = false;
        bool shouldSaveWorkspace = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorBackupRotationPlan
    {
        std::vector<std::string> keep;
        std::vector<std::string> prune;
        i32 maxBackupCount = 0;
        bool ok = false;
        std::string summary;
    };

    struct EditorAutosaveDiagnostics
    {
        EditorAutosavePolicy policy{};
        EditorAutosavePaths paths{};
        EditorAutosaveTickResult firstTick{};
        EditorAutosaveTickResult secondTick{};
        EditorBackupRotationPlan rotation{};
        bool sceneAction = false;
        bool workspaceAction = false;
        bool backupAction = false;
        bool skipOk = false;
        bool rotationOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorAutosaveActionKind kind);
    const char* ToString(EditorAutosaveSkipReason reason);

    EditorAutosavePolicy BuildEditorAutosavePolicyFromPreferences(const EditorPreferencesProfile& preferences);
    EditorAutosaveRuntimeState MakeDefaultEditorAutosaveRuntimeState();
    EditorAutosavePaths BuildEditorAutosavePaths(const EditorWorkspacePaths& workspacePaths, std::string sceneAutosavePath = {});

    std::string BuildEditorSceneBackupFileName(std::string_view sceneStem, u64 revision, u64 triggerCount);
    std::string BuildEditorSceneBackupPath(const EditorAutosavePaths& paths, std::string_view sceneStem, u64 revision, u64 triggerCount);

    EditorAutosaveTickResult TickEditorAutosaveRuntime(const EditorAutosavePolicy& policy,
                                                       const EditorAutosavePaths& paths,
                                                       EditorAutosaveRuntimeState& state,
                                                       const EditorAutosaveTickInput& input,
                                                       std::string_view sceneStem = "scene");

    void NotifyEditorAutosaveSceneSaved(EditorAutosaveRuntimeState& state, u64 revision);
    void NotifyEditorAutosaveWorkspaceSaved(EditorAutosaveRuntimeState& state, u64 revision);

    EditorBackupRotationPlan BuildEditorBackupRotationPlan(std::vector<std::string> existingBackups, i32 maxBackupCount);

    std::string FormatEditorAutosavePolicy(const EditorAutosavePolicy& policy);
    std::string FormatEditorAutosavePaths(const EditorAutosavePaths& paths);
    std::string FormatEditorAutosaveAction(const EditorAutosaveAction& action);
    std::string FormatEditorAutosaveTickResult(const EditorAutosaveTickResult& result);
    std::string FormatEditorBackupRotationPlan(const EditorBackupRotationPlan& plan);

    EditorAutosaveDiagnostics RunEditorAutosaveDiagnostics();
    std::string FormatEditorAutosaveDiagnostics(const EditorAutosaveDiagnostics& diagnostics);
}
