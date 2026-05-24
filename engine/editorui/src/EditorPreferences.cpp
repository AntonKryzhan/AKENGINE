#include <AK/EditorUI/EditorPreferences.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>

namespace AK
{
    namespace
    {
        constexpr int ToInt(EditorStartupMode mode) { return static_cast<int>(mode); }
        constexpr int ToInt(EditorThemeProfile profile) { return static_cast<int>(profile); }

        bool IsValidStartupMode(int value)
        {
            return value >= 0 && value <= static_cast<int>(EditorStartupMode::ProjectBrowser);
        }

        bool IsValidThemeProfile(int value)
        {
            return value >= 0 && value <= static_cast<int>(EditorThemeProfile::LightPreview);
        }

        bool ParseBool(int value)
        {
            return value != 0;
        }

        template <typename T>
        T Clamp(T value, T lo, T hi)
        {
            return std::max(lo, std::min(value, hi));
        }


        std::string BoolString(bool value)
        {
            return value ? "1" : "0";
        }

        bool EmptyOrWhitespace(const std::string& value)
        {
            return std::all_of(value.begin(), value.end(), [](unsigned char ch)
            {
                return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
            });
        }

        void AddIssue(EditorPreferencesRepairReport& report, EditorPreferencesRepairCode code, EditorPreferencesRepairSeverity severity, std::string field, std::string message)
        {
            EditorPreferencesRepairIssue issue{};
            issue.code = code;
            issue.severity = severity;
            issue.field = std::move(field);
            issue.message = std::move(message);
            report.issues.push_back(std::move(issue));
            if (code != EditorPreferencesRepairCode::None)
            {
                report.repaired = true;
            }
        }

        std::string SanitizePath(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            while (path.find("//") != std::string::npos)
            {
                const std::size_t pos = path.find("//");
                path.erase(pos, 1);
            }
            if (!path.empty() && path.front() == ' ')
            {
                path.erase(0, path.find_first_not_of(' '));
            }
            while (!path.empty() && path.back() == ' ')
            {
                path.pop_back();
            }
            return path;
        }

        void EnsureDefaultPaths(EditorPreferencesProfile& profile)
        {
            if (profile.projectRoot.empty())
            {
                profile.projectRoot = "projects/Sandbox";
            }
            if (profile.layoutPath.empty())
            {
                profile.layoutPath = ".akcache/editor/layout.akeditorlayout";
            }
            if (profile.sessionPath.empty())
            {
                profile.sessionPath = ".akcache/editor/session.akeditorsession";
            }
            if (profile.shortcutPath.empty())
            {
                profile.shortcutPath = ".akcache/editor/shortcuts.akeditorshortcuts";
            }
            profile.projectRoot = SanitizePath(profile.projectRoot);
            profile.layoutPath = SanitizePath(profile.layoutPath);
            profile.sessionPath = SanitizePath(profile.sessionPath);
            profile.shortcutPath = SanitizePath(profile.shortcutPath);
        }

