#include <AK/Input/InputActions.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <unordered_set>

namespace AK
{
    namespace
    {
        constexpr std::size_t ActionIndex(InputAction action)
        {
            return static_cast<std::size_t>(action);
        }

        bool IsContextAllowed(const InputBinding& binding, InputContext context)
        {
            switch (context)
            {
                case InputContext::Editor:
                    return binding.editorContext;
                case InputContext::TextEntry:
                    return binding.textEntryContext;
                default:
                    return false;
            }
        }

        bool IsModifierAllowed(bool down, ModifierRule rule)
        {
            switch (rule)
            {
                case ModifierRule::Any:
                    return true;
                case ModifierRule::Required:
                    return down;
                case ModifierRule::Forbidden:
                    return !down;
                default:
                    return false;
            }
        }

        bool SlotPressed(const WindowInput& input, InputSlot slot)
        {
            switch (slot)
            {
                case InputSlot::KeyUp:
                    return input.keyUpPressed;
                case InputSlot::KeyDown:
                    return input.keyDownPressed;
                case InputSlot::KeyLeft:
                    return input.keyLeftPressed;
                case InputSlot::KeyRight:
                    return input.keyRightPressed;
                case InputSlot::KeyDelete:
                    return input.keyDeletePressed;
                case InputSlot::KeyEscape:
                    return input.keyEscapePressed;
                case InputSlot::KeyEnter:
                    return input.keyEnterPressed;
                case InputSlot::KeyBackspace:
                    return input.keyBackspacePressed;
                case InputSlot::KeyHome:
                    return input.keyHomePressed;
                case InputSlot::KeyF2:
                    return input.keyF2Pressed;
                case InputSlot::KeyF:
                    return input.keyFPressed;
                case InputSlot::KeyG:
                    return input.keyGPressed;
                case InputSlot::KeyCtrlD:
                    return input.keyCtrlDPressed;
                case InputSlot::KeyCtrlZ:
                    return input.keyCtrlZPressed;
                case InputSlot::KeyCtrlY:
                    return input.keyCtrlYPressed;
                case InputSlot::KeyN:
                    return input.keyNPressed;
                case InputSlot::KeyS:
                    return input.keySPressed;
                case InputSlot::KeyL:
                    return input.keyLPressed;
                case InputSlot::KeyR:
                    return input.keyRPressed;
                case InputSlot::KeyW:
                    return input.keyWPressed;
                case InputSlot::KeyA:
                    return input.keyAPressed;
                case InputSlot::KeyD:
                    return input.keyDPressed;
                case InputSlot::KeyQ:
                    return input.keyQPressed;
                case InputSlot::KeyE:
                    return input.keyEPressed;
                case InputSlot::KeyI:
                    return input.keyIPressed;
                case InputSlot::KeyJ:
                    return input.keyJPressed;
                case InputSlot::KeyK:
                    return input.keyKPressed;
                case InputSlot::KeyMinus:
                    return input.keyMinusPressed;
                case InputSlot::KeyEquals:
                    return input.keyEqualsPressed;
                default:
                    return false;
            }
        }

        bool SlotActive(const WindowInput& input, InputSlot slot)
        {
            return SlotPressed(input, slot);
        }

        bool BindingFired(const WindowInput& input, const InputBinding& binding)
        {
            if (!IsModifierAllowed(input.keyCtrlDown, binding.ctrl) || !IsModifierAllowed(input.keyShiftDown, binding.shift))
            {
                return false;
            }

            switch (binding.trigger)
            {
                case InputTrigger::Pressed:
                    return SlotPressed(input, binding.slot);
                case InputTrigger::Active:
                    return SlotActive(input, binding.slot);
                default:
                    return false;
            }
        }

        std::uint64_t ConflictKey(const InputBinding& binding, InputContext context)
        {
            const std::uint64_t slot = static_cast<std::uint64_t>(binding.slot);
            const std::uint64_t trigger = static_cast<std::uint64_t>(binding.trigger);
            const std::uint64_t ctrl = static_cast<std::uint64_t>(binding.ctrl);
            const std::uint64_t shift = static_cast<std::uint64_t>(binding.shift);
            const std::uint64_t ctx = static_cast<std::uint64_t>(context);
            return slot | (trigger << 8U) | (ctrl << 16U) | (shift << 24U) | (ctx << 32U);
        }

        bool ActionBoundInAnyContext(const InputMap& map, InputAction action)
        {
            const auto& bindings = map.Bindings();
            return std::any_of(bindings.begin(), bindings.end(), [action](const InputBinding& binding)
            {
                return binding.action == action;
            });
        }

