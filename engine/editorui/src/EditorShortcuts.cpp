#include <AK/EditorUI/EditorShortcuts.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace AK
{
    namespace
    {
        constexpr int ToInt(InputAction action) { return static_cast<int>(action); }
        constexpr int ToInt(CommandId command) { return static_cast<int>(command); }
        constexpr int ToInt(InputSlot slot) { return static_cast<int>(slot); }
        constexpr int ToInt(InputTrigger trigger) { return static_cast<int>(trigger); }
        constexpr int ToInt(EditorShortcutContext context) { return static_cast<int>(context); }

        bool IsValidAction(int value)
        {
            return value >= 0 && value < static_cast<int>(InputAction::Count);
        }

        bool IsValidCommand(int value)
        {
            return value >= 0 && value <= static_cast<int>(CommandId::Count);
        }

        bool IsValidSlot(int value)
        {
            return value >= 0 && value < static_cast<int>(InputSlot::Count);
        }

        bool IsValidTrigger(int value)
        {
            return value >= 0 && value <= static_cast<int>(InputTrigger::Active);
        }

        bool IsValidContext(int value)
        {
            return value >= 0 && value <= static_cast<int>(EditorShortcutContext::TextEntry);
        }

        EditorShortcutContext ContextFromBinding(const InputBinding& binding)
        {
            if (binding.textEntryContext && !binding.editorContext)
            {
                return EditorShortcutContext::TextEntry;
            }

            switch (binding.action)
            {
                case InputAction::MoveForward:
                case InputAction::MoveBackward:
                case InputAction::MoveLeft:
                case InputAction::MoveRight:
                case InputAction::MoveDown:
                case InputAction::MoveUp:
                case InputAction::RotateYawLeft:
                case InputAction::RotateYawRight:
                case InputAction::RotatePitchUp:
                case InputAction::RotatePitchDown:
                case InputAction::RotateRollLeft:
                case InputAction::ScaleUp:
                case InputAction::ScaleDown:
                case InputAction::FocusSelection:
                case InputAction::ResetViewport:
                case InputAction::ToggleGridSnap:
                    return EditorShortcutContext::SceneView;
                case InputAction::SelectPrevious:
                case InputAction::SelectNext:
                case InputAction::RenameSelection:
                    return EditorShortcutContext::Hierarchy;
                default:
                    return EditorShortcutContext::Global;
            }
        }

        ModifierRule BoolToModifier(bool value)
        {
            return value ? ModifierRule::Required : ModifierRule::Forbidden;
        }

        std::string MakeBindingId(InputAction action, EditorShortcutContext context)
        {
            return std::string(ToString(context)) + "." + ToString(action);
        }

        std::string ChordKey(const EditorShortcutChord& chord)
        {
            std::ostringstream out;
            out << ToInt(chord.slot) << ':' << ToInt(chord.trigger) << ':' << (chord.ctrl ? 1 : 0) << ':' << (chord.shift ? 1 : 0) << ':' << (chord.alt ? 1 : 0);
            return out.str();
        }

        std::string BindingActionKey(const EditorShortcutBindingDesc& binding)
        {
            return std::to_string(ToInt(binding.action)) + ':' + std::to_string(ToInt(binding.context));
        }

        bool IsUnsafeTextEntryBinding(const EditorShortcutBindingDesc& binding)
        {
            if (binding.context != EditorShortcutContext::TextEntry)
            {
                return false;
            }

            if (binding.chord.ctrl || binding.chord.alt)
            {
                return false;
            }

            switch (binding.chord.slot)
            {
                case InputSlot::KeyEscape:
                case InputSlot::KeyEnter:
                case InputSlot::KeyBackspace:
                case InputSlot::KeyDelete:
                case InputSlot::KeyLeft:
                case InputSlot::KeyRight:
                case InputSlot::KeyHome:
                    return false;
                default:
                    return true;
            }
        }

        const CommandDescriptor* FindCommandForAction(const CommandRegistry& commands, InputAction action)
        {
            return commands.FindByShortcut(action);
        }

        void AddIssue(EditorShortcutRepairReport& report, EditorShortcutRepairCode code, EditorShortcutRepairSeverity severity, std::string message, std::string id = {})
        {
            EditorShortcutRepairIssue issue{};
            issue.code = code;
            issue.severity = severity;
            issue.message = std::move(message);
            issue.id = std::move(id);
            report.issues.push_back(std::move(issue));
            if (code != EditorShortcutRepairCode::None)
            {
                report.repaired = true;
            }
        }

        bool ParseEditorShortcutProfile(std::string_view text, EditorShortcutProfile& profile)
        {
            std::istringstream input{std::string(text)};
            std::string magic;
            u32 version = 0;
            input >> magic >> version;
            if (magic != "AKEDITORSHORTCUTS" || version != 1)
            {
                return false;
            }

            profile = {};
            profile.schemaVersion = version;

            std::string tag;
            while (input >> tag)
            {
                if (tag == "name")
                {
                    input >> std::quoted(profile.name);
                }
                else if (tag == "revision")
                {
                    input >> profile.revision;
                }
                else if (tag == "binding")
                {
                    EditorShortcutBindingDesc binding{};
                    int action = 0;
                    int command = 0;
                    int context = 0;
                    int slot = 0;
                    int trigger = 0;
                    int ctrl = 0;
                    int shift = 0;
                    int alt = 0;
                    int enabled = 1;
                    int userOverride = 0;
                    int readOnly = 0;
                    input >> std::quoted(binding.id) >> action >> command >> context >> slot >> trigger >> ctrl >> shift >> alt >> enabled >> userOverride >> readOnly >> std::quoted(binding.label) >> std::quoted(binding.category) >> std::quoted(binding.chord.displayName);
                    if (!input)
                    {
                        return false;
                    }

                    binding.action = IsValidAction(action) ? static_cast<InputAction>(action) : InputAction::Count;
                    binding.command = IsValidCommand(command) ? static_cast<CommandId>(command) : CommandId::Count;
                    binding.context = IsValidContext(context) ? static_cast<EditorShortcutContext>(context) : EditorShortcutContext::Global;
                    binding.chord.slot = IsValidSlot(slot) ? static_cast<InputSlot>(slot) : InputSlot::Count;
                    binding.chord.trigger = IsValidTrigger(trigger) ? static_cast<InputTrigger>(trigger) : InputTrigger::Pressed;
                    binding.chord.ctrl = ctrl != 0;
                    binding.chord.shift = shift != 0;
                    binding.chord.alt = alt != 0;
                    binding.enabled = enabled != 0;
                    binding.userOverride = userOverride != 0;
                    binding.readOnly = readOnly != 0;
                    profile.bindings.push_back(std::move(binding));
                }
                else
                {
                    std::string ignored;
                    std::getline(input, ignored);
                }
            }

            return true;
        }
    }

    const char* ToString(EditorShortcutContext context)
    {
        switch (context)
        {
            case EditorShortcutContext::Global: return "global";
            case EditorShortcutContext::SceneView: return "scene-view";
            case EditorShortcutContext::Hierarchy: return "hierarchy";
            case EditorShortcutContext::Inspector: return "inspector";
            case EditorShortcutContext::ProjectBrowser: return "project";
            case EditorShortcutContext::Console: return "console";
            case EditorShortcutContext::TextEntry: return "text-entry";
            default: return "unknown";
        }
    }

    const char* ToString(EditorShortcutRepairCode code)
    {
        switch (code)
        {
            case EditorShortcutRepairCode::None: return "None";
            case EditorShortcutRepairCode::UnsupportedVersion: return "UnsupportedVersion";
            case EditorShortcutRepairCode::ParseFailed: return "ParseFailed";
            case EditorShortcutRepairCode::InvalidAction: return "InvalidAction";
            case EditorShortcutRepairCode::InvalidCommand: return "InvalidCommand";
            case EditorShortcutRepairCode::InvalidSlot: return "InvalidSlot";
            case EditorShortcutRepairCode::InvalidContext: return "InvalidContext";
            case EditorShortcutRepairCode::DuplicateActionBinding: return "DuplicateActionBinding";
            case EditorShortcutRepairCode::DuplicateChordConflict: return "DuplicateChordConflict";
            case EditorShortcutRepairCode::TextEntryUnsafeBinding: return "TextEntryUnsafeBinding";
            case EditorShortcutRepairCode::EmptyProfile: return "EmptyProfile";
            case EditorShortcutRepairCode::ResetToDefault: return "ResetToDefault";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorShortcutRepairSeverity severity)
    {
        switch (severity)
        {
            case EditorShortcutRepairSeverity::Info: return "info";
            case EditorShortcutRepairSeverity::Warning: return "warning";
            case EditorShortcutRepairSeverity::Error: return "error";
            default: return "unknown";
        }
    }

    EditorShortcutPolicy MakeDefaultEditorShortcutPolicy()
    {
        return {};
    }

    EditorShortcutProfile BuildDefaultEditorShortcutProfile(const CommandRegistry& commands, const InputMap& inputMap)
    {
        EditorShortcutProfile profile{};
        profile.name = "AK Default";

        for (const InputBinding& inputBinding : inputMap.Bindings())
        {
            EditorShortcutBindingDesc binding{};
            binding.action = inputBinding.action;
            binding.context = ContextFromBinding(inputBinding);
            binding.command = CommandId::Count;
            binding.chord.slot = inputBinding.slot;
            binding.chord.trigger = inputBinding.trigger;
            binding.chord.ctrl = inputBinding.ctrl == ModifierRule::Required;
            binding.chord.shift = inputBinding.shift == ModifierRule::Required;
            binding.chord.alt = false;
            binding.chord.displayName = inputBinding.displayName && inputBinding.displayName[0] != '\0' ? inputBinding.displayName : FormatEditorShortcutChord(binding.chord);
            binding.id = MakeBindingId(binding.action, binding.context);
            binding.label = ToString(binding.action);
            binding.category = ToString(binding.context);

            if (const CommandDescriptor* command = FindCommandForAction(commands, binding.action))
            {
                binding.command = command->id;
                binding.label = command->displayName;
                binding.category = command->category;
            }

            profile.bindings.push_back(std::move(binding));
        }

        return profile;
    }

    std::string FormatEditorShortcutChord(const EditorShortcutChord& chord)
    {
        std::ostringstream out;
        if (chord.ctrl)
        {
            out << "Ctrl+";
        }
        if (chord.shift)
        {
            out << "Shift+";
        }
        if (chord.alt)
        {
            out << "Alt+";
        }
        out << (chord.displayName.empty() ? ToString(chord.slot) : chord.displayName);
        return out.str();
    }

    std::string FormatEditorShortcutBinding(const EditorShortcutBindingDesc& binding)
    {
        std::ostringstream out;
        out << binding.id << " [" << ToString(binding.context) << "] " << FormatEditorShortcutChord(binding.chord)
            << " -> " << ToString(binding.action);
        if (binding.command != CommandId::Count)
        {
            out << " / " << ToString(binding.command);
        }
        if (!binding.enabled)
        {
            out << " disabled";
        }
        if (binding.userOverride)
        {
            out << " override";
        }
        return out.str();
    }

    std::string SerializeEditorShortcutProfile(const EditorShortcutProfile& profile)
    {
        std::ostringstream out;
        out << "AKEDITORSHORTCUTS " << profile.schemaVersion << '\n';
        out << "name " << std::quoted(profile.name) << '\n';
        out << "revision " << profile.revision << '\n';
        for (const EditorShortcutBindingDesc& binding : profile.bindings)
        {
            out << "binding " << std::quoted(binding.id)
                << ' ' << ToInt(binding.action)
                << ' ' << ToInt(binding.command)
                << ' ' << ToInt(binding.context)
                << ' ' << ToInt(binding.chord.slot)
                << ' ' << ToInt(binding.chord.trigger)
                << ' ' << (binding.chord.ctrl ? 1 : 0)
                << ' ' << (binding.chord.shift ? 1 : 0)
                << ' ' << (binding.chord.alt ? 1 : 0)
                << ' ' << (binding.enabled ? 1 : 0)
                << ' ' << (binding.userOverride ? 1 : 0)
                << ' ' << (binding.readOnly ? 1 : 0)
                << ' ' << std::quoted(binding.label)
                << ' ' << std::quoted(binding.category)
                << ' ' << std::quoted(binding.chord.displayName)
                << '\n';
        }
        return out.str();
    }

    std::vector<EditorShortcutConflict> FindEditorShortcutConflicts(const EditorShortcutProfile& profile)
    {
        std::vector<EditorShortcutConflict> conflicts;
        std::unordered_map<std::string, const EditorShortcutBindingDesc*> seen;
        for (const EditorShortcutBindingDesc& binding : profile.bindings)
        {
            if (!binding.enabled)
            {
                continue;
            }

            const std::string key = std::to_string(ToInt(binding.context)) + ':' + ChordKey(binding.chord);
            auto [it, inserted] = seen.emplace(key, &binding);
            if (!inserted && it->second->action != binding.action)
            {
                EditorShortcutConflict conflict{};
                conflict.context = binding.context;
                conflict.chord = binding.chord;
                conflict.firstAction = it->second->action;
                conflict.secondAction = binding.action;
                conflict.firstCommand = it->second->command;
                conflict.secondCommand = binding.command;
                conflict.firstId = it->second->id;
                conflict.secondId = binding.id;
                conflict.message = FormatEditorShortcutChord(binding.chord) + " is bound twice in " + ToString(binding.context);
                conflicts.push_back(std::move(conflict));
            }
        }
        return conflicts;
    }

    EditorShortcutRepairReport RepairEditorShortcutProfile(EditorShortcutProfile& profile, const CommandRegistry& commands, const EditorShortcutPolicy& policy)
    {
        EditorShortcutRepairReport report{};
        if (profile.schemaVersion != 1)
        {
            AddIssue(report, EditorShortcutRepairCode::UnsupportedVersion, EditorShortcutRepairSeverity::Error, "unsupported shortcut profile version");
            profile = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());
            report.resetToDefault = true;
            report.ok = true;
            report.summary = FormatEditorShortcutRepairReport(report);
            return report;
        }

        std::vector<EditorShortcutBindingDesc> repaired;
        repaired.reserve(profile.bindings.size());
        std::unordered_set<std::string> seenActions;
        for (EditorShortcutBindingDesc binding : profile.bindings)
        {
            if (binding.action == InputAction::Count)
            {
                AddIssue(report, EditorShortcutRepairCode::InvalidAction, EditorShortcutRepairSeverity::Warning, "dropped shortcut with invalid action", binding.id);
                if (policy.dropInvalidBindings) continue;
            }
            if (binding.command != CommandId::Count && !commands.Has(binding.command))
            {
                AddIssue(report, EditorShortcutRepairCode::InvalidCommand, EditorShortcutRepairSeverity::Warning, "cleared invalid command for shortcut", binding.id);
                binding.command = CommandId::Count;
            }
            if (binding.chord.slot == InputSlot::Count)
            {
                AddIssue(report, EditorShortcutRepairCode::InvalidSlot, EditorShortcutRepairSeverity::Warning, "dropped shortcut with invalid slot", binding.id);
                if (policy.dropInvalidBindings) continue;
            }
            if (IsUnsafeTextEntryBinding(binding))
            {
                AddIssue(report, EditorShortcutRepairCode::TextEntryUnsafeBinding, EditorShortcutRepairSeverity::Warning, "dropped unsafe text-entry shortcut", binding.id);
                if (policy.blockUnsafeTextEntryBindings) continue;
            }

            if (binding.id.empty())
            {
                binding.id = MakeBindingId(binding.action, binding.context);
            }
            if (binding.label.empty())
            {
                binding.label = ToString(binding.action);
            }
            if (binding.category.empty())
            {
                binding.category = ToString(binding.context);
            }
            if (binding.chord.displayName.empty())
            {
                binding.chord.displayName = ToString(binding.chord.slot);
            }

            const std::string actionKey = BindingActionKey(binding);
            if (!seenActions.insert(actionKey).second)
            {
                AddIssue(report, EditorShortcutRepairCode::DuplicateActionBinding, EditorShortcutRepairSeverity::Warning, "dropped duplicate action binding", binding.id);
                if (policy.dropDuplicateActionBindings) continue;
            }
            repaired.push_back(std::move(binding));
            if (repaired.size() >= policy.maxBindings)
            {
                break;
            }
        }

        profile.bindings = std::move(repaired);
        if (profile.bindings.empty() && policy.resetIfEmpty)
        {
            AddIssue(report, EditorShortcutRepairCode::EmptyProfile, EditorShortcutRepairSeverity::Error, "shortcut profile was empty; reset to defaults");
            profile = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());
            report.resetToDefault = true;
        }

        const std::vector<EditorShortcutConflict> conflicts = FindEditorShortcutConflicts(profile);
        for (const EditorShortcutConflict& conflict : conflicts)
        {
            AddIssue(report, EditorShortcutRepairCode::DuplicateChordConflict, EditorShortcutRepairSeverity::Warning, conflict.message, conflict.secondId);
        }

        report.ok = true;
        report.summary = FormatEditorShortcutRepairReport(report);
        return report;
    }

    EditorShortcutLoadResult LoadEditorShortcutProfileWithRepair(std::string_view text, const CommandRegistry& commands, const EditorShortcutPolicy& policy)
    {
        EditorShortcutLoadResult result{};
        if (!ParseEditorShortcutProfile(text, result.profile))
        {
            result.profile = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());
            AddIssue(result.repair, EditorShortcutRepairCode::ParseFailed, EditorShortcutRepairSeverity::Error, "failed to parse shortcut profile; reset to defaults");
            result.repair.resetToDefault = true;
            result.repair.ok = true;
            result.repair.summary = FormatEditorShortcutRepairReport(result.repair);
            result.loadedFromText = false;
            result.ok = true;
            result.summary = result.repair.summary;
            return result;
        }

        result.loadedFromText = true;
        result.repair = RepairEditorShortcutProfile(result.profile, commands, policy);
        result.ok = result.repair.ok;
        result.summary = result.repair.summary;
        return result;
    }

    bool SetEditorShortcutBinding(EditorShortcutProfile& profile, InputAction action, EditorShortcutContext context, const EditorShortcutChord& chord, bool userOverride)
    {
        for (EditorShortcutBindingDesc& binding : profile.bindings)
        {
            if (binding.action == action && binding.context == context)
            {
                if (binding.readOnly)
                {
                    return false;
                }
                binding.chord = chord;
                if (binding.chord.displayName.empty())
                {
                    binding.chord.displayName = ToString(chord.slot);
                }
                binding.enabled = true;
                binding.userOverride = userOverride;
                ++profile.revision;
                return true;
            }
        }

        EditorShortcutBindingDesc binding{};
        binding.action = action;
        binding.context = context;
        binding.chord = chord;
        binding.chord.displayName = chord.displayName.empty() ? ToString(chord.slot) : chord.displayName;
        binding.id = MakeBindingId(action, context);
        binding.label = ToString(action);
        binding.category = ToString(context);
        binding.userOverride = userOverride;
        profile.bindings.push_back(std::move(binding));
        ++profile.revision;
        return true;
    }

    bool ClearEditorShortcutBinding(EditorShortcutProfile& profile, InputAction action, EditorShortcutContext context)
    {
        for (EditorShortcutBindingDesc& binding : profile.bindings)
        {
            if (binding.action == action && binding.context == context)
            {
                if (binding.readOnly)
                {
                    return false;
                }
                binding.enabled = false;
                binding.userOverride = true;
                ++profile.revision;
                return true;
            }
        }
        return false;
    }

    InputMap BuildInputMapFromEditorShortcutProfile(const EditorShortcutProfile& profile)
    {
        InputMap map;
        for (const EditorShortcutBindingDesc& shortcut : profile.bindings)
        {
            if (!shortcut.enabled || shortcut.chord.alt)
            {
                continue;
            }

            InputBinding binding{};
            binding.action = shortcut.action;
            binding.slot = shortcut.chord.slot;
            binding.trigger = shortcut.chord.trigger;
            binding.ctrl = BoolToModifier(shortcut.chord.ctrl);
            binding.shift = BoolToModifier(shortcut.chord.shift);
            binding.editorContext = shortcut.context != EditorShortcutContext::TextEntry;
            binding.textEntryContext = shortcut.context == EditorShortcutContext::TextEntry;
            binding.displayName = "";
            map.AddBinding(binding);
        }
        return map;
    }

    std::string FormatEditorShortcutConflict(const EditorShortcutConflict& conflict)
    {
        std::ostringstream out;
        out << "shortcut conflict: " << FormatEditorShortcutChord(conflict.chord)
            << " in " << ToString(conflict.context)
            << " between " << conflict.firstId << " and " << conflict.secondId;
        return out.str();
    }

    std::string FormatEditorShortcutRepairIssue(const EditorShortcutRepairIssue& issue)
    {
        std::ostringstream out;
        out << ToString(issue.severity) << ':' << ToString(issue.code);
        if (!issue.id.empty())
        {
            out << '[' << issue.id << ']';
        }
        out << ' ' << issue.message;
        return out.str();
    }

    std::string FormatEditorShortcutRepairReport(const EditorShortcutRepairReport& report)
    {
        std::ostringstream out;
        out << "editor-shortcuts repair issues=" << report.issues.size()
            << " repaired=" << (report.repaired ? 1 : 0)
            << " reset=" << (report.resetToDefault ? 1 : 0);
        return out.str();
    }

    EditorShortcutDiagnostics RunEditorShortcutDiagnostics()
    {
        EditorShortcutDiagnostics diagnostics{};
        const CommandRegistry commands = BuildDefaultEditorCommandRegistry();
        EditorShortcutProfile profile = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());
        diagnostics.bindingCount = profile.bindings.size();
        for (const EditorShortcutBindingDesc& binding : profile.bindings)
        {
            if (binding.command != CommandId::Count) ++diagnostics.commandBackedCount;
            if (binding.context == EditorShortcutContext::SceneView) ++diagnostics.sceneViewBindingCount;
            if (binding.context == EditorShortcutContext::TextEntry) ++diagnostics.textEntryBindingCount;
            if (binding.userOverride) ++diagnostics.userOverrideCount;
            if (!binding.enabled) ++diagnostics.disabledCount;
        }
        diagnostics.conflictCount = FindEditorShortcutConflicts(profile).size();
        const std::string text = SerializeEditorShortcutProfile(profile);
        diagnostics.serialized = !text.empty();
        const EditorShortcutLoadResult loaded = LoadEditorShortcutProfileWithRepair(text, commands);
        diagnostics.loaded = loaded.ok;
        diagnostics.repairIssueCount = loaded.repair.issues.size();
        const InputMap rebuilt = BuildInputMapFromEditorShortcutProfile(loaded.profile);
        diagnostics.inputMapBuilt = !rebuilt.Bindings().empty();
        diagnostics.ok = diagnostics.bindingCount > 0 && diagnostics.conflictCount == 0 && diagnostics.serialized && diagnostics.loaded && diagnostics.inputMapBuilt;
        diagnostics.summary = FormatEditorShortcutDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorShortcutDiagnostics(const EditorShortcutDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-shortcuts bindings=" << diagnostics.bindingCount
            << " commandBacked=" << diagnostics.commandBackedCount
            << " scene=" << diagnostics.sceneViewBindingCount
            << " text=" << diagnostics.textEntryBindingCount
            << " conflicts=" << diagnostics.conflictCount
            << " overrides=" << diagnostics.userOverrideCount
            << " disabled=" << diagnostics.disabledCount
            << " inputMap=" << (diagnostics.inputMapBuilt ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        return out.str();
    }
}
