#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Input/InputActions.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorShortcutContext
    {
        Global,
        SceneView,
        Hierarchy,
        Inspector,
        ProjectBrowser,
        Console,
        TextEntry
    };

    enum class EditorShortcutRepairCode
    {
        None,
        UnsupportedVersion,
        ParseFailed,
        InvalidAction,
        InvalidCommand,
        InvalidSlot,
        InvalidContext,
        DuplicateActionBinding,
        DuplicateChordConflict,
        TextEntryUnsafeBinding,
        EmptyProfile,
        ResetToDefault
    };

    enum class EditorShortcutRepairSeverity
    {
        Info,
        Warning,
        Error
    };

    struct EditorShortcutChord
    {
        InputSlot slot = InputSlot::KeyEscape;
        InputTrigger trigger = InputTrigger::Pressed;
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
        std::string displayName;
    };

    struct EditorShortcutBindingDesc
    {
        InputAction action = InputAction::Cancel;
        CommandId command = CommandId::Count;
        EditorShortcutContext context = EditorShortcutContext::Global;
        EditorShortcutChord chord{};
        std::string id;
        std::string label;
        std::string category;
        bool enabled = true;
        bool userOverride = false;
        bool readOnly = false;
    };

    struct EditorShortcutProfile
    {
        u32 schemaVersion = 1;
        std::string name = "Default";
        std::vector<EditorShortcutBindingDesc> bindings;
        u64 revision = 1;
    };

    struct EditorShortcutConflict
    {
        EditorShortcutContext context = EditorShortcutContext::Global;
        EditorShortcutChord chord{};
        InputAction firstAction = InputAction::Cancel;
        InputAction secondAction = InputAction::Cancel;
        CommandId firstCommand = CommandId::Count;
        CommandId secondCommand = CommandId::Count;
        std::string firstId;
        std::string secondId;
        std::string message;
    };

    struct EditorShortcutRepairIssue
    {
        EditorShortcutRepairCode code = EditorShortcutRepairCode::None;
        EditorShortcutRepairSeverity severity = EditorShortcutRepairSeverity::Info;
        std::string id;
        std::string message;
    };

    struct EditorShortcutRepairReport
    {
        std::vector<EditorShortcutRepairIssue> issues;
        bool repaired = false;
        bool resetToDefault = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorShortcutLoadResult
    {
        EditorShortcutProfile profile{};
        EditorShortcutRepairReport repair{};
        bool loadedFromText = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorShortcutDiagnostics
    {
        usize bindingCount = 0;
        usize commandBackedCount = 0;
        usize sceneViewBindingCount = 0;
        usize textEntryBindingCount = 0;
        usize conflictCount = 0;
        usize userOverrideCount = 0;
        usize disabledCount = 0;
        usize repairIssueCount = 0;
        bool serialized = false;
        bool loaded = false;
        bool inputMapBuilt = false;
        bool ok = false;
        std::string summary;
    };

    struct EditorShortcutPolicy
    {
        usize maxBindings = 512;
        bool dropInvalidBindings = true;
        bool dropDuplicateActionBindings = true;
        bool blockUnsafeTextEntryBindings = true;
        bool resetIfEmpty = true;
    };

    const char* ToString(EditorShortcutContext context);
    const char* ToString(EditorShortcutRepairCode code);
    const char* ToString(EditorShortcutRepairSeverity severity);

    EditorShortcutPolicy MakeDefaultEditorShortcutPolicy();
    EditorShortcutProfile BuildDefaultEditorShortcutProfile(const CommandRegistry& commands = BuildDefaultEditorCommandRegistry(), const InputMap& inputMap = BuildDefaultEditorInputMap());

    std::string FormatEditorShortcutChord(const EditorShortcutChord& chord);
    std::string FormatEditorShortcutBinding(const EditorShortcutBindingDesc& binding);
    std::string SerializeEditorShortcutProfile(const EditorShortcutProfile& profile);
    EditorShortcutLoadResult LoadEditorShortcutProfileWithRepair(std::string_view text, const CommandRegistry& commands = BuildDefaultEditorCommandRegistry(), const EditorShortcutPolicy& policy = MakeDefaultEditorShortcutPolicy());
    EditorShortcutRepairReport RepairEditorShortcutProfile(EditorShortcutProfile& profile, const CommandRegistry& commands = BuildDefaultEditorCommandRegistry(), const EditorShortcutPolicy& policy = MakeDefaultEditorShortcutPolicy());

    std::vector<EditorShortcutConflict> FindEditorShortcutConflicts(const EditorShortcutProfile& profile);
    bool SetEditorShortcutBinding(EditorShortcutProfile& profile, InputAction action, EditorShortcutContext context, const EditorShortcutChord& chord, bool userOverride = true);
    bool ClearEditorShortcutBinding(EditorShortcutProfile& profile, InputAction action, EditorShortcutContext context);
    InputMap BuildInputMapFromEditorShortcutProfile(const EditorShortcutProfile& profile);

    std::string FormatEditorShortcutConflict(const EditorShortcutConflict& conflict);
    std::string FormatEditorShortcutRepairIssue(const EditorShortcutRepairIssue& issue);
    std::string FormatEditorShortcutRepairReport(const EditorShortcutRepairReport& report);
    EditorShortcutDiagnostics RunEditorShortcutDiagnostics();
    std::string FormatEditorShortcutDiagnostics(const EditorShortcutDiagnostics& diagnostics);
}
