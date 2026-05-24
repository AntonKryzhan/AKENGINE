#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorCommandState.hpp>
#include <AK/EditorUI/EditorSearch.hpp>
#include <AK/EditorUI/EditorShortcuts.hpp>
#include <AK/EditorUI/EditorWidgets.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorCommandPaletteAction
    {
        None,
        Open,
        Close,
        Toggle,
        SetQuery,
        AppendText,
        Backspace,
        ClearQuery,
        MoveSelection,
        AcceptSelection,
        ScrollRows
    };

    enum class EditorCommandPaletteTarget
    {
        None,
        SearchBox,
        ResultRow,
        Footer,
        Surface
    };

    struct EditorCommandPaletteState
    {
        bool open = false;
        std::string query;
        usize selectedIndex = 0;
        i32 scrollOffsetPixels = 0;
        usize maxResults = 48;
        u64 revision = 1;
    };

    struct EditorCommandPaletteRow
    {
        std::string stableId;
        std::string title;
        std::string subtitle;
        std::string rightText;
        std::string disabledReasonText;
        EditorSearchResultKind resultKind = EditorSearchResultKind::Command;
        EditorIconKind icon = EditorIconKind::Search;
        CommandId command = CommandId::Count;
        EditorPanelId panel{};
        EntityId entity{};
        float score = 0.0f;
        bool selected = false;
        bool enabled = true;
        bool destructive = false;
    };

    struct EditorCommandPaletteSurface
    {
        bool open = false;
        std::string query;
        EditorRect surfaceRect{};
        EditorRect searchBoxRect{};
        EditorRect listRect{};
        EditorRect footerRect{};
        i32 rowHeightPixels = 34;
        i32 visibleFirstRow = 0;
        i32 visibleRowCount = 0;
        i32 totalContentHeightPixels = 0;
        usize selectedIndex = 0;
        std::vector<EditorCommandPaletteRow> rows;
        std::string placeholder = "Search commands, assets, entities, panels...";
        std::string footerText;
        u64 revision = 1;
    };

    struct EditorCommandPaletteInput
    {
        EditorCommandPaletteAction action = EditorCommandPaletteAction::None;
        EditorCommandPaletteTarget target = EditorCommandPaletteTarget::None;
        std::string text;
        i32 moveDelta = 0;
        i32 wheelDelta = 0;
        i32 viewportWidth = 1366;
        i32 viewportHeight = 768;
        i32 surfaceWidth = 720;
        i32 surfaceMaxHeight = 520;
        i32 rowHeightPixels = 34;
        bool appendText = false;
        bool ctrlDown = false;
        bool shiftDown = false;
    };

    struct EditorCommandPaletteResult
    {
        bool handled = false;
        bool opened = false;
        bool closed = false;
        bool queryChanged = false;
        bool selectionChanged = false;
        bool scrollChanged = false;
        bool accepted = false;
        bool commandQueued = false;
        std::string acceptedStableId;
        EditorSearchResultKind acceptedKind = EditorSearchResultKind::Command;
        EditorPanelId panel{};
        EntityId entity{};
        CommandInvocation command{};
        std::string statusText;
    };

    struct EditorCommandPaletteDiagnostics
    {
        EditorCommandPaletteState state{};
        EditorCommandPaletteSurface surface{};
        EditorCommandPaletteResult openResult{};
        EditorCommandPaletteResult queryResult{};
        EditorCommandPaletteResult moveResult{};
        EditorCommandPaletteResult acceptResult{};
        EditorCommandPaletteResult disabledAcceptResult{};
        EditorCommandPaletteResult scrollResult{};
        usize rowCount = 0;
        usize commandRowCount = 0;
        usize disabledRowCount = 0;
        usize shortcutTextCount = 0;
        bool openOk = false;
        bool queryOk = false;
        bool selectionClampOk = false;
        bool acceptCommandOk = false;
        bool disabledRejectOk = false;
        bool scrollOk = false;
        bool surfaceOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorCommandPaletteAction action);
    const char* ToString(EditorCommandPaletteTarget target);

    EditorCommandPaletteState MakeDefaultEditorCommandPaletteState();
    EditorCommandPaletteInput MakeEditorCommandPaletteOpenInput(std::string query = {});
    EditorCommandPaletteInput MakeEditorCommandPaletteQueryInput(std::string query);
    EditorCommandPaletteInput MakeEditorCommandPaletteMoveInput(i32 delta);
    EditorCommandPaletteInput MakeEditorCommandPaletteScrollInput(i32 wheelDelta, i32 viewportHeight, i32 rowHeightPixels = 34);

    EditorCommandPaletteSurface BuildEditorCommandPaletteSurface(const EditorCommandPaletteState& state,
                                                                 const EditorSearchIndex& searchIndex,
                                                                 const EditorCommandStateCache& commandState,
                                                                 const EditorShortcutProfile& shortcuts,
                                                                 i32 viewportWidth,
                                                                 i32 viewportHeight,
                                                                 i32 surfaceWidth = 720,
                                                                 i32 surfaceMaxHeight = 520,
                                                                 i32 rowHeightPixels = 34);

    EditorCommandPaletteResult ApplyEditorCommandPaletteInput(EditorCommandPaletteState& state,
                                                              const EditorSearchIndex& searchIndex,
                                                              const EditorCommandStateCache& commandState,
                                                              const EditorShortcutProfile& shortcuts,
                                                              const EditorCommandPaletteInput& input);

    bool ValidateEditorCommandPaletteSurface(const EditorCommandPaletteSurface& surface, i32 viewportWidth, i32 viewportHeight);
    std::string FormatEditorCommandPaletteState(const EditorCommandPaletteState& state);
    std::string FormatEditorCommandPaletteRow(const EditorCommandPaletteRow& row);
    std::string FormatEditorCommandPaletteSurface(const EditorCommandPaletteSurface& surface);
    std::string FormatEditorCommandPaletteResult(const EditorCommandPaletteResult& result);

    EditorCommandPaletteDiagnostics RunEditorCommandPaletteDiagnostics();
    std::string BuildEditorCommandPaletteProbeSummary();
}
