#include <AK/EditorUI/EditorDiagnosticsInteraction.hpp>

#include <iostream>

int main()
{
    const AK::EditorDiagnosticsInteractionDiagnostics diagnostics = AK::RunEditorDiagnosticsInteractionDiagnostics();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor diagnostics interaction / filtering runtime foundation\n";
        std::cerr << diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] editor diagnostics interaction / filtering runtime foundation rows=" << diagnostics.panel.rows.size()
              << " search=" << diagnostics.state.searchQuery
              << " read=" << diagnostics.markReadResult.notificationsMarkedRead
              << " clearTasks=" << diagnostics.clearTasksResult.completedTasksCleared << "\n";
    std::cout << diagnostics.summary << "\n";
    return 0;
}
