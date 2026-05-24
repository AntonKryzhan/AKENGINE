#include <AK/EditorUI/EditorSceneViewProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorSceneViewDiagnostics diagnostics = AK::RunEditorSceneViewProbe();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor scene view foundation\n";
        std::cerr << diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] editor scene view foundation\n";
    std::cout << diagnostics.summary << "\n";
    return 0;
}
