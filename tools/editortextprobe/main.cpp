#include <AK/EditorUI/EditorText.hpp>

#include <iostream>

int main()
{
    const AK::EditorTextDiagnostics diagnostics = AK::RunEditorTextDiagnostics();
    if (!diagnostics.ok)
    {
        std::cerr << "[fail] editor text / unicode / dpi foundation\n" << diagnostics.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor text / unicode / dpi foundation codepoints=" << diagnostics.codepointCount
              << " row144dpi=" << diagnostics.scaledRowHeight << '\n';
    std::cout << diagnostics.summary << '\n';
    return 0;
}
