#include <AK/EditorUI/EditorUXRuntimeProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorUXRuntimeProbeResult result = AK::RunEditorUXRuntimeProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor ux runtime / popup surface workflow\n" << result.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] editor ux runtime / popup surface workflow foundation\n";
    std::cout << result.summary << "\n";
    return 0;
}
