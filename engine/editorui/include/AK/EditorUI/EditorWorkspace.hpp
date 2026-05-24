#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorLayoutPersistence.hpp>
#include <AK/EditorUI/EditorPreferences.hpp>
#include <AK/EditorUI/EditorSessionState.hpp>
#include <AK/EditorUI/EditorShortcuts.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorWorkspaceFileKind
    {
        Preferences,
        Layout,
        Session,
        Shortcuts,
        AutosaveDirectory,
        BackupDirectory
    };

    enum class EditorWorkspaceIssueCode
    {
        None,
        MissingDirectory,
        DirectoryCreateFailed,
        MissingFile,
        ReadFailed,
        WriteFailed,
        AtomicTempWriteFailed,
        AtomicRenameFailed,
        PreferencesRepaired,
        LayoutRepaired,
        SessionRepaired,
        ShortcutsRepaired,
        DefaultsUsed
    };

    enum class EditorWorkspaceIssueSeverity
    {
        Info,
        Warning,
        Error
    };

    struct EditorWorkspacePaths
    {
        std::string projectRoot = ".";
        std::string cacheDirectory = ".akcache/editor";
        std::string preferencesPath = ".akcache/editor/preferences.akeditorprefs";
        std::string layoutPath = ".akcache/editor/layout.akeditorlayout";
        std::string sessionPath = ".akcache/editor/session.akeditorsession";
        std::string shortcutPath = ".akcache/editor/shortcuts.akeditorshortcuts";
        std::string autosaveDirectory = ".akcache/editor/autosaves";
        std::string backupDirectory = ".akcache/editor/backups";
    };

    struct EditorWorkspaceIssue
    {
        EditorWorkspaceIssueCode code = EditorWorkspaceIssueCode::None;
        EditorWorkspaceIssueSeverity severity = EditorWorkspaceIssueSeverity::Info;
        EditorWorkspaceFileKind fileKind = EditorWorkspaceFileKind::Preferences;
        std::string path;
        std::string message;
    };

    struct EditorWorkspaceReport
    {
        std::vector<EditorWorkspaceIssue> issues;
        bool repaired = false;
        bool usedDefaults = false;
        bool hasErrors = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorWorkspaceLoadResult
    {
        EditorWorkspacePaths paths{};
        EditorPreferencesLoadResult preferences{};
        EditorShortcutLoadResult shortcuts{};
        EditorLayoutLoadResult layout{};
        EditorSessionLoadResult session{};
        EditorStartupPlan startup{};
        EditorWorkspaceReport report{};
        bool loadedPreferences = false;
        bool loadedShortcuts = false;
        bool loadedLayout = false;
        bool loadedSession = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorWorkspaceSaveInput
    {
        EditorWorkspacePaths paths{};
        const EditorPreferencesProfile* preferences = nullptr;
        const EditorShortcutProfile* shortcuts = nullptr;
        const EditorFrameLayout* layout = nullptr;
        const EditorWindowPlacement* placement = nullptr;
        const EditorSessionState* session = nullptr;
        bool savePreferences = true;
        bool saveShortcuts = true;
        bool saveLayout = true;
        bool saveSession = true;
    };

    struct EditorWorkspaceSaveResult
    {
        EditorWorkspaceReport report{};
        usize filesWritten = 0;
        bool preferencesSaved = false;
        bool shortcutsSaved = false;
        bool layoutSaved = false;
        bool sessionSaved = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorWorkspaceDiagnostics
    {
        bool directoriesOk = false;
        bool saved = false;
        bool loaded = false;
        bool repaired = false;
        bool usedDefaults = false;
        usize filesWritten = 0;
        usize issueCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorWorkspaceFileKind kind);
    const char* ToString(EditorWorkspaceIssueCode code);
    const char* ToString(EditorWorkspaceIssueSeverity severity);

    EditorWorkspacePaths BuildEditorWorkspacePaths(std::string projectRoot, const EditorPreferencesProfile& preferences = BuildDefaultEditorPreferencesProfile());
    EditorWorkspaceReport EnsureEditorWorkspaceDirectories(const EditorWorkspacePaths& paths);

    bool ReadEditorWorkspaceTextFile(const std::string& path, std::string& outText);
    bool AtomicWriteEditorWorkspaceTextFile(const std::string& path, std::string_view text);

    EditorWorkspaceLoadResult LoadEditorWorkspace(const EditorWorkspacePaths& seedPaths,
                                                  const EditorPanelRegistry& panels,
                                                  i32 fallbackWidth,
                                                  i32 fallbackHeight,
                                                  const std::vector<EditorMonitorBounds>& monitors = MakeDefaultEditorMonitorSet(),
                                                  const std::vector<std::string>& validStableIds = {});

    EditorWorkspaceSaveResult SaveEditorWorkspace(const EditorWorkspaceSaveInput& input);

    std::string FormatEditorWorkspaceIssue(const EditorWorkspaceIssue& issue);
    std::string FormatEditorWorkspaceReport(const EditorWorkspaceReport& report);
    EditorWorkspaceDiagnostics RunEditorWorkspaceDiagnostics();
    std::string FormatEditorWorkspaceDiagnostics(const EditorWorkspaceDiagnostics& diagnostics);
}
