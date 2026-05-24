#include <AK/EditorUI/EditorCommandPalette.hpp>

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace AK
{
    namespace
    {
        i32 ClampI32(i32 value, i32 minValue, i32 maxValue)
        {
            return std::max<i32>(minValue, std::min<i32>(value, maxValue));
        }

        i32 SaturatingI32(long long value)
        {
            const long long minValue = static_cast<long long>(std::numeric_limits<i32>::min());
            const long long maxValue = static_cast<long long>(std::numeric_limits<i32>::max());
            return static_cast<i32>(std::clamp(value, minValue, maxValue));
        }

        i32 SaturatingPixelProduct(usize count, i32 pixels)
        {
            const long long itemPixels = static_cast<long long>(std::max<i32>(1, pixels));
            const usize clampedCount = std::min<usize>(count, static_cast<usize>(std::numeric_limits<i32>::max()));
            const long long product = static_cast<long long>(clampedCount) * itemPixels;
            return product > static_cast<long long>(std::numeric_limits<i32>::max()) ? std::numeric_limits<i32>::max() : static_cast<i32>(product);
        }

        i32 ClampScroll(i32 value, i32 contentHeight, i32 viewportHeight)
        {
            if (contentHeight <= viewportHeight)
            {
                return 0;
            }
            return ClampI32(value, 0, contentHeight - viewportHeight);
        }

        void Bump(EditorCommandPaletteState& state)
        {
            ++state.revision;
        }

        EditorIconKind IconForResult(EditorSearchResultKind kind)
        {
            switch (kind)
            {
                case EditorSearchResultKind::Command: return EditorIconKind::Search;
                case EditorSearchResultKind::Panel: return EditorIconKind::Scene;
                case EditorSearchResultKind::Asset: return EditorIconKind::Asset;
                case EditorSearchResultKind::Entity: return EditorIconKind::Entity;
                case EditorSearchResultKind::Setting: return EditorIconKind::Diagnostics;
                case EditorSearchResultKind::Overlay: return EditorIconKind::Diagnostics;
                default: return EditorIconKind::Search;
            }
        }

        std::string ShortcutTextForCommand(const EditorShortcutProfile& shortcuts, CommandId command)
        {
            if (command == CommandId::Count)
            {
                return {};
            }

            for (const EditorShortcutBindingDesc& binding : shortcuts.bindings)
            {
                if (binding.enabled && binding.command == command && binding.context != EditorShortcutContext::TextEntry)
                {
                    return FormatEditorShortcutChord(binding.chord);
                }
            }
            return {};
        }

        EditorCommandPaletteRow RowFromSearchResult(const EditorSearchResult& result,
                                                    const EditorCommandStateCache& commandState,
                                                    const EditorShortcutProfile& shortcuts)
        {
            EditorCommandPaletteRow row{};
            row.stableId = result.stableId;
            row.title = result.title;
            row.subtitle = result.subtitle;
            row.resultKind = result.kind;
            row.icon = IconForResult(result.kind);
            row.command = result.command;
            row.panel = result.panel;
            row.entity = result.entity;
            row.score = result.score;
            row.enabled = true;

            if (result.kind == EditorSearchResultKind::Command)
            {
                if (const EditorCommandStateEntry* state = FindEditorCommandState(commandState, result.command))
                {
                    row.enabled = state->enabled && state->visible;
                    row.destructive = state->destructive;
                    row.disabledReasonText = state->disabledReasonText;
                    row.rightText = ShortcutTextForCommand(shortcuts, result.command);
                }
                else
                {
                    row.enabled = false;
                    row.disabledReasonText = "Command is not registered in state cache";
                }
            }
            else
            {
                row.rightText = ToString(result.kind);
            }
            return row;
        }

        std::vector<EditorCommandPaletteRow> BuildRows(const EditorCommandPaletteState& state,
                                                       const EditorSearchIndex& searchIndex,
                                                       const EditorCommandStateCache& commandState,
                                                       const EditorShortcutProfile& shortcuts)
        {
            EditorSearchQuery query{};
            query.text = state.query;
            query.maxResults = std::max<usize>(1, state.maxResults);
            query.includeHidden = state.query.empty();

            std::vector<EditorSearchResult> results = SearchEditorIndex(searchIndex, query);
            std::vector<EditorCommandPaletteRow> rows;
            rows.reserve(results.size());
            for (const EditorSearchResult& result : results)
            {
                rows.push_back(RowFromSearchResult(result, commandState, shortcuts));
            }
            return rows;
        }

        void ClampSelection(EditorCommandPaletteState& state, usize rowCount)
        {
            if (rowCount == 0)
            {
                state.selectedIndex = 0;
                return;
            }
            if (state.selectedIndex >= rowCount)
            {
                state.selectedIndex = rowCount - 1;
            }
        }

        void EnsureSelectedVisible(EditorCommandPaletteState& state, usize rowCount, i32 rowHeight, i32 viewportHeight)
        {
            if (rowCount == 0 || rowHeight <= 0 || viewportHeight <= 0)
            {
                state.scrollOffsetPixels = 0;
                return;
            }

            const i32 selectedTop = SaturatingPixelProduct(state.selectedIndex, rowHeight);
            const i32 selectedBottom = SaturatingI32(static_cast<long long>(selectedTop) + static_cast<long long>(std::max<i32>(1, rowHeight)));
            if (selectedTop < state.scrollOffsetPixels)
            {
                state.scrollOffsetPixels = selectedTop;
            }
            else if (selectedBottom > SaturatingI32(static_cast<long long>(state.scrollOffsetPixels) + static_cast<long long>(viewportHeight)))
            {
                state.scrollOffsetPixels = selectedBottom - viewportHeight;
            }

            state.scrollOffsetPixels = ClampScroll(state.scrollOffsetPixels, SaturatingPixelProduct(rowCount, rowHeight), viewportHeight);
        }

        void SetResultStatus(EditorCommandPaletteResult& result, const EditorCommandPaletteState& state)
        {
            result.statusText = FormatEditorCommandPaletteState(state);
        }
    }

    const char* ToString(EditorCommandPaletteAction action)
    {
        switch (action)
        {
            case EditorCommandPaletteAction::None: return "none";
            case EditorCommandPaletteAction::Open: return "open";
            case EditorCommandPaletteAction::Close: return "close";
            case EditorCommandPaletteAction::Toggle: return "toggle";
            case EditorCommandPaletteAction::SetQuery: return "set-query";
            case EditorCommandPaletteAction::AppendText: return "append-text";
            case EditorCommandPaletteAction::Backspace: return "backspace";
            case EditorCommandPaletteAction::ClearQuery: return "clear-query";
            case EditorCommandPaletteAction::MoveSelection: return "move-selection";
            case EditorCommandPaletteAction::AcceptSelection: return "accept-selection";
            case EditorCommandPaletteAction::ScrollRows: return "scroll-rows";
            default: return "unknown";
        }
    }

    const char* ToString(EditorCommandPaletteTarget target)
    {
        switch (target)
        {
            case EditorCommandPaletteTarget::None: return "none";
            case EditorCommandPaletteTarget::SearchBox: return "search-box";
            case EditorCommandPaletteTarget::ResultRow: return "result-row";
            case EditorCommandPaletteTarget::Footer: return "footer";
            case EditorCommandPaletteTarget::Surface: return "surface";
            default: return "unknown";
        }
    }

    EditorCommandPaletteState MakeDefaultEditorCommandPaletteState()
    {
        EditorCommandPaletteState state{};
        state.maxResults = 48;
        state.revision = 1;
        return state;
    }

    EditorCommandPaletteInput MakeEditorCommandPaletteOpenInput(std::string query)
    {
        EditorCommandPaletteInput input{};
        input.action = EditorCommandPaletteAction::Open;
        input.target = EditorCommandPaletteTarget::SearchBox;
        input.text = std::move(query);
        return input;
    }

    EditorCommandPaletteInput MakeEditorCommandPaletteQueryInput(std::string query)
    {
        EditorCommandPaletteInput input{};
        input.action = EditorCommandPaletteAction::SetQuery;
        input.target = EditorCommandPaletteTarget::SearchBox;
        input.text = std::move(query);
        return input;
    }

    EditorCommandPaletteInput MakeEditorCommandPaletteMoveInput(i32 delta)
    {
        EditorCommandPaletteInput input{};
        input.action = EditorCommandPaletteAction::MoveSelection;
        input.target = EditorCommandPaletteTarget::ResultRow;
        input.moveDelta = delta;
        return input;
    }

    EditorCommandPaletteInput MakeEditorCommandPaletteScrollInput(i32 wheelDelta, i32 viewportHeight, i32 rowHeightPixels)
    {
        EditorCommandPaletteInput input{};
        input.action = EditorCommandPaletteAction::ScrollRows;
        input.target = EditorCommandPaletteTarget::ResultRow;
        input.wheelDelta = wheelDelta;
        input.viewportHeight = viewportHeight;
        input.rowHeightPixels = rowHeightPixels;
        return input;
    }

    EditorCommandPaletteSurface BuildEditorCommandPaletteSurface(const EditorCommandPaletteState& state,
                                                                 const EditorSearchIndex& searchIndex,
                                                                 const EditorCommandStateCache& commandState,
                                                                 const EditorShortcutProfile& shortcuts,
                                                                 i32 viewportWidth,
                                                                 i32 viewportHeight,
                                                                 i32 surfaceWidth,
                                                                 i32 surfaceMaxHeight,
                                                                 i32 rowHeightPixels)
    {
        EditorCommandPaletteSurface surface{};
        surface.open = state.open;
        surface.query = state.query;
        surface.rowHeightPixels = std::max<i32>(24, rowHeightPixels);
        surface.rows = BuildRows(state, searchIndex, commandState, shortcuts);
        surface.selectedIndex = surface.rows.empty() ? 0 : std::min<usize>(state.selectedIndex, surface.rows.size() - 1);
        surface.revision = state.revision;

        const i32 safeViewportWidth = std::max<i32>(320, viewportWidth);
        const i32 safeViewportHeight = std::max<i32>(240, viewportHeight);
        const i32 width = ClampI32(surfaceWidth, 360, std::max<i32>(360, safeViewportWidth - 64));
        const i32 contentRows = std::min<i32>(static_cast<i32>(surface.rows.size()), 12);
        const i32 desiredHeight = 48 + 16 + std::max<i32>(4, contentRows) * surface.rowHeightPixels + 28;
        const i32 height = ClampI32(desiredHeight, 220, std::min<i32>(surfaceMaxHeight, safeViewportHeight - 64));
        const i32 x = (safeViewportWidth - width) / 2;
        const i32 y = std::max<i32>(40, safeViewportHeight / 8);

        surface.surfaceRect = {x, y, width, height};
        surface.searchBoxRect = {x + 14, y + 12, width - 28, 32};
        surface.footerRect = {x + 14, y + height - 28, width - 28, 20};
        surface.listRect = {x + 8, surface.searchBoxRect.y + surface.searchBoxRect.height + 8, width - 16, surface.footerRect.y - (surface.searchBoxRect.y + surface.searchBoxRect.height + 12)};
        surface.visibleRowCount = std::max<i32>(0, surface.listRect.height / surface.rowHeightPixels);
        surface.totalContentHeightPixels = SaturatingPixelProduct(surface.rows.size(), surface.rowHeightPixels);
        surface.visibleFirstRow = surface.rowHeightPixels > 0 ? std::max<i32>(0, state.scrollOffsetPixels / surface.rowHeightPixels) : 0;

        for (usize index = 0; index < surface.rows.size(); ++index)
        {
            surface.rows[index].selected = (index == surface.selectedIndex);
        }

        std::ostringstream footer;
        footer << surface.rows.size() << " results";
        if (!surface.rows.empty())
        {
            footer << " | Enter accept | Esc close | Up/Down navigate";
        }
        else
        {
            footer << " | No matching commands, panels, assets or entities";
        }
        surface.footerText = footer.str();
        return surface;
    }

    EditorCommandPaletteResult ApplyEditorCommandPaletteInput(EditorCommandPaletteState& state,
                                                              const EditorSearchIndex& searchIndex,
                                                              const EditorCommandStateCache& commandState,
                                                              const EditorShortcutProfile& shortcuts,
                                                              const EditorCommandPaletteInput& input)
    {
        EditorCommandPaletteResult result{};
        result.command.id = CommandId::Count;
        result.command.source = CommandSource::CommandPalette;

        std::vector<EditorCommandPaletteRow> rows = BuildRows(state, searchIndex, commandState, shortcuts);
        ClampSelection(state, rows.size());

        switch (input.action)
        {
            case EditorCommandPaletteAction::Open:
                state.open = true;
                state.query = input.text;
                state.selectedIndex = 0;
                state.scrollOffsetPixels = 0;
                result.handled = true;
                result.opened = true;
                result.queryChanged = !input.text.empty();
                Bump(state);
                break;
            case EditorCommandPaletteAction::Close:
                if (state.open)
                {
                    state.open = false;
                    result.closed = true;
                    Bump(state);
                }
                result.handled = true;
                break;
            case EditorCommandPaletteAction::Toggle:
                state.open = !state.open;
                result.opened = state.open;
                result.closed = !state.open;
                result.handled = true;
                Bump(state);
                break;
            case EditorCommandPaletteAction::SetQuery:
            {
                const std::string next = input.appendText ? state.query + input.text : input.text;
                if (state.query != next)
                {
                    state.query = next;
                    state.selectedIndex = 0;
                    state.scrollOffsetPixels = 0;
                    result.queryChanged = true;
                    Bump(state);
                }
                state.open = true;
                result.handled = true;
                break;
            }
            case EditorCommandPaletteAction::AppendText:
                if (!input.text.empty())
                {
                    state.query += input.text;
                    state.open = true;
                    state.selectedIndex = 0;
                    state.scrollOffsetPixels = 0;
                    result.queryChanged = true;
                    result.handled = true;
                    Bump(state);
                }
                break;
            case EditorCommandPaletteAction::Backspace:
                if (!state.query.empty())
                {
                    state.query.pop_back();
                    state.selectedIndex = 0;
                    state.scrollOffsetPixels = 0;
                    result.queryChanged = true;
                    Bump(state);
                }
                result.handled = true;
                break;
            case EditorCommandPaletteAction::ClearQuery:
                if (!state.query.empty())
                {
                    state.query.clear();
                    state.selectedIndex = 0;
                    state.scrollOffsetPixels = 0;
                    result.queryChanged = true;
                    Bump(state);
                }
                result.handled = true;
                break;
            case EditorCommandPaletteAction::MoveSelection:
            {
                rows = BuildRows(state, searchIndex, commandState, shortcuts);
                if (!rows.empty())
                {
                    const i32 current = static_cast<i32>(std::min<usize>(state.selectedIndex, rows.size() - 1));
                    const i32 next = ClampI32(SaturatingI32(static_cast<long long>(current) + static_cast<long long>(input.moveDelta)), 0, static_cast<i32>(rows.size()) - 1);
                    if (next != current)
                    {
                        state.selectedIndex = static_cast<usize>(next);
                        EnsureSelectedVisible(state, rows.size(), input.rowHeightPixels, input.viewportHeight);
                        result.selectionChanged = true;
                        Bump(state);
                    }
                }
                result.handled = true;
                break;
            }
            case EditorCommandPaletteAction::AcceptSelection:
            {
                rows = BuildRows(state, searchIndex, commandState, shortcuts);
                ClampSelection(state, rows.size());
                if (!rows.empty())
                {
                    const EditorCommandPaletteRow& row = rows[state.selectedIndex];
                    result.acceptedStableId = row.stableId;
                    result.acceptedKind = row.resultKind;
                    result.panel = row.panel;
                    result.entity = row.entity;
                    result.accepted = row.enabled;
                    if (row.enabled && row.resultKind == EditorSearchResultKind::Command && row.command != CommandId::Count)
                    {
                        result.command.id = row.command;
                        result.command.source = CommandSource::CommandPalette;
                        result.commandQueued = true;
                    }
                    if (row.enabled)
                    {
                        state.open = false;
                    }
                    result.handled = true;
                    Bump(state);
                }
                break;
            }
            case EditorCommandPaletteAction::ScrollRows:
            {
                rows = BuildRows(state, searchIndex, commandState, shortcuts);
                const i32 rowHeight = std::max<i32>(1, input.rowHeightPixels);
                const i32 visibleHeight = std::max<i32>(rowHeight, input.viewportHeight);
                const i32 before = state.scrollOffsetPixels;
                const i32 wheelPixels = SaturatingI32(-static_cast<long long>(input.wheelDelta / 120) * static_cast<long long>(rowHeight) * 3LL);
                state.scrollOffsetPixels = ClampScroll(SaturatingI32(static_cast<long long>(state.scrollOffsetPixels) + static_cast<long long>(wheelPixels)),
                                                       SaturatingPixelProduct(rows.size(), rowHeight),
                                                       visibleHeight);
                result.scrollChanged = before != state.scrollOffsetPixels;
                result.handled = true;
                if (result.scrollChanged)
                {
                    Bump(state);
                }
                break;
            }
            case EditorCommandPaletteAction::None:
            default:
                break;
        }

        SetResultStatus(result, state);
        return result;
    }

    bool ValidateEditorCommandPaletteSurface(const EditorCommandPaletteSurface& surface, i32 viewportWidth, i32 viewportHeight)
    {
        if (!surface.open)
        {
            return true;
        }
        if (surface.surfaceRect.width <= 0 || surface.surfaceRect.height <= 0 || surface.searchBoxRect.width <= 0 || surface.listRect.height < 0)
        {
            return false;
        }
        if (surface.surfaceRect.x < 0 || surface.surfaceRect.y < 0 || surface.surfaceRect.x + surface.surfaceRect.width > viewportWidth || surface.surfaceRect.y + surface.surfaceRect.height > viewportHeight)
        {
            return false;
        }
        if (!surface.rows.empty() && surface.selectedIndex >= surface.rows.size())
        {
            return false;
        }
        std::unordered_set<std::string> ids;
        for (const EditorCommandPaletteRow& row : surface.rows)
        {
            if (row.stableId.empty() || !ids.insert(row.stableId).second)
            {
                return false;
            }
        }
        return true;
    }

    std::string FormatEditorCommandPaletteState(const EditorCommandPaletteState& state)
    {
        std::ostringstream out;
        out << "editor-command-palette open=" << (state.open ? 1 : 0)
            << " query='" << state.query << "' selected=" << state.selectedIndex
            << " scroll=" << state.scrollOffsetPixels
            << " max=" << state.maxResults
            << " revision=" << state.revision;
        return out.str();
    }

    std::string FormatEditorCommandPaletteRow(const EditorCommandPaletteRow& row)
    {
        std::ostringstream out;
        out << ToString(row.resultKind) << ':' << row.title
            << " id=" << row.stableId
            << " score=" << row.score
            << " enabled=" << (row.enabled ? 1 : 0)
            << " selected=" << (row.selected ? 1 : 0)
            << " shortcut='" << row.rightText << "'";
        if (!row.disabledReasonText.empty())
        {
            out << " disabled='" << row.disabledReasonText << "'";
        }
        return out.str();
    }

    std::string FormatEditorCommandPaletteSurface(const EditorCommandPaletteSurface& surface)
    {
        std::ostringstream out;
        out << "editor-command-palette-surface open=" << (surface.open ? 1 : 0)
            << " rows=" << surface.rows.size()
            << " selected=" << surface.selectedIndex
            << " first=" << surface.visibleFirstRow
            << " visible=" << surface.visibleRowCount
            << " query='" << surface.query << "' footer='" << surface.footerText << "'";
        return out.str();
    }

    std::string FormatEditorCommandPaletteResult(const EditorCommandPaletteResult& result)
    {
        std::ostringstream out;
        out << "editor-command-palette-result handled=" << (result.handled ? 1 : 0)
            << " opened=" << (result.opened ? 1 : 0)
            << " closed=" << (result.closed ? 1 : 0)
            << " queryChanged=" << (result.queryChanged ? 1 : 0)
            << " selectionChanged=" << (result.selectionChanged ? 1 : 0)
            << " accepted=" << (result.accepted ? 1 : 0)
            << " commandQueued=" << (result.commandQueued ? 1 : 0)
            << " target='" << result.acceptedStableId << "'";
        return out.str();
    }
}
