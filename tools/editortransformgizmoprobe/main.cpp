#include <AK/EditorUI/EditorTransformGizmoProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorTransformGizmoDiagnostics diagnostics = AK::RunEditorTransformGizmoProbe();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor transform gizmo foundation\n";
        std::cerr << diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] editor transform gizmo foundation handles=" << diagnostics.handleCount
              << " visible=" << diagnostics.visibleHandleCount << "\n";
    std::cout << diagnostics.summary << "\n";
    return 0;
}