        bool ParseEditorPreferencesProfile(std::string_view text, EditorPreferencesProfile& profile)
        {
            std::istringstream input{std::string(text)};
            std::string magic;
            u32 version = 0;
            input >> magic >> version;
            if (magic != "AKEDITORPREFS" || version != 1)
            {
                return false;
            }

            profile = {};
            profile.schemaVersion = version;

            std::string token;
            while (input >> token)
            {
                if (token == "profile")
                {
                    input >> std::quoted(profile.profileName);
                }
                else if (token == "startup")
                {
                    int mode = 0;
                    input >> mode;
                    profile.startupMode = static_cast<EditorStartupMode>(mode);
                }
                else if (token == "project")
                {
                    input >> std::quoted(profile.projectRoot);
                }
                else if (token == "layout")
                {
                    input >> std::quoted(profile.layoutPath);
                }
                else if (token == "session")
                {
                    input >> std::quoted(profile.sessionPath);
                }
                else if (token == "shortcuts")
                {
                    input >> std::quoted(profile.shortcutPath);
                }
                else if (token == "recent")
                {
                    std::string recent;
                    input >> std::quoted(recent);
                    profile.recentProjects.push_back(std::move(recent));
                }
                else if (token == "ui")
                {
                    int theme = 0;
                    int systemDpi = 1;
                    int tooltips = 1;
                    int status = 1;
                    int compactToolbar = 1;
                    int restoreLayout = 1;
                    int restoreSession = 1;
                    int debugUi = 0;
                    input >> theme
                          >> profile.ui.dpiScale
                          >> systemDpi
                          >> tooltips
                          >> status
                          >> compactToolbar
                          >> restoreLayout
                          >> restoreSession
                          >> debugUi
                          >> profile.ui.mouseWheelLines
                          >> profile.ui.assetIconScale;
                    profile.ui.themeProfile = static_cast<EditorThemeProfile>(theme);
                    profile.ui.useSystemDpi = ParseBool(systemDpi);
                    profile.ui.showTooltips = ParseBool(tooltips);
                    profile.ui.showStatusBar = ParseBool(status);
                    profile.ui.compactToolbar = ParseBool(compactToolbar);
                    profile.ui.restoreLastLayout = ParseBool(restoreLayout);
                    profile.ui.restoreLastSession = ParseBool(restoreSession);
                    profile.ui.showDebugUiDiagnostics = ParseBool(debugUi);
                }
                else if (token == "autosave")
                {
                    int enabled = 1;
                    int saveLayout = 1;
                    int saveSession = 1;
                    input >> enabled
                          >> profile.autosave.intervalSeconds
                          >> profile.autosave.maxBackupCount
                          >> saveLayout
                          >> saveSession;
                    profile.autosave.enabled = ParseBool(enabled);
                    profile.autosave.saveLayoutWithProject = ParseBool(saveLayout);
                    profile.autosave.saveSessionOnExit = ParseBool(saveSession);
                }
                else if (token == "viewport")
                {
                    int grid = 1;
                    int gizmos = 1;
                    int stats = 0;
                    int focus = 1;
                    input >> grid
                          >> gizmos
                          >> stats
                          >> focus
                          >> profile.viewport.cameraMoveSpeed
                          >> profile.viewport.cameraLookSensitivity;
                    profile.viewport.showGrid = ParseBool(grid);
                    profile.viewport.showGizmos = ParseBool(gizmos);
                    profile.viewport.frameStatsVisible = ParseBool(stats);
                    profile.viewport.focusSelectedOnF = ParseBool(focus);
                }
                else if (token == "safety")
                {
                    int confirmDelete = 1;
                    int confirmClose = 1;
                    int atomicSave = 1;
                    int crashRecovery = 1;
                    int blockUnsafe = 1;
                    input >> confirmDelete >> confirmClose >> atomicSave >> crashRecovery >> blockUnsafe;
                    profile.safety.confirmDelete = ParseBool(confirmDelete);
                    profile.safety.confirmSceneClose = ParseBool(confirmClose);
                    profile.safety.atomicSave = ParseBool(atomicSave);
                    profile.safety.createCrashRecoveryBackup = ParseBool(crashRecovery);
                    profile.safety.blockUnsafeTextShortcuts = ParseBool(blockUnsafe);
                }
                else if (token == "revision")
                {
                    input >> profile.revision;
                }
            }

            return true;
        }
    }

    const char* ToString(EditorStartupMode mode)
    {
        switch (mode)
        {
            case EditorStartupMode::EmptyProject: return "EmptyProject";
            case EditorStartupMode::LastProject: return "LastProject";
            case EditorStartupMode::ProjectBrowser: return "ProjectBrowser";
        }
        return "Unknown";
    }

    const char* ToString(EditorPreferencesRepairCode code)
    {
        switch (code)
        {
            case EditorPreferencesRepairCode::None: return "None";
            case EditorPreferencesRepairCode::UnsupportedVersion: return "UnsupportedVersion";
            case EditorPreferencesRepairCode::ParseFailed: return "ParseFailed";
            case EditorPreferencesRepairCode::InvalidThemeProfile: return "InvalidThemeProfile";
            case EditorPreferencesRepairCode::InvalidStartupMode: return "InvalidStartupMode";
            case EditorPreferencesRepairCode::InvalidDpiScale: return "InvalidDpiScale";
            case EditorPreferencesRepairCode::InvalidAutosaveInterval: return "InvalidAutosaveInterval";
            case EditorPreferencesRepairCode::InvalidBackupCount: return "InvalidBackupCount";
            case EditorPreferencesRepairCode::InvalidScrollLines: return "InvalidScrollLines";
            case EditorPreferencesRepairCode::InvalidAssetIconScale: return "InvalidAssetIconScale";
            case EditorPreferencesRepairCode::InvalidCameraSensitivity: return "InvalidCameraSensitivity";
            case EditorPreferencesRepairCode::EmptyRecentProject: return "EmptyRecentProject";
            case EditorPreferencesRepairCode::DuplicateRecentProject: return "DuplicateRecentProject";
            case EditorPreferencesRepairCode::ResetToDefault: return "ResetToDefault";
        }
        return "Unknown";
    }

