#include <AK/EditorUI/EditorSessionStateProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorSessionStateProbeResult result = AK::BuildEditorSessionStateProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor session state persistence / recovery foundation\n" << result.summary << '\n' << result.diagnostics.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor session state persistence / recovery foundation panels=" << result.repairedLoad.session.panels.size()
              << " selection=" << result.repairedLoad.session.selection.size()
              << " issues=" << result.repairedLoad.repair.issues.size()
              << " transient=" << result.transientDropped
              << " activeTabs=" << result.activeTabsRestored << '\n';
    std::cout << result.summary << '\n';
    std::cout << result.diagnostics.summary << '\n';
    return 0;
}
