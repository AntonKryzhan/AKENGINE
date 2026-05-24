#include <AK/EditorUI/EditorWorkspace.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace AK
{
    namespace
    {
        std::string NormalizePathString(std::filesystem::path path)
        {
            return path.lexically_normal().generic_string();
        }

        std::filesystem::path JoinWorkspacePath(const std::string& root, const std::string& path)
        {
            std::filesystem::path p(path);
            if (p.is_absolute())
            {
                return p.lexically_normal();
            }
            return (std::filesystem::path(root) / p).lexically_normal();
        }

        bool ExistsRegularFile(const std::string& path)
        {
            std::error_code ec;
            return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
        }

        void AddWorkspaceIssue(EditorWorkspaceReport& report,
                               EditorWorkspaceIssueCode code,
                               EditorWorkspaceIssueSeverity severity,
                               EditorWorkspaceFileKind kind,
                               std::string path,
                               std::string message)
        {
            EditorWorkspaceIssue issue{};
            issue.code = code;
            issue.severity = severity;
            issue.fileKind = kind;
            issue.path = std::move(path);
            issue.message = std::move(message);
            report.issues.push_back(std::move(issue));

            if (code == EditorWorkspaceIssueCode::DefaultsUsed)
            {
                report.usedDefaults = true;
            }
            if (code == EditorWorkspaceIssueCode::PreferencesRepaired
                || code == EditorWorkspaceIssueCode::LayoutRepaired
                || code == EditorWorkspaceIssueCode::SessionRepaired
                || code == EditorWorkspaceIssueCode::ShortcutsRepaired)
            {
                report.repaired = true;
            }
            if (severity == EditorWorkspaceIssueSeverity::Error)
            {
                report.hasErrors = true;
            }
        }

        void MergeWorkspaceReport(EditorWorkspaceReport& dst, const EditorWorkspaceReport& src)
        {
            dst.issues.insert(dst.issues.end(), src.issues.begin(), src.issues.end());
            dst.repaired = dst.repaired || src.repaired;
            dst.usedDefaults = dst.usedDefaults || src.usedDefaults;
            dst.hasErrors = dst.hasErrors || src.hasErrors;
        }
    }

    const char* ToString(EditorWorkspaceFileKind kind)
    {
        switch (kind)
        {
            case EditorWorkspaceFileKind::Preferences: return "preferences";
            case EditorWorkspaceFileKind::Layout: return "layout";
            case EditorWorkspaceFileKind::Session: return "session";
            case EditorWorkspaceFileKind::Shortcuts: return "shortcuts";
            case EditorWorkspaceFileKind::AutosaveDirectory: return "autosave-directory";
            case EditorWorkspaceFileKind::BackupDirectory: return "backup-directory";
            default: return "unknown";
        }
    }

    const char* ToString(EditorWorkspaceIssueCode code)
    {
        switch (code)
        {
            case EditorWorkspaceIssueCode::None: return "None";
            case EditorWorkspaceIssueCode::MissingDirectory: return "MissingDirectory";
            case EditorWorkspaceIssueCode::DirectoryCreateFailed: return "DirectoryCreateFailed";
            case EditorWorkspaceIssueCode::MissingFile: return "MissingFile";
            case EditorWorkspaceIssueCode::ReadFailed: return "ReadFailed";
            case EditorWorkspaceIssueCode::WriteFailed: return "WriteFailed";
            case EditorWorkspaceIssueCode::AtomicTempWriteFailed: return "AtomicTempWriteFailed";
            case EditorWorkspaceIssueCode::AtomicRenameFailed: return "AtomicRenameFailed";
            case EditorWorkspaceIssueCode::PreferencesRepaired: return "PreferencesRepaired";
            case EditorWorkspaceIssueCode::LayoutRepaired: return "LayoutRepaired";
            case EditorWorkspaceIssueCode::SessionRepaired: return "SessionRepaired";
            case EditorWorkspaceIssueCode::ShortcutsRepaired: return "ShortcutsRepaired";
            case EditorWorkspaceIssueCode::DefaultsUsed: return "DefaultsUsed";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorWorkspaceIssueSeverity severity)
    {
        switch (severity)
        {
            case EditorWorkspaceIssueSeverity::Info: return "info";
            case EditorWorkspaceIssueSeverity::Warning: return "warning";
            case EditorWorkspaceIssueSeverity::Error: return "error";
            default: return "unknown";
        }
    }

    EditorWorkspacePaths BuildEditorWorkspacePaths(std::string projectRoot, const EditorPreferencesProfile& preferences)
    {
        if (projectRoot.empty())
        {
            projectRoot = ".";
        }

        EditorWorkspacePaths paths{};
        paths.projectRoot = NormalizePathString(std::filesystem::path(projectRoot));
        paths.cacheDirectory = NormalizePathString(JoinWorkspacePath(paths.projectRoot, ".akcache/editor"));
        paths.preferencesPath = NormalizePathString(JoinWorkspacePath(paths.projectRoot, ".akcache/editor/preferences.akeditorprefs"));
        paths.layoutPath = NormalizePathString(JoinWorkspacePath(paths.projectRoot, preferences.layoutPath.empty() ? ".akcache/editor/layout.akeditorlayout" : preferences.layoutPath));
        paths.sessionPath = NormalizePathString(JoinWorkspacePath(paths.projectRoot, preferences.sessionPath.empty() ? ".akcache/editor/session.akeditorsession" : preferences.sessionPath));
        paths.shortcutPath = NormalizePathString(JoinWorkspacePath(paths.projectRoot, preferences.shortcutPath.empty() ? ".akcache/editor/shortcuts.akeditorshortcuts" : preferences.shortcutPath));
        paths.autosaveDirectory = NormalizePathString(JoinWorkspacePath(paths.projectRoot, ".akcache/editor/autosaves"));
        paths.backupDirectory = NormalizePathString(JoinWorkspacePath(paths.projectRoot, ".akcache/editor/backups"));
        return paths;
    }

    EditorWorkspaceReport EnsureEditorWorkspaceDirectories(const EditorWorkspacePaths& paths)
    {
        EditorWorkspaceReport report{};
        const std::pair<EditorWorkspaceFileKind, std::string> dirs[] = {
            {EditorWorkspaceFileKind::Preferences, paths.cacheDirectory},
            {EditorWorkspaceFileKind::AutosaveDirectory, paths.autosaveDirectory},
            {EditorWorkspaceFileKind::BackupDirectory, paths.backupDirectory}
        };

        for (const auto& [kind, path] : dirs)
        {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
            {
                AddWorkspaceIssue(report, EditorWorkspaceIssueCode::MissingDirectory, EditorWorkspaceIssueSeverity::Info, kind, path, "created missing editor workspace directory");
                std::filesystem::create_directories(path, ec);
            }
            if (ec)
            {
                AddWorkspaceIssue(report, EditorWorkspaceIssueCode::DirectoryCreateFailed, EditorWorkspaceIssueSeverity::Error, kind, path, ec.message());
            }
        }

        report.ok = !report.hasErrors;
        report.summary = FormatEditorWorkspaceReport(report);
        return report;
    }

    bool ReadEditorWorkspaceTextFile(const std::string& path, std::string& outText)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return false;
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        outText = buffer.str();
        return input.good() || input.eof();
    }

    bool AtomicWriteEditorWorkspaceTextFile(const std::string& path, std::string_view text)
    {
        std::filesystem::path target(path);
        std::filesystem::path directory = target.parent_path();
        std::error_code ec;
        if (!directory.empty())
        {
            std::filesystem::create_directories(directory, ec);
            if (ec)
            {
                return false;
            }
        }

        std::filesystem::path temp = target;
        temp += ".tmp";
        {
            std::ofstream output(temp, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                return false;
            }
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            output.flush();
            if (!output)
            {
                return false;
            }
        }

        std::filesystem::rename(temp, target, ec);
        if (ec)
        {
            std::filesystem::remove(target, ec);
            ec.clear();
            std::filesystem::rename(temp, target, ec);
        }
        return !ec;
    }

    EditorWorkspaceLoadResult LoadEditorWorkspace(const EditorWorkspacePaths& seedPaths,
                                                  const EditorPanelRegistry& panels,
                                                  i32 fallbackWidth,
                                                  i32 fallbackHeight,
                                                  const std::vector<EditorMonitorBounds>& monitors,
                                                  const std::vector<std::string>& validStableIds)
    {
        EditorWorkspaceLoadResult result{};
        result.paths = seedPaths;
        MergeWorkspaceReport(result.report, EnsureEditorWorkspaceDirectories(seedPaths));

        std::string text;
        if (ExistsRegularFile(seedPaths.preferencesPath) && ReadEditorWorkspaceTextFile(seedPaths.preferencesPath, text))
        {
            result.preferences = LoadEditorPreferencesProfileWithRepair(text);
            result.loadedPreferences = result.preferences.loadedFromText;
            if (result.preferences.repair.repaired)
            {
                AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::PreferencesRepaired, EditorWorkspaceIssueSeverity::Warning, EditorWorkspaceFileKind::Preferences, seedPaths.preferencesPath, result.preferences.repair.summary);
            }
        }
        else
        {
            result.preferences.profile = BuildDefaultEditorPreferencesProfile();
            result.preferences.ok = true;
            result.preferences.summary = "editor preferences missing; using defaults";
            AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::DefaultsUsed, EditorWorkspaceIssueSeverity::Info, EditorWorkspaceFileKind::Preferences, seedPaths.preferencesPath, "preferences missing; default profile was used");
        }

        result.paths = BuildEditorWorkspacePaths(seedPaths.projectRoot, result.preferences.profile);
        MergeWorkspaceReport(result.report, EnsureEditorWorkspaceDirectories(result.paths));

        text.clear();
        if (ExistsRegularFile(result.paths.shortcutPath) && ReadEditorWorkspaceTextFile(result.paths.shortcutPath, text))
        {
            result.shortcuts = LoadEditorShortcutProfileWithRepair(text);
            result.loadedShortcuts = result.shortcuts.loadedFromText;
            if (result.shortcuts.repair.repaired)
            {
                AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::ShortcutsRepaired, EditorWorkspaceIssueSeverity::Warning, EditorWorkspaceFileKind::Shortcuts, result.paths.shortcutPath, result.shortcuts.repair.summary);
            }
        }
        else
        {
            result.shortcuts.profile = BuildDefaultEditorShortcutProfile();
            result.shortcuts.ok = true;
            result.shortcuts.summary = "editor shortcuts missing; using defaults";
            AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::DefaultsUsed, EditorWorkspaceIssueSeverity::Info, EditorWorkspaceFileKind::Shortcuts, result.paths.shortcutPath, "shortcut profile missing; defaults were used");
        }

        text.clear();
        if (ExistsRegularFile(result.paths.layoutPath) && ReadEditorWorkspaceTextFile(result.paths.layoutPath, text))
        {
            result.layout = LoadEditorLayoutWithRepair(text, panels, fallbackWidth, fallbackHeight, monitors);
            result.loadedLayout = result.layout.loadedFromText;
            if (result.layout.repair.repaired)
            {
                AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::LayoutRepaired, EditorWorkspaceIssueSeverity::Warning, EditorWorkspaceFileKind::Layout, result.paths.layoutPath, result.layout.repair.summary);
            }
        }
        else
        {
            result.layout.frame = BuildDefaultEditorFrameLayout(panels, fallbackWidth, fallbackHeight);
            result.layout.placement = MakeDefaultEditorWindowPlacement(fallbackWidth, fallbackHeight);
            result.layout.ok = true;
            result.layout.summary = "editor layout missing; using default layout";
            AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::DefaultsUsed, EditorWorkspaceIssueSeverity::Info, EditorWorkspaceFileKind::Layout, result.paths.layoutPath, "layout missing; default dock layout was used");
        }

        text.clear();
        if (ExistsRegularFile(result.paths.sessionPath) && ReadEditorWorkspaceTextFile(result.paths.sessionPath, text))
        {
            result.session = LoadEditorSessionWithRepair(text, panels, validStableIds);
            result.loadedSession = result.session.loadedFromText;
            if (result.session.repair.repaired)
            {
                AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::SessionRepaired, EditorWorkspaceIssueSeverity::Warning, EditorWorkspaceFileKind::Session, result.paths.sessionPath, result.session.repair.summary);
            }
        }
        else
        {
            result.session.session = MakeDefaultEditorSessionState(panels);
            result.session.ok = true;
            result.session.summary = "editor session missing; using default session";
            AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::DefaultsUsed, EditorWorkspaceIssueSeverity::Info, EditorWorkspaceFileKind::Session, result.paths.sessionPath, "session missing; default session state was used");
        }

        result.startup = BuildEditorStartupPlan(result.preferences.profile, result.shortcuts.profile);
        result.report.ok = !result.report.hasErrors;
        result.ok = result.report.ok && result.preferences.ok && result.shortcuts.ok && result.layout.ok && result.session.ok && result.startup.ok;
        result.report.summary = FormatEditorWorkspaceReport(result.report);
        std::ostringstream summary;
        summary << "editor-workspace load prefs=" << (result.loadedPreferences ? 1 : 0)
                << " shortcuts=" << (result.loadedShortcuts ? 1 : 0)
                << " layout=" << (result.loadedLayout ? 1 : 0)
                << " session=" << (result.loadedSession ? 1 : 0)
                << " issues=" << result.report.issues.size()
                << " ok=" << (result.ok ? 1 : 0);
        result.summary = summary.str();
        return result;
    }

    EditorWorkspaceSaveResult SaveEditorWorkspace(const EditorWorkspaceSaveInput& input)
    {
        EditorWorkspaceSaveResult result{};
        MergeWorkspaceReport(result.report, EnsureEditorWorkspaceDirectories(input.paths));

        auto writeFile = [&](EditorWorkspaceFileKind kind, const std::string& path, std::string_view text, bool& flag)
        {
            if (AtomicWriteEditorWorkspaceTextFile(path, text))
            {
                flag = true;
                ++result.filesWritten;
            }
            else
            {
                AddWorkspaceIssue(result.report, EditorWorkspaceIssueCode::WriteFailed, EditorWorkspaceIssueSeverity::Error, kind, path, "failed to atomically write editor workspace file");
            }
        };

        if (input.savePreferences && input.preferences)
        {
            const std::string text = SerializeEditorPreferencesProfile(*input.preferences);
            writeFile(EditorWorkspaceFileKind::Preferences, input.paths.preferencesPath, text, result.preferencesSaved);
        }
        if (input.saveShortcuts && input.shortcuts)
        {
            const std::string text = SerializeEditorShortcutProfile(*input.shortcuts);
            writeFile(EditorWorkspaceFileKind::Shortcuts, input.paths.shortcutPath, text, result.shortcutsSaved);
        }
        if (input.saveLayout && input.layout && input.placement)
        {
            const std::string text = SerializeEditorLayoutDocument(*input.layout, *input.placement);
            writeFile(EditorWorkspaceFileKind::Layout, input.paths.layoutPath, text, result.layoutSaved);
        }
        if (input.saveSession && input.session)
        {
            const std::string text = SerializeEditorSessionState(*input.session);
            writeFile(EditorWorkspaceFileKind::Session, input.paths.sessionPath, text, result.sessionSaved);
        }

        result.report.ok = !result.report.hasErrors;
        result.ok = result.report.ok;
        result.report.summary = FormatEditorWorkspaceReport(result.report);
        std::ostringstream summary;
        summary << "editor-workspace save files=" << result.filesWritten
                << " prefs=" << (result.preferencesSaved ? 1 : 0)
                << " shortcuts=" << (result.shortcutsSaved ? 1 : 0)
                << " layout=" << (result.layoutSaved ? 1 : 0)
                << " session=" << (result.sessionSaved ? 1 : 0)
                << " ok=" << (result.ok ? 1 : 0);
        result.summary = summary.str();
        return result;
    }

    std::string FormatEditorWorkspaceIssue(const EditorWorkspaceIssue& issue)
    {
        std::ostringstream out;
        out << ToString(issue.severity) << ':' << ToString(issue.fileKind) << ':' << ToString(issue.code)
            << " path=" << issue.path << " message=" << issue.message;
        return out.str();
    }

    std::string FormatEditorWorkspaceReport(const EditorWorkspaceReport& report)
    {
        std::ostringstream out;
        out << "editor-workspace-report issues=" << report.issues.size()
            << " repaired=" << (report.repaired ? 1 : 0)
            << " defaults=" << (report.usedDefaults ? 1 : 0)
            << " errors=" << (report.hasErrors ? 1 : 0)
            << " ok=" << (!report.hasErrors ? 1 : 0);
        return out.str();
    }

    EditorWorkspaceDiagnostics RunEditorWorkspaceDiagnostics()
    {
        const EditorPanelRegistry panels = BuildDefaultEditorPanelRegistry();
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "ak_editor_workspace_diagnostics";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);

        EditorWorkspaceDiagnostics diagnostics{};
        const EditorPreferencesProfile preferences = BuildDefaultEditorPreferencesProfile();
        const EditorWorkspacePaths paths = BuildEditorWorkspacePaths(root.generic_string(), preferences);
        const EditorWorkspaceReport directoryReport = EnsureEditorWorkspaceDirectories(paths);
        diagnostics.directoriesOk = directoryReport.ok;

        EditorFrameLayout frame = BuildDefaultEditorFrameLayout(panels, 1280, 720);
        EditorWindowPlacement placement = MakeDefaultEditorWindowPlacement(1280, 720);
        EditorSessionState session = MakeDefaultEditorSessionState(panels);
        EditorShortcutProfile shortcuts = BuildDefaultEditorShortcutProfile();

        EditorWorkspaceSaveInput saveInput{};
        saveInput.paths = paths;
        saveInput.preferences = &preferences;
        saveInput.shortcuts = &shortcuts;
        saveInput.layout = &frame;
        saveInput.placement = &placement;
        saveInput.session = &session;
        const EditorWorkspaceSaveResult saved = SaveEditorWorkspace(saveInput);

        const EditorWorkspaceLoadResult loaded = LoadEditorWorkspace(paths, panels, 1280, 720);

        diagnostics.saved = saved.ok;
        diagnostics.loaded = loaded.ok;
        diagnostics.repaired = loaded.report.repaired;
        diagnostics.usedDefaults = loaded.report.usedDefaults;
        diagnostics.filesWritten = saved.filesWritten;
        diagnostics.issueCount = directoryReport.issues.size() + saved.report.issues.size() + loaded.report.issues.size();
        diagnostics.ok = diagnostics.directoriesOk && diagnostics.saved && diagnostics.loaded && diagnostics.filesWritten >= 4;
        diagnostics.summary = FormatEditorWorkspaceDiagnostics(diagnostics);

        std::filesystem::remove_all(root, ec);
        return diagnostics;
    }

    std::string FormatEditorWorkspaceDiagnostics(const EditorWorkspaceDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-workspace diagnostics directories=" << (diagnostics.directoriesOk ? 1 : 0)
            << " saved=" << (diagnostics.saved ? 1 : 0)
            << " loaded=" << (diagnostics.loaded ? 1 : 0)
            << " files=" << diagnostics.filesWritten
            << " issues=" << diagnostics.issueCount
            << " defaults=" << (diagnostics.usedDefaults ? 1 : 0)
            << " repaired=" << (diagnostics.repaired ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        return out.str();
    }
}
