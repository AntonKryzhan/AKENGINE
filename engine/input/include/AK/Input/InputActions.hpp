#pragma once

#include <AK/Platform/Window.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class InputContext
    {
        Editor,
        TextEntry
    };

    enum class InputSlot
    {
        KeyUp,
        KeyDown,
        KeyLeft,
        KeyRight,
        KeyDelete,
        KeyEscape,
        KeyEnter,
        KeyBackspace,
        KeyHome,
        KeyF2,
        KeyF,
        KeyG,
        KeyCtrlD,
        KeyCtrlZ,
        KeyCtrlY,
        KeyN,
        KeyS,
        KeyL,
        KeyR,
        KeyW,
        KeyA,
        KeyD,
        KeyQ,
        KeyE,
        KeyI,
        KeyJ,
        KeyK,
        KeyMinus,
        KeyEquals,
        Count
    };

    enum class InputAction
    {
        Cancel,
        Confirm,
        RenameBackspace,
        NewEntity,
        DeleteSelection,
        SaveScene,
        LoadScene,
        RescanAssets,
        Undo,
        Redo,
        DuplicateSelection,
        FocusSelection,
        ResetViewport,
        ToggleGridSnap,
        SelectPrevious,
        SelectNext,
        RenameSelection,
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveDown,
        MoveUp,
        RotateYawLeft,
        RotateYawRight,
        RotatePitchUp,
        RotatePitchDown,
        RotateRollLeft,
        ScaleUp,
        ScaleDown,
        Count
    };

    enum class InputTrigger
    {
        Pressed,
        Active
    };

    enum class ModifierRule
    {
        Any,
        Required,
        Forbidden
    };

    struct InputBinding
    {
        InputAction action = InputAction::Cancel;
        InputSlot slot = InputSlot::KeyEscape;
        InputTrigger trigger = InputTrigger::Pressed;
        ModifierRule ctrl = ModifierRule::Any;
        ModifierRule shift = ModifierRule::Any;
        bool editorContext = true;
        bool textEntryContext = false;
        const char* displayName = "";
    };

    struct InputActionState
    {
        bool pressed = false;
        bool active = false;
    };

    struct InputActionSnapshot
    {
        std::array<InputActionState, static_cast<std::size_t>(InputAction::Count)> states{};
        bool ctrlDown = false;
        bool shiftDown = false;
        std::vector<InputAction> pressedActions;
        std::vector<InputAction> activeActions;

        bool Pressed(InputAction action) const;
        bool Active(InputAction action) const;
    };

    struct InputDiagnostics
    {
        std::size_t bindingCount = 0;
        std::size_t editorBindingCount = 0;
        std::size_t textEntryBindingCount = 0;
        std::size_t conflictCount = 0;
        std::size_t unboundActionCount = 0;
        std::string summary;
    };

    class InputMap final
    {
    public:
        void AddBinding(const InputBinding& binding);
        InputActionSnapshot Evaluate(const WindowInput& input, InputContext context) const;
        const std::vector<InputBinding>& Bindings() const;

    private:
        std::vector<InputBinding> mBindings;
    };

    InputMap BuildDefaultEditorInputMap();
    InputDiagnostics BuildInputDiagnostics(const InputMap& map);
    std::string BuildInputProbeSummary();

    const char* ToString(InputContext context);
    const char* ToString(InputSlot slot);
    const char* ToString(InputAction action);
    const char* ToString(InputTrigger trigger);
    const char* ToString(ModifierRule rule);

    std::string FormatBinding(const InputBinding& binding);
    std::string FormatPressedActions(const InputActionSnapshot& snapshot);
}
