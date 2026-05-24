#include <AK/EditorUI/EditorCommandStateProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorCommandStateProbeResult result = AK::RunEditorCommandStateProbe();
    std::cout << result.summary << std::endl;
    return result.ok ? 0 : 1;
}
