#include <AK/EditorUI/EditorDragDropProbe.hpp>
#include <AK/EditorUI/EditorDragDrop.hpp>

#include <iostream>

int main()
{
    const AK::EditorDragDropProbeResult result = AK::RunEditorDragDropProbe();
    const AK::EditorDragDropDiagnostics diagnostics = AK::RunEditorDragDropDiagnostics();
    if (!result.ok)
    {
        std::cerr << "[fail] " << result.summary << "\n";
        std::cerr << diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] " << result.summary << "\n";
    std::cout << diagnostics.summary << "\n";
    return 0;
}
