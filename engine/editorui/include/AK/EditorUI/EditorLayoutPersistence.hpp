#pragma once

#include <AK/EditorUI/EditorDock.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorLayoutRepairCode
    {
        None,
        UnsupportedVersion,
        ParseFailed,
        MissingRoot,
        InvalidSplitReference,
        InvalidSplitWeight,
        EmptyStack,
        UnknownPanel,
        DuplicatePanel,
        ActiveTabOutOfRange,
        InvalidFrameSize,
        InvalidDockSpace,
        OffscreenWindow,
        ClampedWindow,
        ResetToDefault
    };

    enum class EditorLayoutRepairSeverity
    {
        Info,
        Warning,
        Error
    };

    struct EditorMonitorBounds
    {
        EditorRect workArea{0, 0, 1920, 1080};
        float dpiScale = 1.0f;
        bool primary = true;
        std::string name = "Primary";
    };

    struct EditorWindowPlacement
    {
        EditorRect rect{80, 80, 1600, 900};
        i32 monitorIndex = 0;
        bool maximized = false;
    };

    struct EditorLayoutPersistencePolicy
    {
        i32 minWindowWidth = 900;
        i32 minWindowHeight = 540;
        i32 minVisibleWindowPixels = 80;
        float minSplitWeight = 0.08f;
        float maxSplitWeight = 0.92f;
        bool removeUnknownPanels = true;
        bool removeDuplicatePanels = true;
        bool resetIfCorePanelsMissing = true;
        bool clampWindowToMonitors = true;
    };

    struct EditorLayoutRepairIssue
    {
        EditorLayoutRepairCode code = EditorLayoutRepairCode::None;
        EditorLayoutRepairSeverity severity = EditorLayoutRepairSeverity::Info;
        EditorDockNodeId node{};
        EditorPanelId panel{};
        std::string message;
    };

    struct EditorLayoutRepairReport
    {
        std::vector<EditorLayoutRepairIssue> issues;
        bool repaired = false;
        bool resetToDefault = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorLayoutLoadResult
    {
        EditorFrameLayout frame{};
        EditorWindowPlacement placement{};
        EditorLayoutRepairReport repair{};
        bool loadedFromText = false;
        bool usedLegacyAkLayout = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorLayoutRepairCode code);
    const char* ToString(EditorLayoutRepairSeverity severity);

    EditorLayoutPersistencePolicy MakeDefaultEditorLayoutPersistencePolicy();
    std::vector<EditorMonitorBounds> MakeDefaultEditorMonitorSet(i32 width = 1920, i32 height = 1080);

    EditorWindowPlacement MakeDefaultEditorWindowPlacement(i32 width, i32 height);
    EditorWindowPlacement ClampEditorWindowPlacementToMonitors(EditorWindowPlacement placement, const std::vector<EditorMonitorBounds>& monitors, const EditorLayoutPersistencePolicy& policy, EditorLayoutRepairReport* report = nullptr);

    EditorLayoutRepairReport RepairEditorFrameLayout(EditorFrameLayout& frame, const EditorPanelRegistry& panels, const EditorLayoutPersistencePolicy& policy);
    EditorLayoutLoadResult LoadEditorLayoutWithRepair(std::string_view text, const EditorPanelRegistry& panels, i32 fallbackWidth, i32 fallbackHeight, const std::vector<EditorMonitorBounds>& monitors, const EditorLayoutPersistencePolicy& policy = MakeDefaultEditorLayoutPersistencePolicy());

    std::string SerializeEditorLayoutDocument(const EditorFrameLayout& frame, const EditorWindowPlacement& placement);
    std::string FormatEditorLayoutRepairIssue(const EditorLayoutRepairIssue& issue);
    std::string FormatEditorLayoutRepairReport(const EditorLayoutRepairReport& report);
}
