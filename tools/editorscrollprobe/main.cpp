#include <AK/EditorUI/EditorScrollClip.hpp>

#include <iostream>

int main()
{
    const AK::EditorScrollDiagnostics diagnostics = AK::RunEditorScrollDiagnostics();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor scroll / clip / virtualized lists foundation\n" << diagnostics.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor scroll / clip / virtualized lists foundation list=" << diagnostics.visibleListItems
              << " grid=" << diagnostics.visibleGridItems
              << " clamp=" << diagnostics.clampedOffset << '\n';
    std::cout << diagnostics.summary << '\n';
    return 0;
}
