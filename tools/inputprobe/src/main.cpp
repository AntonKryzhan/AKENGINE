#include <AK/Core/Log.hpp>
#include <AK/Input/InputActions.hpp>

#include <iostream>

int main()
{
    AK::LogInfo("AK input probe starting");

    const AK::InputMap map = AK::BuildDefaultEditorInputMap();
    const AK::InputDiagnostics diagnostics = AK::BuildInputDiagnostics(map);

    std::cout << diagnostics.summary << '\n';
    std::cout << "Bindings:" << '\n';
    for (const AK::InputBinding& binding : map.Bindings())
    {
        std::cout << "  " << AK::FormatBinding(binding)
            << "  trigger=" << AK::ToString(binding.trigger)
            << "  editor=" << (binding.editorContext ? "yes" : "no")
            << "  text=" << (binding.textEntryContext ? "yes" : "no")
            << "  ctrl=" << AK::ToString(binding.ctrl)
            << '\n';
    }

    AK::WindowInput sample{};
    sample.keyCtrlDown = true;
    sample.keySPressed = true;
    const AK::InputActionSnapshot editorSnapshot = map.Evaluate(sample, AK::InputContext::Editor);
    std::cout << AK::FormatPressedActions(editorSnapshot) << '\n';

    sample = {};
    sample.keyEscapePressed = true;
    const AK::InputActionSnapshot textSnapshot = map.Evaluate(sample, AK::InputContext::TextEntry);
    std::cout << AK::FormatPressedActions(textSnapshot) << '\n';

    const bool ok = diagnostics.conflictCount == 0
        && editorSnapshot.Pressed(AK::InputAction::SaveScene)
        && textSnapshot.Pressed(AK::InputAction::Cancel);

    if (!ok)
    {
        AK::LogError("AK input probe failed");
        return 1;
    }

    AK::LogInfo("AK input probe finished");
    return 0;
}
