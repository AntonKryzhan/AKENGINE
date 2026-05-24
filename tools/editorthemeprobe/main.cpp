#include <AK/EditorUI/EditorThemeProbe.hpp>

#include <iostream>

int main()
{
    const std::string report = AK::RunEditorThemeProbe();
    std::cout << report << '\n';
    return report.rfind("[ ok ]", 0) == 0 ? 0 : 1;
}
