#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>
#include <AK/EditorUI/EditorScrollClip.hpp>
#include <AK/EditorUI/EditorSelection.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorSessionRepairCode
    {
        None,
        UnsupportedVersion,
        ParseFailed,
        UnknownPanel,
        DuplicatePanel,
        InvalidFocusedPanel,
        DuplicateSelection,
        UnknownSelection,
        InvalidScrollOffset,
        InvalidExpansionKey,
        InvalidFoldoutKey,
        TransientStateDropped,
        ResetToDefault
    };

    enum class EditorSessionRepairSeverity
    {
        Info,
        Warning,
        Error
    };

    struct EditorPanelSessionState
    {
        EditorPanelId panel{};
        std::string panelName;
        i32 scrollX = 0;
        i32 scrollY = 0;
        bool activeTab = false;
        bool visible = true;
        u64 contentRevision = 0;
    };

    struct EditorSessionSelectionEntry
    {
        EditorSelectionDomain domain = EditorSelectionDomain::None;
        std::string stableId;
        std::string displayName;
        bool active = false;
    };

    struct EditorSessionExpansionEntry
    {
        std::string scope;
        std::string stableId;
        bool expanded = true;
    };

    struct EditorSessionFoldoutEntry
    {
        std::string key;
        bool expanded = true;
    };

    struct EditorSessionState
    {
        u32 schemaVersion = 1;
        std::string projectName = "Sandbox";
        std::string scenePath = "projects/Sandbox/scene.akscene";
        EditorPanelId focusedPanel{};
        std::string focusedPanelName;
        std::vector<EditorPanelSessionState> panels;
        std::vector<EditorSessionSelectionEntry> selection;
        std::vector<EditorSessionExpansionEntry> expansions;
        std::vector<EditorSessionFoldoutEntry> foldouts;
        std::string hierarchySearch;
        std::string assetSearch;
        std::string projectFolder = "Assets";
        std::string commandPaletteQuery;
        bool commandPaletteOpen = false;
        bool popupOpen = false;
        bool modalOpen = false;
        bool textEditActive = false;
        bool dragActive = false;
        u64 revision = 1;
    };

    struct EditorSessionPersistencePolicy
    {
        i32 maxScrollPixels = 2'000'000;
        usize maxPanelStates = 128;
        usize maxSelectionEntries = 256;
        usize maxExpansionEntries = 4096;
        usize maxFoldoutEntries = 4096;
        bool dropUnknownPanels = true;
        bool dropUnknownSelections = true;
        bool dropDuplicatePanels = true;
        bool dropDuplicateSelections = true;
        bool dropTransientUiState = true;
        bool clampScrollOffsets = true;
        bool resetIfNoPanels = true;
    };

    struct EditorSessionRepairIssue
    {
        EditorSessionRepairCode code = EditorSessionRepairCode::None;
        EditorSessionRepairSeverity severity = EditorSessionRepairSeverity::Info;
        EditorPanelId panel{};
        std::string stableId;
        std::string message;
    };

    struct EditorSessionRepairReport
    {
        std::vector<EditorSessionRepairIssue> issues;
        bool repaired = false;
        bool resetToDefault = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorSessionLoadResult
    {
        EditorSessionState session{};
        EditorSessionRepairReport repair{};
        bool loadedFromText = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorSessionCaptureInput
    {
        const EditorFrameLayout* frame = nullptr;
        const EditorSelectionModel* selection = nullptr;
        const std::vector<EditorScrollState>* scrollStates = nullptr;
        std::vector<EditorSessionExpansionEntry> expansions;
        std::vector<EditorSessionFoldoutEntry> foldouts;
        EditorPanelId focusedPanel{};
        std::string projectName = "Sandbox";
        std::string scenePath = "projects/Sandbox/scene.akscene";
        std::string hierarchySearch;
        std::string assetSearch;
        std::string projectFolder = "Assets";
    };

    struct EditorSessionDiagnostics
    {
        usize panelStateCount = 0;
        usize selectionCount = 0;
        usize expansionCount = 0;
        usize foldoutCount = 0;
        usize issueCount = 0;
        bool serialized = false;
        bool loaded = false;
        bool repaired = false;
        bool transientDropped = false;
        bool activeTabsRestored = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorSessionRepairCode code);
    const char* ToString(EditorSessionRepairSeverity severity);

    EditorSessionPersistencePolicy MakeDefaultEditorSessionPersistencePolicy();
    EditorSessionState MakeDefaultEditorSessionState(const EditorPanelRegistry& panels, std::string projectName = "Sandbox", std::string scenePath = "projects/Sandbox/scene.akscene");
    EditorSessionState CaptureEditorSessionState(const EditorSessionCaptureInput& input, const EditorPanelRegistry& panels);

    std::string SerializeEditorSessionState(const EditorSessionState& session);
    EditorSessionLoadResult LoadEditorSessionWithRepair(std::string_view text, const EditorPanelRegistry& panels, const std::vector<std::string>& validStableIds, const EditorSessionPersistencePolicy& policy = MakeDefaultEditorSessionPersistencePolicy());
    EditorSessionRepairReport RepairEditorSessionState(EditorSessionState& session, const EditorPanelRegistry& panels, const std::vector<std::string>& validStableIds, const EditorSessionPersistencePolicy& policy = MakeDefaultEditorSessionPersistencePolicy());

    bool RestoreEditorSessionActiveTabs(EditorFrameLayout& frame, const EditorSessionState& session, const EditorPanelRegistry& panels);
    std::vector<EditorScrollState> RestoreEditorSessionScrollStates(const EditorSessionState& session);
    EditorSelectionModel RestoreEditorSessionSelection(const EditorSessionState& session);

    std::string FormatEditorSessionRepairIssue(const EditorSessionRepairIssue& issue);
    std::string FormatEditorSessionRepairReport(const EditorSessionRepairReport& report);
    EditorSessionDiagnostics RunEditorSessionDiagnostics();
    std::string FormatEditorSessionDiagnostics(const EditorSessionDiagnostics& diagnostics);
}
