#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Log.hpp>

#include <iostream>

int main()
{
    AK::LogInfo("AK command probe starting");

    const AK::InputMap inputMap = AK::BuildDefaultEditorInputMap();
    const AK::CommandRegistry registry = AK::BuildDefaultEditorCommandRegistry();
    const AK::CommandDiagnostics diagnostics = AK::BuildCommandDiagnostics(registry, inputMap);

    std::cout << diagnostics.summary << '\n';
    std::cout << "Commands:" << '\n';
    for (const AK::CommandDescriptor& command : registry.Commands())
    {
        std::cout << "  " << AK::FormatCommand(command) << '\n';
    }

    AK::WindowInput sample{};
    sample.keyCtrlDown = true;
    sample.keySPressed = true;
    const AK::InputActionSnapshot inputSnapshot = inputMap.Evaluate(sample, AK::InputContext::Editor);
    const std::vector<AK::CommandInvocation> invocations = AK::CollectCommandInvocations(registry, inputSnapshot);
    for (const AK::CommandInvocation& invocation : invocations)
    {
        std::cout << "  invoke " << AK::FormatCommandInvocation(registry, invocation) << '\n';
    }

    const bool ok = diagnostics.duplicateIdCount == 0
        && diagnostics.duplicateNameCount == 0
        && diagnostics.missingShortcutBindingCount == 0
        && diagnostics.missingRequiredCommandCount == 0
        && !invocations.empty()
        && invocations.front().id == AK::CommandId::SaveScene;

    if (!ok)
    {
        AK::LogError("AK command probe failed");
        return 1;
    }

    AK::LogInfo("AK command probe finished");
    return 0;
}
