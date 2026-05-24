#include <AK/EditorUI/EditorCommandPaletteRuntime.hpp>

#include <AK/EditorUI/EditorFocus.hpp>
#include <AK/EditorUI/EditorRuntimeBridge.hpp>
#include <AK/Input/InputActions.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool Contains(const EditorRect& rect, i32 x, i32 y)
        {
            return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
        }

        EditorCommandPaletteRuntimeInput MakeRuntimeInput(const EditorSearchIndex& searchIndex,
                                                          const EditorCommandStateCache& commandState,
                                                          const EditorShortcutProfile& shortcuts,
                                                          i32 viewportWidth,
                                                          i32 viewportHeight)
        {
            EditorCommandPaletteRuntimeInput input{};
            input.searchIndex = &searchIndex;
            input.commandState = &commandState;
            input.shortcuts = &shortcuts;
            input.viewportWidth = viewportWidth;
            input.viewportHeight = viewportHeight;
            return input;
        }

        EditorCommandPaletteRuntimeResult ConvertResult(const EditorCommandPaletteResult& source)
        {
            EditorCommandPaletteRuntimeResult result{};
            result.handled = source.handled;
            result.opened = source.opened;
            result.closed = source.closed;
            result.accepted = source.accepted;
            result.commandQueued = source.commandQueued;
            result.command = source.command;
            result.acceptedKind = source.acceptedKind;
            result.acceptedEntity = source.entity;
            result.acceptedPanel = source.panel;
            result.acceptedStableId = source.acceptedStableId;
            result.statusText = source.statusText;
            result.surfaceChanged = source.opened || source.closed || source.queryChanged || source.selectionChanged || source.scrollChanged || source.accepted;
            return result;
        }

        void ApplyPaletteAction(EditorCommandPaletteRuntimeState& runtime,
                                EditorCommandPaletteRuntimeResult& runtimeResult,
                                const EditorCommandPaletteInput& paletteInput,
                                const EditorCommandPaletteRuntimeInput& input)
        {
            if (!input.searchIndex || !input.commandState || !input.shortcuts)
            {
                return;
            }

            const EditorCommandPaletteResult result = ApplyEditorCommandPaletteInput(runtime.palette,
                                                                                     *input.searchIndex,
                                                                                     *input.commandState,
                                                                                     *input.shortcuts,
                                                                                     paletteInput);
            runtime.lastResult = result;
            EditorCommandPaletteRuntimeResult converted = ConvertResult(result);
            runtimeResult.handled = runtimeResult.handled || converted.handled;
            runtimeResult.opened = runtimeResult.opened || converted.opened;
            runtimeResult.closed = runtimeResult.closed || converted.closed;
            runtimeResult.surfaceChanged = runtimeResult.surfaceChanged || converted.surfaceChanged;
            runtimeResult.accepted = runtimeResult.accepted || converted.accepted;
            runtimeResult.commandQueued = runtimeResult.commandQueued || converted.commandQueued;
            if (converted.commandQueued)
            {
                runtimeResult.command = converted.command;
            }
            if (converted.accepted)
            {
                runtimeResult.acceptedKind = converted.acceptedKind;
                runtimeResult.acceptedEntity = converted.acceptedEntity;
                runtimeResult.acceptedPanel = converted.acceptedPanel;
                runtimeResult.acceptedStableId = converted.acceptedStableId;
            }
            if (!converted.statusText.empty())
            {
                runtimeResult.statusText = converted.statusText;
            }
            ++runtime.revision;
        }
    }

    const char* ToString(EditorCommandPaletteHitKind kind)
    {
        switch (kind)
        {
            case EditorCommandPaletteHitKind::None: return "none";
            case EditorCommandPaletteHitKind::Outside: return "outside";
            case EditorCommandPaletteHitKind::Surface: return "surface";
            case EditorCommandPaletteHitKind::SearchBox: return "search-box";
            case EditorCommandPaletteHitKind::ResultRow: return "result-row";
            case EditorCommandPaletteHitKind::Footer: return "footer";
            default: return "unknown";
        }
    }

    EditorCommandPaletteRuntimeState MakeDefaultEditorCommandPaletteRuntimeState()
    {
        EditorCommandPaletteRuntimeState runtime{};
        runtime.palette = MakeDefaultEditorCommandPaletteState();
        runtime.lastResult.command.id = CommandId::Count;
        runtime.lastResult.command.source = CommandSource::CommandPalette;
        runtime.revision = 1;
        return runtime;
    }

    EditorCommandPaletteHitTestResult HitTestEditorCommandPaletteSurface(const EditorCommandPaletteSurface& surface, i32 x, i32 y)
    {
        EditorCommandPaletteHitTestResult hit{};
        if (!surface.open)
        {
            hit.kind = EditorCommandPaletteHitKind::None;
            return hit;
        }
        if (!Contains(surface.surfaceRect, x, y))
        {
            hit.kind = EditorCommandPaletteHitKind::Outside;
            return hit;
        }
        if (Contains(surface.searchBoxRect, x, y))
        {
            hit.kind = EditorCommandPaletteHitKind::SearchBox;
            hit.actionable = true;
            hit.enabled = true;
            hit.label = "Search Box";
            return hit;
        }
        if (Contains(surface.footerRect, x, y))
        {
            hit.kind = EditorCommandPaletteHitKind::Footer;
            hit.label = "Footer";
            return hit;
        }
        if (Contains(surface.listRect, x, y) && surface.rowHeightPixels > 0)
        {
            const i32 localY = y - surface.listRect.y;
            const i32 visibleRow = std::max<i32>(0, localY / surface.rowHeightPixels);
            const usize rowIndex = static_cast<usize>(surface.visibleFirstRow + visibleRow);
            if (rowIndex < surface.rows.size())
            {
                const EditorCommandPaletteRow& row = surface.rows[rowIndex];
                hit.kind = EditorCommandPaletteHitKind::ResultRow;
                hit.rowIndex = rowIndex;
                hit.actionable = true;
                hit.enabled = row.enabled;
                hit.stableId = row.stableId;
                hit.label = row.title;
                return hit;
            }
        }
        hit.kind = EditorCommandPaletteHitKind::Surface;
        hit.label = "Command Palette";
        return hit;
    }

    void RefreshEditorCommandPaletteRuntimeSurface(EditorCommandPaletteRuntimeState& runtime,
                                                   const EditorSearchIndex& searchIndex,
                                                   const EditorCommandStateCache& commandState,
                                                   const EditorShortcutProfile& shortcuts,
                                                   i32 viewportWidth,
                                                   i32 viewportHeight)
    {
        runtime.surface = BuildEditorCommandPaletteSurface(runtime.palette, searchIndex, commandState, shortcuts, viewportWidth, viewportHeight);
        runtime.keyboardCapture = runtime.palette.open;
        runtime.mouseCapture = runtime.palette.open;
    }

    EditorCommandPaletteRuntimeResult ApplyEditorCommandPaletteRuntimeInput(EditorCommandPaletteRuntimeState& runtime,
                                                                            const EditorCommandPaletteRuntimeInput& input)
    {
        EditorCommandPaletteRuntimeResult result{};
        result.command.id = CommandId::Count;
        result.command.source = CommandSource::CommandPalette;
        if (!input.searchIndex || !input.commandState || !input.shortcuts)
        {
            result.statusText = "command palette runtime missing search/command/shortcut inputs";
            return result;
        }

        if (input.openShortcutPressed)
        {
            ApplyPaletteAction(runtime, result, MakeEditorCommandPaletteOpenInput(runtime.palette.query), input);
        }

        if (runtime.palette.open)
        {
            if (input.keyEscapePressed)
            {
                EditorCommandPaletteInput close{};
                close.action = EditorCommandPaletteAction::Close;
                close.target = EditorCommandPaletteTarget::Surface;
                ApplyPaletteAction(runtime, result, close, input);
            }
            else
            {
                if (!input.textInput.empty())
                {
                    EditorCommandPaletteInput append{};
                    append.action = EditorCommandPaletteAction::AppendText;
                    append.target = EditorCommandPaletteTarget::SearchBox;
                    append.text = input.textInput;
                    ApplyPaletteAction(runtime, result, append, input);
                }
                if (input.keyBackspacePressed)
                {
                    EditorCommandPaletteInput backspace{};
                    backspace.action = EditorCommandPaletteAction::Backspace;
                    backspace.target = EditorCommandPaletteTarget::SearchBox;
                    ApplyPaletteAction(runtime, result, backspace, input);
                }
                if (input.keyUpPressed)
                {
                    ApplyPaletteAction(runtime, result, MakeEditorCommandPaletteMoveInput(-1), input);
                }
                if (input.keyDownPressed)
                {
                    ApplyPaletteAction(runtime, result, MakeEditorCommandPaletteMoveInput(1), input);
                }
                if (input.mouseWheelDelta != 0)
                {
                    ApplyPaletteAction(runtime, result, MakeEditorCommandPaletteScrollInput(input.mouseWheelDelta, runtime.surface.listRect.height), input);
                }
                if (input.mouseLeftPressed)
                {
                    RefreshEditorCommandPaletteRuntimeSurface(runtime, *input.searchIndex, *input.commandState, *input.shortcuts, input.viewportWidth, input.viewportHeight);
                    const EditorCommandPaletteHitTestResult hit = HitTestEditorCommandPaletteSurface(runtime.surface, input.mouseX, input.mouseY);
                    runtime.hover = hit;
                    result.hoverChanged = true;
                    result.handled = true;
                    if (hit.kind == EditorCommandPaletteHitKind::Outside)
                    {
                        EditorCommandPaletteInput close{};
                        close.action = EditorCommandPaletteAction::Close;
                        close.target = EditorCommandPaletteTarget::Surface;
                        ApplyPaletteAction(runtime, result, close, input);
                    }
                    else if (hit.kind == EditorCommandPaletteHitKind::ResultRow && hit.rowIndex < runtime.surface.rows.size())
                    {
                        runtime.palette.selectedIndex = hit.rowIndex;
                        ++runtime.palette.revision;
                        EditorCommandPaletteInput accept{};
                        accept.action = EditorCommandPaletteAction::AcceptSelection;
                        accept.target = EditorCommandPaletteTarget::ResultRow;
                        ApplyPaletteAction(runtime, result, accept, input);
                    }
                }
                if (input.keyEnterPressed)
                {
                    EditorCommandPaletteInput accept{};
                    accept.action = EditorCommandPaletteAction::AcceptSelection;
                    accept.target = EditorCommandPaletteTarget::ResultRow;
                    ApplyPaletteAction(runtime, result, accept, input);
                }
            }
        }

        RefreshEditorCommandPaletteRuntimeSurface(runtime, *input.searchIndex, *input.commandState, *input.shortcuts, input.viewportWidth, input.viewportHeight);
        result.surfaceChanged = result.surfaceChanged || result.opened || result.closed;
        if (result.statusText.empty())
        {
            result.statusText = FormatEditorCommandPaletteRuntimeState(runtime);
        }
        return result;
    }

    bool ValidateEditorCommandPaletteRuntimeState(const EditorCommandPaletteRuntimeState& runtime, i32 viewportWidth, i32 viewportHeight)
    {
        if (runtime.keyboardCapture != runtime.palette.open || runtime.mouseCapture != runtime.palette.open)
        {
            return false;
        }
        if (!ValidateEditorCommandPaletteSurface(runtime.surface, viewportWidth, viewportHeight))
        {
            return false;
        }
        if (!runtime.surface.rows.empty() && runtime.surface.selectedIndex >= runtime.surface.rows.size())
        {
            return false;
        }
        return true;
    }

    std::string FormatEditorCommandPaletteHitTestResult(const EditorCommandPaletteHitTestResult& hit)
    {
        std::ostringstream out;
        out << "command-palette-hit kind=" << ToString(hit.kind)
            << " row=" << hit.rowIndex
            << " actionable=" << (hit.actionable ? 1 : 0)
            << " enabled=" << (hit.enabled ? 1 : 0)
            << " id='" << hit.stableId << "' label='" << hit.label << "'";
        return out.str();
    }

    std::string FormatEditorCommandPaletteRuntimeState(const EditorCommandPaletteRuntimeState& runtime)
    {
        std::ostringstream out;
        out << "editor-command-palette-runtime open=" << (runtime.palette.open ? 1 : 0)
            << " capture=" << (runtime.keyboardCapture ? 1 : 0)
            << " rows=" << runtime.surface.rows.size()
            << " selected=" << runtime.surface.selectedIndex
            << " query='" << runtime.palette.query << "' revision=" << runtime.revision;
        return out.str();
    }

    std::string FormatEditorCommandPaletteRuntimeResult(const EditorCommandPaletteRuntimeResult& result)
    {
        std::ostringstream out;
        out << "editor-command-palette-runtime-result handled=" << (result.handled ? 1 : 0)
            << " opened=" << (result.opened ? 1 : 0)
            << " closed=" << (result.closed ? 1 : 0)
            << " accepted=" << (result.accepted ? 1 : 0)
            << " command=" << ToString(result.command.id)
            << " target='" << result.acceptedStableId << "'";
        return out.str();
    }

    EditorCommandPaletteRuntimeDiagnostics RunEditorCommandPaletteRuntimeDiagnostics()
    {
        EditorCommandPaletteRuntimeDiagnostics diagnostics{};
        CommandRegistry commands = BuildDefaultEditorCommandRegistry();
        EditorRuntimeBridge bridge = BuildDefaultEditorRuntimeBridge(1366, 768);
        std::vector<EditorSelectionItem> entities;
        EditorSelectionItem cube{};
        cube.domain = EditorSelectionDomain::SceneEntity;
        cube.entity = {1, 1};
        cube.stableId = "entity:1:1";
        cube.displayName = "SampleCube";
        entities.push_back(cube);
        std::vector<std::string> assets{"Assets/Materials/Brick.akmat", "Assets/Meshes/Cube.glb"};
        EditorSearchIndex search = BuildDefaultEditorSearchIndex(commands, bridge.panels, entities, assets);
        EditorFocusState focus = MakeDefaultEditorFocusState();
        EditorCommandContext context = BuildEditorCommandContext(focus, true, true, false, true, false, true);
        EditorCommandStateCache commandState = BuildEditorCommandStateCache(commands, context);
        EditorShortcutProfile shortcuts = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());

        diagnostics.runtime = MakeDefaultEditorCommandPaletteRuntimeState();
        EditorCommandPaletteRuntimeInput input = MakeRuntimeInput(search, commandState, shortcuts, 1366, 768);
        input.openShortcutPressed = true;
        diagnostics.openResult = ApplyEditorCommandPaletteRuntimeInput(diagnostics.runtime, input);
        diagnostics.openOk = diagnostics.runtime.palette.open && diagnostics.openResult.opened;

        input = MakeRuntimeInput(search, commandState, shortcuts, 1366, 768);
        input.textInput = "save";
        diagnostics.textResult = ApplyEditorCommandPaletteRuntimeInput(diagnostics.runtime, input);
        diagnostics.textOk = diagnostics.runtime.palette.query == "save" && !diagnostics.runtime.surface.rows.empty();

        input = MakeRuntimeInput(search, commandState, shortcuts, 1366, 768);
        input.keyDownPressed = true;
        diagnostics.moveResult = ApplyEditorCommandPaletteRuntimeInput(diagnostics.runtime, input);

        diagnostics.rowHit = HitTestEditorCommandPaletteSurface(diagnostics.runtime.surface,
                                                                diagnostics.runtime.surface.listRect.x + 12,
                                                                diagnostics.runtime.surface.listRect.y + 8);
        diagnostics.hitOk = diagnostics.rowHit.kind == EditorCommandPaletteHitKind::ResultRow;

        input = MakeRuntimeInput(search, commandState, shortcuts, 1366, 768);
        input.keyEnterPressed = true;
        diagnostics.acceptResult = ApplyEditorCommandPaletteRuntimeInput(diagnostics.runtime, input);
        diagnostics.acceptOk = diagnostics.acceptResult.accepted && !diagnostics.runtime.palette.open;
        diagnostics.captureOk = !diagnostics.runtime.keyboardCapture && !diagnostics.runtime.mouseCapture;
        diagnostics.validSurface = ValidateEditorCommandPaletteRuntimeState(diagnostics.runtime, 1366, 768);
        diagnostics.rowCount = diagnostics.runtime.surface.rows.size();
        diagnostics.ok = diagnostics.openOk && diagnostics.textOk && diagnostics.hitOk && diagnostics.acceptOk && diagnostics.captureOk && diagnostics.validSurface;

        std::ostringstream out;
        out << "editor-command-palette-runtime rows=" << diagnostics.rowCount
            << " open=" << (diagnostics.openOk ? 1 : 0)
            << " text=" << (diagnostics.textOk ? 1 : 0)
            << " hit=" << (diagnostics.hitOk ? 1 : 0)
            << " accept=" << (diagnostics.acceptOk ? 1 : 0)
            << " capture=" << (diagnostics.captureOk ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string BuildEditorCommandPaletteRuntimeProbeSummary()
    {
        return RunEditorCommandPaletteRuntimeDiagnostics().summary;
    }
}