    const char* ToString(EditorPreferencesRepairSeverity severity)
    {
        switch (severity)
        {
            case EditorPreferencesRepairSeverity::Info: return "Info";
            case EditorPreferencesRepairSeverity::Warning: return "Warning";
            case EditorPreferencesRepairSeverity::Error: return "Error";
        }
        return "Unknown";
    }

    EditorPreferencesPolicy MakeDefaultEditorPreferencesPolicy()
    {
        return {};
    }

    EditorPreferencesProfile BuildDefaultEditorPreferencesProfile()
    {
        EditorPreferencesProfile profile{};
        profile.recentProjects.push_back("projects/Sandbox");
        return profile;
    }

    std::string SerializeEditorPreferencesProfile(const EditorPreferencesProfile& profile)
    {
        std::ostringstream out;
        out << "AKEDITORPREFS " << profile.schemaVersion << '\n';
        out << "profile " << std::quoted(profile.profileName) << '\n';
        out << "startup " << ToInt(profile.startupMode) << '\n';
        out << "project " << std::quoted(profile.projectRoot) << '\n';
        out << "layout " << std::quoted(profile.layoutPath) << '\n';
        out << "session " << std::quoted(profile.sessionPath) << '\n';
        out << "shortcuts " << std::quoted(profile.shortcutPath) << '\n';
        for (const std::string& recent : profile.recentProjects)
        {
            out << "recent " << std::quoted(recent) << '\n';
        }
        out << "ui " << ToInt(profile.ui.themeProfile)
            << ' ' << std::fixed << std::setprecision(3) << profile.ui.dpiScale
            << ' ' << BoolString(profile.ui.useSystemDpi)
            << ' ' << BoolString(profile.ui.showTooltips)
            << ' ' << BoolString(profile.ui.showStatusBar)
            << ' ' << BoolString(profile.ui.compactToolbar)
            << ' ' << BoolString(profile.ui.restoreLastLayout)
            << ' ' << BoolString(profile.ui.restoreLastSession)
            << ' ' << BoolString(profile.ui.showDebugUiDiagnostics)
            << ' ' << profile.ui.mouseWheelLines
            << ' ' << std::fixed << std::setprecision(3) << profile.ui.assetIconScale << '\n';
        out << "autosave " << BoolString(profile.autosave.enabled)
            << ' ' << profile.autosave.intervalSeconds
            << ' ' << profile.autosave.maxBackupCount
            << ' ' << BoolString(profile.autosave.saveLayoutWithProject)
            << ' ' << BoolString(profile.autosave.saveSessionOnExit) << '\n';
        out << "viewport " << BoolString(profile.viewport.showGrid)
            << ' ' << BoolString(profile.viewport.showGizmos)
            << ' ' << BoolString(profile.viewport.frameStatsVisible)
            << ' ' << BoolString(profile.viewport.focusSelectedOnF)
            << ' ' << std::fixed << std::setprecision(3) << profile.viewport.cameraMoveSpeed
            << ' ' << std::fixed << std::setprecision(3) << profile.viewport.cameraLookSensitivity << '\n';
        out << "safety " << BoolString(profile.safety.confirmDelete)
            << ' ' << BoolString(profile.safety.confirmSceneClose)
            << ' ' << BoolString(profile.safety.atomicSave)
            << ' ' << BoolString(profile.safety.createCrashRecoveryBackup)
            << ' ' << BoolString(profile.safety.blockUnsafeTextShortcuts) << '\n';
        out << "revision " << profile.revision << '\n';
        return out.str();
    }

