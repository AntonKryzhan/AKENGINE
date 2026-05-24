#include <AK/EditorUI/EditorUXProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorUXProbeResult result = AK::BuildEditorUXProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] " << result.summary << "\n";
        std::cerr << result.diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] " << result.summary << "\n";
    std::cout << result.diagnostics.summary << "\n";
    std::cout << result.firstWorkflow << "\n";
    return 0;
}
