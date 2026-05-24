#include <AK/EditorUI/EditorFocusProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorFocusProbeResult result = AK::RunEditorFocusProbe();
    std::cout << result.summary << std::endl;
    return result.ok ? 0 : 1;
}