    EditorPreferencesRepairReport RepairEditorPreferencesProfile(EditorPreferencesProfile& profile, const EditorPreferencesPolicy& policy)
    {
        EditorPreferencesRepairReport report{};
        if (profile.schemaVersion != 1)
        {
            if (policy.resetOnUnsupportedVersion)
            {
                AddIssue(report, EditorPreferencesRepairCode::UnsupportedVersion, EditorPreferencesRepairSeverity::Error, "schemaVersion", "unsupported editor preferences version; reset to defaults");
                profile = BuildDefaultEditorPreferencesProfile();
                report.resetToDefault = true;
            }
        }

        if (!IsValidStartupMode(ToInt(profile.startupMode)))
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidStartupMode, EditorPreferencesRepairSeverity::Warning, "startupMode", "invalid startup mode reset to LastProject");
            profile.startupMode = EditorStartupMode::LastProject;
        }

        if (!IsValidThemeProfile(ToInt(profile.ui.themeProfile)))
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidThemeProfile, EditorPreferencesRepairSeverity::Warning, "ui.themeProfile", "invalid theme profile reset to UnityLikeDark");
            profile.ui.themeProfile = EditorThemeProfile::UnityLikeDark;
        }

        if (!std::isfinite(profile.ui.dpiScale) || profile.ui.dpiScale < policy.minDpiScale || profile.ui.dpiScale > policy.maxDpiScale)
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidDpiScale, EditorPreferencesRepairSeverity::Warning, "ui.dpiScale", "dpi scale clamped to supported range");
            profile.ui.dpiScale = Clamp(std::isfinite(profile.ui.dpiScale) ? profile.ui.dpiScale : 1.0f, policy.minDpiScale, policy.maxDpiScale);
        }

        if (profile.autosave.intervalSeconds < policy.minAutosaveIntervalSeconds || profile.autosave.intervalSeconds > policy.maxAutosaveIntervalSeconds)
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidAutosaveInterval, EditorPreferencesRepairSeverity::Warning, "autosave.intervalSeconds", "autosave interval clamped to supported range");
            profile.autosave.intervalSeconds = Clamp(profile.autosave.intervalSeconds, policy.minAutosaveIntervalSeconds, policy.maxAutosaveIntervalSeconds);
        }

        if (profile.autosave.maxBackupCount < policy.minBackupCount || profile.autosave.maxBackupCount > policy.maxBackupCount)
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidBackupCount, EditorPreferencesRepairSeverity::Warning, "autosave.maxBackupCount", "autosave backup count clamped to supported range");
            profile.autosave.maxBackupCount = Clamp(profile.autosave.maxBackupCount, policy.minBackupCount, policy.maxBackupCount);
        }

        if (profile.ui.mouseWheelLines < policy.minMouseWheelLines || profile.ui.mouseWheelLines > policy.maxMouseWheelLines)
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidScrollLines, EditorPreferencesRepairSeverity::Warning, "ui.mouseWheelLines", "mouse wheel line count clamped");
            profile.ui.mouseWheelLines = Clamp(profile.ui.mouseWheelLines, policy.minMouseWheelLines, policy.maxMouseWheelLines);
        }

        if (!std::isfinite(profile.ui.assetIconScale) || profile.ui.assetIconScale < policy.minAssetIconScale || profile.ui.assetIconScale > policy.maxAssetIconScale)
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidAssetIconScale, EditorPreferencesRepairSeverity::Warning, "ui.assetIconScale", "asset icon scale clamped");
            profile.ui.assetIconScale = Clamp(std::isfinite(profile.ui.assetIconScale) ? profile.ui.assetIconScale : 1.0f, policy.minAssetIconScale, policy.maxAssetIconScale);
        }

        if (!std::isfinite(profile.viewport.cameraMoveSpeed) || profile.viewport.cameraMoveSpeed < policy.minCameraSensitivity || profile.viewport.cameraMoveSpeed > policy.maxCameraSensitivity)
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidCameraSensitivity, EditorPreferencesRepairSeverity::Warning, "viewport.cameraMoveSpeed", "camera move speed clamped");
            profile.viewport.cameraMoveSpeed = Clamp(std::isfinite(profile.viewport.cameraMoveSpeed) ? profile.viewport.cameraMoveSpeed : 1.0f, policy.minCameraSensitivity, policy.maxCameraSensitivity);
        }

        if (!std::isfinite(profile.viewport.cameraLookSensitivity) || profile.viewport.cameraLookSensitivity < policy.minCameraSensitivity || profile.viewport.cameraLookSensitivity > policy.maxCameraSensitivity)
        {
            AddIssue(report, EditorPreferencesRepairCode::InvalidCameraSensitivity, EditorPreferencesRepairSeverity::Warning, "viewport.cameraLookSensitivity", "camera look sensitivity clamped");
            profile.viewport.cameraLookSensitivity = Clamp(std::isfinite(profile.viewport.cameraLookSensitivity) ? profile.viewport.cameraLookSensitivity : 1.0f, policy.minCameraSensitivity, policy.maxCameraSensitivity);
        }

        EnsureDefaultPaths(profile);

        std::vector<std::string> cleaned;
        std::unordered_set<std::string> seen;
        cleaned.reserve(profile.recentProjects.size());
        for (std::string recent : profile.recentProjects)
        {
            recent = SanitizePath(std::move(recent));
            if (recent.empty() || EmptyOrWhitespace(recent))
            {
                AddIssue(report, EditorPreferencesRepairCode::EmptyRecentProject, EditorPreferencesRepairSeverity::Info, "recentProjects", "empty recent project removed");
                continue;
            }
            if (!seen.insert(recent).second)
            {
                AddIssue(report, EditorPreferencesRepairCode::DuplicateRecentProject, EditorPreferencesRepairSeverity::Info, "recentProjects", "duplicate recent project removed");
                continue;
            }
            cleaned.push_back(std::move(recent));
            if (cleaned.size() >= policy.maxRecentProjects)
            {
                break;
            }
        }
        profile.recentProjects = std::move(cleaned);
        if (profile.recentProjects.empty())
        {
            profile.recentProjects.push_back(profile.projectRoot);
        }

        if (profile.revision == 0)
        {
            profile.revision = 1;
        }

        report.ok = true;
        report.summary = FormatEditorPreferencesRepairReport(report);
        return report;
    }

    EditorPreferencesLoadResult LoadEditorPreferencesProfileWithRepair(std::string_view text, const EditorPreferencesPolicy& policy)
    {
        EditorPreferencesLoadResult result{};
        EditorPreferencesProfile profile{};
        if (!ParseEditorPreferencesProfile(text, profile))
        {
            result.profile = BuildDefaultEditorPreferencesProfile();
            AddIssue(result.repair, EditorPreferencesRepairCode::ParseFailed, EditorPreferencesRepairSeverity::Error, "document", "failed to parse editor preferences; reset to defaults");
            AddIssue(result.repair, EditorPreferencesRepairCode::ResetToDefault, EditorPreferencesRepairSeverity::Info, "document", "default editor preferences restored");
            result.repair.resetToDefault = true;
            result.repair.ok = true;
            result.repair.summary = FormatEditorPreferencesRepairReport(result.repair);
            result.ok = true;
            result.summary = result.repair.summary;
            return result;
        }

        result.loadedFromText = true;
        result.profile = std::move(profile);
        result.repair = RepairEditorPreferencesProfile(result.profile, policy);
        result.ok = result.repair.ok;
        result.summary = result.repair.summary;
        return result;
    }

    bool AddRecentEditorProject(EditorPreferencesProfile& profile, std::string projectRoot, const EditorPreferencesPolicy& policy)
    {
        projectRoot = SanitizePath(std::move(projectRoot));
        if (projectRoot.empty())
        {
            return false;
        }
        profile.recentProjects.erase(std::remove(profile.recentProjects.begin(), profile.recentProjects.end(), projectRoot), profile.recentProjects.end());
        profile.recentProjects.insert(profile.recentProjects.begin(), projectRoot);
        if (profile.recentProjects.size() > policy.maxRecentProjects)
        {
            profile.recentProjects.resize(policy.maxRecentProjects);
        }
        ++profile.revision;
        return true;
    }

    EditorTheme BuildEditorThemeFromPreferences(const EditorPreferencesProfile& profile)
    {
        EditorTheme theme{};
        switch (profile.ui.themeProfile)
        {
            case EditorThemeProfile::HighContrastDark:
                theme = BuildHighContrastEditorTheme();
                break;
            case EditorThemeProfile::LightPreview:
                theme = BuildLightPreviewEditorTheme();
                break;
            case EditorThemeProfile::UnityLikeDark:
            default:
                theme = BuildDefaultEditorTheme();
                break;
        }
        return ScaleEditorThemeForDpi(theme, profile.ui.dpiScale);
    }

    EditorStartupPlan BuildEditorStartupPlan(const EditorPreferencesProfile& profile, const EditorShortcutProfile& shortcuts)
    {
        EditorStartupPlan plan{};
        plan.theme = BuildEditorThemeFromPreferences(profile);
        plan.shortcutProfile = shortcuts;
        plan.layoutPath = profile.layoutPath;
        plan.sessionPath = profile.sessionPath;
        plan.projectRoot = profile.projectRoot;
        plan.loadLayout = profile.ui.restoreLastLayout;
        plan.loadSession = profile.ui.restoreLastSession;
        plan.saveSessionOnExit = profile.autosave.saveSessionOnExit;
        plan.autosaveEnabled = profile.autosave.enabled;
        plan.blockUnsafeTextShortcuts = profile.safety.blockUnsafeTextShortcuts;
        const EditorThemeDiagnostics themeDiagnostics = ValidateEditorTheme(plan.theme);
        plan.ok = themeDiagnostics.ok && !plan.projectRoot.empty() && !plan.layoutPath.empty() && !plan.sessionPath.empty();
        std::ostringstream out;
        out << "editor-startup project=" << plan.projectRoot
            << " theme=" << plan.theme.id
            << " dpi=" << std::fixed << std::setprecision(2) << profile.ui.dpiScale
            << " layout=" << (plan.loadLayout ? 1 : 0)
            << " session=" << (plan.loadSession ? 1 : 0)
            << " autosave=" << (plan.autosaveEnabled ? 1 : 0)
            << " shortcuts=" << plan.shortcutProfile.bindings.size()
            << " ok=" << (plan.ok ? 1 : 0);
        plan.summary = out.str();
        return plan;
    }

    std::string FormatEditorPreferencesRepairIssue(const EditorPreferencesRepairIssue& issue)
    {
        std::ostringstream out;
        out << ToString(issue.severity) << ':' << ToString(issue.code) << ':' << issue.field << ':' << issue.message;
        return out.str();
    }

    std::string FormatEditorPreferencesRepairReport(const EditorPreferencesRepairReport& report)
    {
        std::ostringstream out;
        out << "editor-preferences-repair issues=" << report.issues.size()
            << " repaired=" << (report.repaired ? 1 : 0)
            << " reset=" << (report.resetToDefault ? 1 : 0)
            << " ok=" << (report.ok ? 1 : 0);
        return out.str();
    }

    std::string FormatEditorPreferencesSummary(const EditorPreferencesProfile& profile)
    {
        std::ostringstream out;
        out << "editor-preferences profile=" << profile.profileName
            << " startup=" << ToString(profile.startupMode)
            << " project=" << profile.projectRoot
            << " theme=" << ToString(profile.ui.themeProfile)
            << " dpi=" << std::fixed << std::setprecision(2) << profile.ui.dpiScale
            << " recent=" << profile.recentProjects.size()
            << " autosave=" << (profile.autosave.enabled ? 1 : 0)
            << " interval=" << profile.autosave.intervalSeconds
            << " backups=" << profile.autosave.maxBackupCount;
        return out.str();
    }

    EditorPreferencesDiagnostics RunEditorPreferencesDiagnostics()
    {
        EditorPreferencesDiagnostics diagnostics{};
        EditorPreferencesProfile profile = BuildDefaultEditorPreferencesProfile();
        AddRecentEditorProject(profile, "projects/Sandbox");
        AddRecentEditorProject(profile, "D:/AKENGINE/projects/Sandbox");
        const std::string text = SerializeEditorPreferencesProfile(profile);
        const EditorPreferencesLoadResult load = LoadEditorPreferencesProfileWithRepair(text);
        const EditorStartupPlan plan = BuildEditorStartupPlan(load.profile);
        const EditorThemeDiagnostics themeDiagnostics = ValidateEditorTheme(plan.theme);
        const EditorShortcutDiagnostics shortcutDiagnostics = RunEditorShortcutDiagnostics();
        diagnostics.recentProjectCount = load.profile.recentProjects.size();
        diagnostics.repairIssueCount = load.repair.issues.size();
        diagnostics.serialized = !text.empty();
        diagnostics.loaded = load.ok;
        diagnostics.repaired = load.repair.repaired;
        diagnostics.startupPlanOk = plan.ok;
        diagnostics.themeOk = themeDiagnostics.ok;
        diagnostics.shortcutsOk = shortcutDiagnostics.ok;
        diagnostics.ok = diagnostics.serialized && diagnostics.loaded && diagnostics.startupPlanOk && diagnostics.themeOk && diagnostics.shortcutsOk;
        diagnostics.summary = FormatEditorPreferencesDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorPreferencesDiagnostics(const EditorPreferencesDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-preferences diagnostics recent=" << diagnostics.recentProjectCount
            << " issues=" << diagnostics.repairIssueCount
            << " serialized=" << (diagnostics.serialized ? 1 : 0)
            << " loaded=" << (diagnostics.loaded ? 1 : 0)
            << " startup=" << (diagnostics.startupPlanOk ? 1 : 0)
            << " theme=" << (diagnostics.themeOk ? 1 : 0)
            << " shortcuts=" << (diagnostics.shortcutsOk ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        return out.str();
    }
}
