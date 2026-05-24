#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorTheme.hpp>
#include <AK/EditorUI/EditorShortcuts.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorStartupMode
    {
        EmptyProject,
        LastProject,
        ProjectBrowser
    };

    enum class EditorPreferencesRepairCode
    {
        None,
        UnsupportedVersion,
        ParseFailed,
        InvalidThemeProfile,
        InvalidStartupMode,
        InvalidDpiScale,
        InvalidAutosaveInterval,
        InvalidBackupCount,
        InvalidScrollLines,
        InvalidAssetIconScale,
        InvalidCameraSensitivity,
        EmptyRecentProject,
        DuplicateRecentProject,
        ResetToDefault
    };

    enum class EditorPreferencesRepairSeverity
    {
        Info,
        Warning,
        Error
    };

    struct EditorAutosavePreferences
    {
        bool enabled = true;
        i32 intervalSeconds = 120;
        i32 maxBackupCount = 8;
        bool saveLayoutWithProject = true;
        bool saveSessionOnExit = true;
    };

    struct EditorUiPreferences
    {
        EditorThemeProfile themeProfile = EditorThemeProfile::UnityLikeDark;
        float dpiScale = 1.0f;
        bool useSystemDpi = true;
        bool showTooltips = true;
        bool showStatusBar = true;
        bool compactToolbar = true;
        bool restoreLastLayout = true;
        bool restoreLastSession = true;
        bool showDebugUiDiagnostics = false;
        i32 mouseWheelLines = 3;
        float assetIconScale = 1.0f;
    };

    struct EditorViewportPreferences
    {
        bool showGrid = true;
        bool showGizmos = true;
        bool frameStatsVisible = false;
        bool focusSelectedOnF = true;
        float cameraMoveSpeed = 1.0f;
        float cameraLookSensitivity = 1.0f;
    };

    struct EditorSafetyPreferences
    {
        bool confirmDelete = true;
        bool confirmSceneClose = true;
        bool atomicSave = true;
        bool createCrashRecoveryBackup = true;
        bool blockUnsafeTextShortcuts = true;
    };

    struct EditorPreferencesProfile
    {
        u32 schemaVersion = 1;
        std::string profileName = "Default";
        EditorStartupMode startupMode = EditorStartupMode::LastProject;
        std::string projectRoot = "projects/Sandbox";
        std::string layoutPath = ".akcache/editor/layout.akeditorlayout";
        std::string sessionPath = ".akcache/editor/session.akeditorsession";
        std::string shortcutPath = ".akcache/editor/shortcuts.akeditorshortcuts";
        std::vector<std::string> recentProjects;
        EditorAutosavePreferences autosave{};
        EditorUiPreferences ui{};
        EditorViewportPreferences viewport{};
        EditorSafetyPreferences safety{};
        u64 revision = 1;
    };

    struct EditorPreferencesRepairIssue
    {
        EditorPreferencesRepairCode code = EditorPreferencesRepairCode::None;
        EditorPreferencesRepairSeverity severity = EditorPreferencesRepairSeverity::Info;
        std::string field;
        std::string message;
    };

    struct EditorPreferencesRepairReport
    {
        std::vector<EditorPreferencesRepairIssue> issues;
        bool repaired = false;
        bool resetToDefault = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorPreferencesLoadResult
    {
        EditorPreferencesProfile profile{};
        EditorPreferencesRepairReport repair{};
        bool loadedFromText = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorStartupPlan
    {
        EditorTheme theme{};
        EditorShortcutProfile shortcutProfile{};
        std::string layoutPath;
        std::string sessionPath;
        std::string projectRoot;
        bool loadLayout = true;
        bool loadSession = true;
        bool saveSessionOnExit = true;
        bool autosaveEnabled = true;
        bool blockUnsafeTextShortcuts = true;
        bool ok = false;
        std::string summary;
    };

    struct EditorPreferencesDiagnostics
    {
        usize recentProjectCount = 0;
        usize repairIssueCount = 0;
        bool serialized = false;
        bool loaded = false;
        bool repaired = false;
        bool startupPlanOk = false;
        bool themeOk = false;
        bool shortcutsOk = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorPreferencesPolicy
    {
        usize maxRecentProjects = 16;
        float minDpiScale = 0.75f;
        float maxDpiScale = 2.50f;
        i32 minAutosaveIntervalSeconds = 15;
        i32 maxAutosaveIntervalSeconds = 3600;
        i32 minBackupCount = 1;
        i32 maxBackupCount = 64;
        i32 minMouseWheelLines = 1;
        i32 maxMouseWheelLines = 16;
        float minAssetIconScale = 0.50f;
        float maxAssetIconScale = 2.00f;
        float minCameraSensitivity = 0.05f;
        float maxCameraSensitivity = 10.00f;
        bool resetOnUnsupportedVersion = true;
    };

    const char* ToString(EditorStartupMode mode);
    const char* ToString(EditorPreferencesRepairCode code);
    const char* ToString(EditorPreferencesRepairSeverity severity);

    EditorPreferencesPolicy MakeDefaultEditorPreferencesPolicy();
    EditorPreferencesProfile BuildDefaultEditorPreferencesProfile();
    std::string SerializeEditorPreferencesProfile(const EditorPreferencesProfile& profile);
    EditorPreferencesLoadResult LoadEditorPreferencesProfileWithRepair(std::string_view text, const EditorPreferencesPolicy& policy = MakeDefaultEditorPreferencesPolicy());
    EditorPreferencesRepairReport RepairEditorPreferencesProfile(EditorPreferencesProfile& profile, const EditorPreferencesPolicy& policy = MakeDefaultEditorPreferencesPolicy());

    bool AddRecentEditorProject(EditorPreferencesProfile& profile, std::string projectRoot, const EditorPreferencesPolicy& policy = MakeDefaultEditorPreferencesPolicy());
    EditorTheme BuildEditorThemeFromPreferences(const EditorPreferencesProfile& profile);
    EditorStartupPlan BuildEditorStartupPlan(const EditorPreferencesProfile& profile, const EditorShortcutProfile& shortcuts = BuildDefaultEditorShortcutProfile());

    std::string FormatEditorPreferencesRepairIssue(const EditorPreferencesRepairIssue& issue);
    std::string FormatEditorPreferencesRepairReport(const EditorPreferencesRepairReport& report);
    std::string FormatEditorPreferencesSummary(const EditorPreferencesProfile& profile);
    EditorPreferencesDiagnostics RunEditorPreferencesDiagnostics();
    std::string FormatEditorPreferencesDiagnostics(const EditorPreferencesDiagnostics& diagnostics);
}
