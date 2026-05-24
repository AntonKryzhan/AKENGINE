#include <AK/EditorUI/EditorCommandPaletteProbe.hpp>
#include <AK/EditorUI/EditorPanelModels.hpp>

#include <sstream>

namespace AK
{
    namespace
    {
        EditorSearchIndex BuildProbeSearchIndex(EditorRuntimeBridge& bridge)
        {
            EditorPanelModelFrame frame = BuildDefaultEditorPanelModelFrame(bridge);
            return BuildPanelModelSearchIndex(frame, bridge.searchIndex);
        }
    }

    EditorCommandPaletteDiagnostics RunEditorCommandPaletteDiagnostics()
    {
        EditorCommandPaletteDiagnostics diagnostics{};

        CommandRegistry commands = BuildDefaultEditorCommandRegistry();
        EditorRuntimeBridge bridge = BuildDefaultEditorRuntimeBridge(1600, 900);
        EditorSearchIndex searchIndex = BuildProbeSearchIndex(bridge);

        EditorFocusState focus{};
        EditorCommandContext commandContext = BuildEditorCommandContext(focus,
                                                                        false,
                                                                        true,
                                                                        false,
                                                                        true,
                                                                        true,
                                                                        true);
        EditorCommandStateCache commandState = BuildEditorCommandStateCache(commands, commandContext);
        EditorShortcutProfile shortcuts = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());

        EditorCommandPaletteState state = MakeDefaultEditorCommandPaletteState();
        diagnostics.openResult = ApplyEditorCommandPaletteInput(state, searchIndex, commandState, shortcuts, MakeEditorCommandPaletteOpenInput("save"));
        diagnostics.openOk = diagnostics.openResult.handled && diagnostics.openResult.opened && state.open;

        diagnostics.queryResult = ApplyEditorCommandPaletteInput(state, searchIndex, commandState, shortcuts, MakeEditorCommandPaletteQueryInput("selection"));
        EditorCommandPaletteSurface selectionSurface = BuildEditorCommandPaletteSurface(state, searchIndex, commandState, shortcuts, 1600, 900);
        diagnostics.queryOk = diagnostics.queryResult.handled && !selectionSurface.rows.empty() && state.query == "selection";

        diagnostics.moveResult = ApplyEditorCommandPaletteInput(state, searchIndex, commandState, shortcuts, MakeEditorCommandPaletteMoveInput(99));
        EditorCommandPaletteSurface movedSurface = BuildEditorCommandPaletteSurface(state, searchIndex, commandState, shortcuts, 1600, 900);
        diagnostics.selectionClampOk = movedSurface.rows.empty() || state.selectedIndex < movedSurface.rows.size();

        state = MakeDefaultEditorCommandPaletteState();
        ApplyEditorCommandPaletteInput(state, searchIndex, commandState, shortcuts, MakeEditorCommandPaletteOpenInput("delete"));
        EditorCommandPaletteInput acceptInput{};
        acceptInput.action = EditorCommandPaletteAction::AcceptSelection;
        acceptInput.target = EditorCommandPaletteTarget::ResultRow;
        diagnostics.disabledAcceptResult = ApplyEditorCommandPaletteInput(state,
                                                                         searchIndex,
                                                                         commandState,
                                                                         shortcuts,
                                                                         acceptInput);
        diagnostics.disabledRejectOk = diagnostics.disabledAcceptResult.handled && !diagnostics.disabledAcceptResult.commandQueued && state.open;

        state = MakeDefaultEditorCommandPaletteState();
        ApplyEditorCommandPaletteInput(state, searchIndex, commandState, shortcuts, MakeEditorCommandPaletteOpenInput("save"));
        diagnostics.acceptResult = ApplyEditorCommandPaletteInput(state,
                                                                  searchIndex,
                                                                  commandState,
                                                                  shortcuts,
                                                                  acceptInput);
        diagnostics.acceptCommandOk = diagnostics.acceptResult.commandQueued && diagnostics.acceptResult.command.id == CommandId::SaveScene && !state.open;

        state = MakeDefaultEditorCommandPaletteState();
        ApplyEditorCommandPaletteInput(state, searchIndex, commandState, shortcuts, MakeEditorCommandPaletteOpenInput(""));
        diagnostics.scrollResult = ApplyEditorCommandPaletteInput(state,
                                                                  searchIndex,
                                                                  commandState,
                                                                  shortcuts,
                                                                  MakeEditorCommandPaletteScrollInput(-120, 170));
        diagnostics.scrollOk = diagnostics.scrollResult.handled;
        diagnostics.surface = BuildEditorCommandPaletteSurface(state, searchIndex, commandState, shortcuts, 1600, 900);
        diagnostics.surfaceOk = ValidateEditorCommandPaletteSurface(diagnostics.surface, 1600, 900);
        diagnostics.state = state;
        diagnostics.rowCount = diagnostics.surface.rows.size();

        for (const EditorCommandPaletteRow& row : diagnostics.surface.rows)
        {
            if (row.resultKind == EditorSearchResultKind::Command)
            {
                ++diagnostics.commandRowCount;
            }
            if (!row.enabled)
            {
                ++diagnostics.disabledRowCount;
            }
            if (!row.rightText.empty())
            {
                ++diagnostics.shortcutTextCount;
            }
        }

        diagnostics.ok = diagnostics.openOk
            && diagnostics.queryOk
            && diagnostics.selectionClampOk
            && diagnostics.acceptCommandOk
            && diagnostics.disabledRejectOk
            && diagnostics.scrollOk
            && diagnostics.surfaceOk
            && diagnostics.rowCount >= 20
            && diagnostics.commandRowCount >= 10
            && diagnostics.shortcutTextCount >= 5;

        std::ostringstream out;
        out << "editor-command-palette-probe rows=" << diagnostics.rowCount
            << " commands=" << diagnostics.commandRowCount
            << " disabled=" << diagnostics.disabledRowCount
            << " shortcuts=" << diagnostics.shortcutTextCount
            << " accept=" << (diagnostics.acceptCommandOk ? 1 : 0)
            << " disabledReject=" << (diagnostics.disabledRejectOk ? 1 : 0)
            << " scroll=" << (diagnostics.scrollOk ? 1 : 0)
            << " surface=" << (diagnostics.surfaceOk ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        diagnostics.summary = out.str();
        return diagnostics;
    }

    EditorCommandPaletteDiagnostics RunEditorCommandPaletteProbe()
    {
        return RunEditorCommandPaletteDiagnostics();
    }

    std::string BuildEditorCommandPaletteProbeSummary()
    {
        return RunEditorCommandPaletteDiagnostics().summary;
    }
}
