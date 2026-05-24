#include <AK/EditorUI/EditorDiagnosticsView.hpp>

#include <iostream>

int main()
{
    const AK::EditorDiagnosticsViewDiagnostics diagnostics = AK::RunEditorDiagnosticsViewDiagnostics();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor diagnostics view / visual panel foundation\n";
        std::cerr << diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] editor diagnostics view / visual panel foundation rows=" << diagnostics.view.rows.size()
              << " visible=" << diagnostics.view.visibleRowCount
              << " metrics=" << diagnostics.view.metrics.size()
              << " badges=" << diagnostics.view.badges.size()
              << " first=" << diagnostics.view.firstVisibleRow << "\n";
    std::cout << diagnostics.summary << "\n";
    return 0;
}
