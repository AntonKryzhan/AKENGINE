#include <AK/EditorUI/EditorPropertyEditingProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorPropertyEditingProbeResult result = AK::RunEditorPropertyEditingProbe();
    std::cout << result.summary << std::endl;
    return result.ok ? 0 : 1;
}
