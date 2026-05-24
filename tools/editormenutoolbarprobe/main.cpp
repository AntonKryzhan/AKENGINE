#include <AK/EditorUI/EditorMenuToolbarRuntimeProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorMenuToolbarRuntimeDiagnostics diagnostics = AK::RunEditorMenuToolbarRuntimeProbe();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor menu / toolbar runtime integration foundation\n";
        std::cerr << diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] editor menu / toolbar runtime integration foundation roots=" << diagnostics.rootCount
              << " buttons=" << diagnostics.toolbarButtonCount
              << " popupItems=" << diagnostics.popupItemCount
              << " shortcuts=" << diagnostics.shortcutTextCount << "\n";
    std::cout << diagnostics.summary << "\n";
    return 0;
}
