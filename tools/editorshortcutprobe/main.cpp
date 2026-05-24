#include <AK/EditorUI/EditorShortcutProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorShortcutProbeResult result = AK::BuildEditorShortcutProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor shortcut manager / keymap preference foundation\n" << result.summary << '\n' << result.diagnostics.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor shortcut manager / keymap preference foundation defaults=" << result.defaultBindingCount
              << " conflicts=" << result.conflictCount
              << " repairedIssues=" << result.repairedLoad.repair.issues.size()
              << " inputMap=" << result.inputMapBuilt << '\n';
    std::cout << result.summary << '\n';
    std::cout << result.diagnostics.summary << '\n';
    return 0;
}