        void AddEditorBinding(InputMap& map, InputAction action, InputSlot slot, InputTrigger trigger, ModifierRule ctrl, const char* displayName)
        {
            map.AddBinding({action, slot, trigger, ctrl, ModifierRule::Any, true, false, displayName});
        }

        void AddTextBinding(InputMap& map, InputAction action, InputSlot slot, InputTrigger trigger, const char* displayName)
        {
            map.AddBinding({action, slot, trigger, ModifierRule::Any, ModifierRule::Any, false, true, displayName});
        }
    }

    bool InputActionSnapshot::Pressed(InputAction action) const
    {
        return states[ActionIndex(action)].pressed;
    }

    bool InputActionSnapshot::Active(InputAction action) const
    {
        return states[ActionIndex(action)].active;
    }

    void InputMap::AddBinding(const InputBinding& binding)
    {
        mBindings.push_back(binding);
    }

    InputActionSnapshot InputMap::Evaluate(const WindowInput& input, InputContext context) const
    {
        InputActionSnapshot snapshot{};
        snapshot.ctrlDown = input.keyCtrlDown;
        snapshot.shiftDown = input.keyShiftDown;

        for (const InputBinding& binding : mBindings)
        {
            if (!IsContextAllowed(binding, context) || !BindingFired(input, binding))
            {
                continue;
            }

            InputActionState& state = snapshot.states[ActionIndex(binding.action)];
            if (binding.trigger == InputTrigger::Pressed)
            {
                if (!state.pressed)
                {
                    snapshot.pressedActions.push_back(binding.action);
                }
                state.pressed = true;
                state.active = true;
            }
            else
            {
                if (!state.active)
                {
                    snapshot.activeActions.push_back(binding.action);
                }
                state.active = true;
            }
        }

        return snapshot;
    }

    const std::vector<InputBinding>& InputMap::Bindings() const
    {
        return mBindings;
    }

    InputMap BuildDefaultEditorInputMap()
    {
        InputMap map;

        AddEditorBinding(map, InputAction::Cancel, InputSlot::KeyEscape, InputTrigger::Pressed, ModifierRule::Any, "Esc");
        AddEditorBinding(map, InputAction::NewEntity, InputSlot::KeyN, InputTrigger::Pressed, ModifierRule::Forbidden, "N");
        AddEditorBinding(map, InputAction::DeleteSelection, InputSlot::KeyDelete, InputTrigger::Pressed, ModifierRule::Any, "Delete");
        AddEditorBinding(map, InputAction::SaveScene, InputSlot::KeyS, InputTrigger::Pressed, ModifierRule::Required, "S");
        AddEditorBinding(map, InputAction::LoadScene, InputSlot::KeyL, InputTrigger::Pressed, ModifierRule::Required, "L");
        AddEditorBinding(map, InputAction::RescanAssets, InputSlot::KeyR, InputTrigger::Pressed, ModifierRule::Forbidden, "R");
        AddEditorBinding(map, InputAction::Undo, InputSlot::KeyCtrlZ, InputTrigger::Pressed, ModifierRule::Any, "Ctrl+Z");
        AddEditorBinding(map, InputAction::Redo, InputSlot::KeyCtrlY, InputTrigger::Pressed, ModifierRule::Any, "Ctrl+Y");
        AddEditorBinding(map, InputAction::DuplicateSelection, InputSlot::KeyCtrlD, InputTrigger::Pressed, ModifierRule::Any, "Ctrl+D");
        AddEditorBinding(map, InputAction::FocusSelection, InputSlot::KeyF, InputTrigger::Pressed, ModifierRule::Forbidden, "F");
        AddEditorBinding(map, InputAction::ResetViewport, InputSlot::KeyHome, InputTrigger::Pressed, ModifierRule::Any, "Home");
        AddEditorBinding(map, InputAction::ToggleGridSnap, InputSlot::KeyG, InputTrigger::Pressed, ModifierRule::Forbidden, "G");
        AddEditorBinding(map, InputAction::SelectPrevious, InputSlot::KeyUp, InputTrigger::Pressed, ModifierRule::Any, "Up");
        AddEditorBinding(map, InputAction::SelectNext, InputSlot::KeyDown, InputTrigger::Pressed, ModifierRule::Any, "Down");
        AddEditorBinding(map, InputAction::RenameSelection, InputSlot::KeyF2, InputTrigger::Pressed, ModifierRule::Any, "F2");

        AddEditorBinding(map, InputAction::MoveForward, InputSlot::KeyW, InputTrigger::Active, ModifierRule::Forbidden, "W");
        AddEditorBinding(map, InputAction::MoveBackward, InputSlot::KeyS, InputTrigger::Active, ModifierRule::Forbidden, "S");
        AddEditorBinding(map, InputAction::MoveLeft, InputSlot::KeyA, InputTrigger::Active, ModifierRule::Forbidden, "A");
        AddEditorBinding(map, InputAction::MoveRight, InputSlot::KeyD, InputTrigger::Active, ModifierRule::Forbidden, "D");
        AddEditorBinding(map, InputAction::MoveDown, InputSlot::KeyQ, InputTrigger::Active, ModifierRule::Forbidden, "Q");
        AddEditorBinding(map, InputAction::MoveUp, InputSlot::KeyE, InputTrigger::Active, ModifierRule::Forbidden, "E");
        AddEditorBinding(map, InputAction::RotateYawLeft, InputSlot::KeyLeft, InputTrigger::Active, ModifierRule::Forbidden, "Left");
        AddEditorBinding(map, InputAction::RotateYawRight, InputSlot::KeyRight, InputTrigger::Active, ModifierRule::Forbidden, "Right");
        AddEditorBinding(map, InputAction::RotatePitchUp, InputSlot::KeyI, InputTrigger::Active, ModifierRule::Forbidden, "I");
        AddEditorBinding(map, InputAction::RotatePitchDown, InputSlot::KeyK, InputTrigger::Active, ModifierRule::Forbidden, "K");
        AddEditorBinding(map, InputAction::RotateRollLeft, InputSlot::KeyJ, InputTrigger::Active, ModifierRule::Forbidden, "J");
        AddEditorBinding(map, InputAction::ScaleUp, InputSlot::KeyEquals, InputTrigger::Active, ModifierRule::Forbidden, "+");
        AddEditorBinding(map, InputAction::ScaleDown, InputSlot::KeyMinus, InputTrigger::Active, ModifierRule::Forbidden, "-");

        AddTextBinding(map, InputAction::Cancel, InputSlot::KeyEscape, InputTrigger::Pressed, "Esc");
        AddTextBinding(map, InputAction::Confirm, InputSlot::KeyEnter, InputTrigger::Pressed, "Enter");
        AddTextBinding(map, InputAction::RenameBackspace, InputSlot::KeyBackspace, InputTrigger::Pressed, "Backspace");

        return map;
    }

