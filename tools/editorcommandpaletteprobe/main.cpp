#include <AK/EditorUI/EditorCommandPaletteProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorCommandPaletteDiagnostics diagnostics = AK::RunEditorCommandPaletteProbe();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor command palette / global search runtime foundation\n";
        std::cerr << diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] editor command palette / global search runtime foundation rows=" << diagnostics.rowCount
              << " commands=" << diagnostics.commandRowCount
              << " disabled=" << diagnostics.disabledRowCount
              << " shortcuts=" << diagnostics.shortcutTextCount << "\n";
    std::cout << diagnostics.summary << "\n";
    return 0;
}
