#include <AK/EditorUI/EditorWidgetProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorWidgetProbeResult result = AK::BuildEditorWidgetProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] " << result.summary << "\n";
        std::cerr << result.diagnostics.summary << "\n";
        return 1;
    }

    std::cout << "[ ok ] " << result.summary << "\n";
    std::cout << result.diagnostics.summary << "\n";
    return 0;
}