    InputDiagnostics BuildInputDiagnostics(const InputMap& map)
    {
        InputDiagnostics diagnostics{};
        diagnostics.bindingCount = map.Bindings().size();

        std::unordered_set<std::uint64_t> seen;
        for (const InputBinding& binding : map.Bindings())
        {
            if (binding.editorContext)
            {
                ++diagnostics.editorBindingCount;
                const std::uint64_t key = ConflictKey(binding, InputContext::Editor);
                if (!seen.insert(key).second)
                {
                    ++diagnostics.conflictCount;
                }
            }
            if (binding.textEntryContext)
            {
                ++diagnostics.textEntryBindingCount;
                const std::uint64_t key = ConflictKey(binding, InputContext::TextEntry);
                if (!seen.insert(key).second)
                {
                    ++diagnostics.conflictCount;
                }
            }
        }

        for (std::size_t index = 0; index < static_cast<std::size_t>(InputAction::Count); ++index)
        {
            if (!ActionBoundInAnyContext(map, static_cast<InputAction>(index)))
            {
                ++diagnostics.unboundActionCount;
            }
        }

        std::ostringstream out;
        out << "Input map: bindings=" << diagnostics.bindingCount
            << " editor=" << diagnostics.editorBindingCount
            << " text=" << diagnostics.textEntryBindingCount
            << " conflicts=" << diagnostics.conflictCount
            << " unbound=" << diagnostics.unboundActionCount;
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string BuildInputProbeSummary()
    {
        const InputMap map = BuildDefaultEditorInputMap();
        return BuildInputDiagnostics(map).summary;
    }

    const char* ToString(InputContext context)
    {
        switch (context)
        {
            case InputContext::Editor:
                return "Editor";
            case InputContext::TextEntry:
                return "TextEntry";
            default:
                return "Unknown";
        }
    }

    const char* ToString(InputSlot slot)
    {
        switch (slot)
        {
            case InputSlot::KeyUp:
                return "Up";
            case InputSlot::KeyDown:
                return "Down";
            case InputSlot::KeyLeft:
                return "Left";
            case InputSlot::KeyRight:
                return "Right";
            case InputSlot::KeyDelete:
                return "Delete";
            case InputSlot::KeyEscape:
                return "Esc";
            case InputSlot::KeyEnter:
                return "Enter";
            case InputSlot::KeyBackspace:
                return "Backspace";
            case InputSlot::KeyHome:
                return "Home";
            case InputSlot::KeyF2:
                return "F2";
            case InputSlot::KeyF:
                return "F";
            case InputSlot::KeyG:
                return "G";
            case InputSlot::KeyCtrlD:
                return "Ctrl+D";
            case InputSlot::KeyCtrlZ:
                return "Ctrl+Z";
            case InputSlot::KeyCtrlY:
                return "Ctrl+Y";
            case InputSlot::KeyN:
                return "N";
            case InputSlot::KeyS:
                return "S";
            case InputSlot::KeyL:
                return "L";
            case InputSlot::KeyR:
                return "R";
            case InputSlot::KeyW:
                return "W";
            case InputSlot::KeyA:
                return "A";
            case InputSlot::KeyD:
                return "D";
            case InputSlot::KeyQ:
                return "Q";
            case InputSlot::KeyE:
                return "E";
            case InputSlot::KeyI:
                return "I";
            case InputSlot::KeyJ:
                return "J";
            case InputSlot::KeyK:
                return "K";
            case InputSlot::KeyMinus:
                return "-";
            case InputSlot::KeyEquals:
                return "+";
            default:
                return "Unknown";
        }
    }

    const char* ToString(InputAction action)
    {
        switch (action)
        {
            case InputAction::Cancel:
                return "Cancel";
            case InputAction::Confirm:
                return "Confirm";
            case InputAction::RenameBackspace:
                return "RenameBackspace";
            case InputAction::NewEntity:
                return "NewEntity";
            case InputAction::DeleteSelection:
                return "DeleteSelection";
            case InputAction::SaveScene:
                return "SaveScene";
            case InputAction::LoadScene:
                return "LoadScene";
            case InputAction::RescanAssets:
                return "RescanAssets";
            case InputAction::Undo:
                return "Undo";
            case InputAction::Redo:
                return "Redo";
            case InputAction::DuplicateSelection:
                return "DuplicateSelection";
            case InputAction::FocusSelection:
                return "FocusSelection";
            case InputAction::ResetViewport:
                return "ResetViewport";
            case InputAction::ToggleGridSnap:
                return "ToggleGridSnap";
            case InputAction::SelectPrevious:
                return "SelectPrevious";
            case InputAction::SelectNext:
                return "SelectNext";
            case InputAction::RenameSelection:
                return "RenameSelection";
            case InputAction::MoveForward:
                return "MoveForward";
            case InputAction::MoveBackward:
                return "MoveBackward";
            case InputAction::MoveLeft:
                return "MoveLeft";
            case InputAction::MoveRight:
                return "MoveRight";
            case InputAction::MoveDown:
                return "MoveDown";
            case InputAction::MoveUp:
                return "MoveUp";
            case InputAction::RotateYawLeft:
                return "RotateYawLeft";
            case InputAction::RotateYawRight:
                return "RotateYawRight";
            case InputAction::RotatePitchUp:
                return "RotatePitchUp";
            case InputAction::RotatePitchDown:
                return "RotatePitchDown";
            case InputAction::RotateRollLeft:
                return "RotateRollLeft";
            case InputAction::ScaleUp:
                return "ScaleUp";
            case InputAction::ScaleDown:
                return "ScaleDown";
            default:
                return "Unknown";
        }
    }

    const char* ToString(InputTrigger trigger)
    {
        switch (trigger)
        {
            case InputTrigger::Pressed:
                return "Pressed";
            case InputTrigger::Active:
                return "Active";
            default:
                return "Unknown";
        }
    }

    const char* ToString(ModifierRule rule)
    {
        switch (rule)
        {
            case ModifierRule::Any:
                return "Any";
            case ModifierRule::Required:
                return "Required";
            case ModifierRule::Forbidden:
                return "Forbidden";
            default:
                return "Unknown";
        }
    }

    std::string FormatBinding(const InputBinding& binding)
    {
        std::ostringstream out;
        if (binding.ctrl == ModifierRule::Required)
        {
            out << "Ctrl+";
        }
        if (binding.shift == ModifierRule::Required)
        {
            out << "Shift+";
        }
        out << (binding.displayName && binding.displayName[0] != '\0' ? binding.displayName : ToString(binding.slot));
        out << " -> " << ToString(binding.action);
        return out.str();
    }

    std::string FormatPressedActions(const InputActionSnapshot& snapshot)
    {
        if (snapshot.pressedActions.empty() && snapshot.activeActions.empty())
        {
            return "Input actions: none";
        }

        std::ostringstream out;
        out << "Input actions:";
        bool first = true;
        for (InputAction action : snapshot.pressedActions)
        {
            out << (first ? " " : ", ") << ToString(action);
            first = false;
        }
        for (InputAction action : snapshot.activeActions)
        {
            if (std::find(snapshot.pressedActions.begin(), snapshot.pressedActions.end(), action) != snapshot.pressedActions.end())
            {
                continue;
            }
            out << (first ? " " : ", ") << ToString(action);
            first = false;
        }
        return out.str();
    }
}
