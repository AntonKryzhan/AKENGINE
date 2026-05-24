#include <AK/EditorUI/EditorCommandPaletteRuntime.hpp>

#include <iostream>

int main()
{
    const AK::EditorCommandPaletteRuntimeDiagnostics diagnostics = AK::RunEditorCommandPaletteRuntimeDiagnostics();
    std::cout << (diagnostics.ok ? "[ ok ] " : "[fail] ")
              << "editor command palette runtime / overlay interaction foundation"
              << " rows=" << diagnostics.rowCount
              << " open=" << diagnostics.openOk
              << " text=" << diagnostics.textOk
              << " hit=" << diagnostics.hitOk
              << " accept=" << diagnostics.acceptOk
              << " capture=" << diagnostics.captureOk
              << '\n';
    std::cout << diagnostics.summary << '\n';
    std::cout << AK::FormatEditorCommandPaletteHitTestResult(diagnostics.rowHit) << '\n';
    return diagnostics.ok ? 0 : 1;
}
