#include <AK/EditorUI/EditorInteractionProbe.hpp>

#include <sstream>

namespace AK
{
    EditorInteractionProbeResult BuildEditorInteractionProbe()
    {
        EditorInteractionProbeResult result{};
        result.context = BuildDefaultEditorInteractionContext(1600, 900);

        result.hierarchySelection = SelectEditorHierarchyItem(result.context, "entity:3:1", EditorSelectionChangeReason::UserClick);

        BeginEditorPropertyEdit(result.context, "Transform.Position");
        result.propertyCommit = CommitEditorPropertyEdit(result.context, "Transform.Position", MakeNumberPropertyValue(EditorPropertyType::Vec3, 2.0, 3.0, -8.0));

        OpenEditorCommandPalette(result.context, "save");
        result.paletteSummary = FormatEditorInteractionPaletteState(result.context.palette);
        result.paletteAccept = AcceptEditorCommandPaletteSelection(result.context);

        EditorInteractionEvent overlay{};
        overlay.kind = EditorInteractionKind::OverlayToggle;
        overlay.overlayId = 3;
        overlay.direction = -1;
        result.overlayToggle = ProcessEditorInteraction(result.context, overlay);

        OpenEditorCommandPalette(result.context, "position");
        result.propertySummary = FormatEditorPropertyChangeRequest(result.context.pendingPropertyChanges.empty() ? EditorPropertyChangeRequest{} : result.context.pendingPropertyChanges.back());
        result.diagnostics = ValidateEditorInteractionContext(result.context);

        result.ok = result.diagnostics.ok
            && result.hierarchySelection.consumed
            && result.propertyCommit.consumed
            && !result.propertyCommit.propertyChanges.empty()
            && !result.paletteAccept.commands.empty()
            && result.overlayToggle.consumed
            && !result.paletteSummary.empty()
            && !result.propertySummary.empty();

        std::ostringstream out;
        out << (result.ok ? "[ ok ]" : "[fail]")
            << " editor interaction / command palette workflow foundation"
            << " commands=" << result.diagnostics.pendingCommandCount
            << " selections=" << result.diagnostics.pendingSelectionChangeCount
            << " properties=" << result.diagnostics.pendingPropertyChangeCount
            << " paletteResults=" << result.diagnostics.paletteResultCount
            << " rejected=" << result.diagnostics.rejectedPropertyChangeCount;
        result.summary = out.str();
        return result;
    }
}
