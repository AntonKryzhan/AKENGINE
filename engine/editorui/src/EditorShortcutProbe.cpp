#include <AK/EditorUI/EditorShortcutProbe.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool HasIssue(const EditorShortcutRepairReport& report, EditorShortcutRepairCode code)
        {
            return std::any_of(report.issues.begin(), report.issues.end(), [code](const EditorShortcutRepairIssue& issue)
            {
                return issue.code == code;
            });
        }
    }

    EditorShortcutProbeResult BuildEditorShortcutProbe()
    {
        EditorShortcutProbeResult result{};
        const CommandRegistry commands = BuildDefaultEditorCommandRegistry();
        EditorShortcutProfile profile = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());
        result.defaultBindingCount = profile.bindings.size();

        EditorShortcutChord focusChord{};
        focusChord.slot = InputSlot::KeyF;
        focusChord.trigger = InputTrigger::Pressed;
        focusChord.ctrl = true;
        focusChord.displayName = "Ctrl+F";
        result.overrideApplied = SetEditorShortcutBinding(profile, InputAction::FocusSelection, EditorShortcutContext::SceneView, focusChord, true);

        const std::string cleanText = SerializeEditorShortcutProfile(profile);
        result.cleanLoad = LoadEditorShortcutProfileWithRepair(cleanText, commands);
        result.inputMapBuilt = !BuildInputMapFromEditorShortcutProfile(result.cleanLoad.profile).Bindings().empty();

        EditorShortcutProfile conflicted = profile;
        EditorShortcutChord saveChord{};
        saveChord.slot = InputSlot::KeyS;
        saveChord.trigger = InputTrigger::Pressed;
        saveChord.ctrl = true;
        saveChord.displayName = "Ctrl+S";
        SetEditorShortcutBinding(conflicted, InputAction::LoadScene, EditorShortcutContext::Global, saveChord, true);
        result.conflictCount = FindEditorShortcutConflicts(conflicted).size();
        result.conflictDetected = result.conflictCount > 0;

        EditorShortcutProfile corrupted = conflicted;
        if (!corrupted.bindings.empty())
        {
            corrupted.bindings.push_back(corrupted.bindings.front());
        }

        EditorShortcutBindingDesc invalid{};
        invalid.id = "invalid.action";
        invalid.action = InputAction::Count;
        invalid.command = CommandId::Count;
        invalid.context = EditorShortcutContext::Global;
        invalid.chord.slot = InputSlot::KeyA;
        invalid.chord.displayName = "A";
        corrupted.bindings.push_back(invalid);

        EditorShortcutBindingDesc unsafe{};
        unsafe.id = "text.unsafe";
        unsafe.action = InputAction::NewEntity;
        unsafe.context = EditorShortcutContext::TextEntry;
        unsafe.chord.slot = InputSlot::KeyN;
        unsafe.chord.displayName = "N";
        corrupted.bindings.push_back(unsafe);

        result.repairedLoad = LoadEditorShortcutProfileWithRepair(SerializeEditorShortcutProfile(corrupted), commands);
        result.invalidDropped = HasIssue(result.repairedLoad.repair, EditorShortcutRepairCode::InvalidAction);
        result.duplicateDropped = HasIssue(result.repairedLoad.repair, EditorShortcutRepairCode::DuplicateActionBinding);
        result.textEntryBlocked = HasIssue(result.repairedLoad.repair, EditorShortcutRepairCode::TextEntryUnsafeBinding);
        result.diagnostics = RunEditorShortcutDiagnostics();
        result.ok = result.defaultBindingCount > 0
            && result.overrideApplied
            && result.cleanLoad.ok
            && result.inputMapBuilt
            && result.conflictDetected
            && result.repairedLoad.ok
            && result.invalidDropped
            && result.duplicateDropped
            && result.textEntryBlocked
            && result.diagnostics.ok;

        std::ostringstream out;
        out << "editor-shortcut-probe defaults=" << result.defaultBindingCount
            << " override=" << (result.overrideApplied ? 1 : 0)
            << " conflicts=" << result.conflictCount
            << " invalid=" << (result.invalidDropped ? 1 : 0)
            << " duplicate=" << (result.duplicateDropped ? 1 : 0)
            << " textSafe=" << (result.textEntryBlocked ? 1 : 0)
            << " inputMap=" << (result.inputMapBuilt ? 1 : 0)
            << " ok=" << (result.ok ? 1 : 0);
        result.summary = out.str();
        return result;
    }
}
