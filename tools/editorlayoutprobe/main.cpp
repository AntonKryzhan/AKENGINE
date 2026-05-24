#include <AK/EditorUI/EditorLayoutPersistenceProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorLayoutPersistenceProbeResult result = AK::BuildEditorLayoutPersistenceProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor layout persistence / repair foundation\n" << result.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor layout persistence / repair foundation issues=" << result.issueCount
              << " clean=" << result.cleanLoad.ok
              << " legacy=" << result.legacyLoad.ok
              << " window=" << result.windowClamped
              << " reset=" << result.resetFallback << '\n';
    std::cout << result.summary << '\n';
    return 0;
}
