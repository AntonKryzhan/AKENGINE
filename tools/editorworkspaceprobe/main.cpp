#include <AK/EditorUI/EditorWorkspaceProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorWorkspaceProbeResult result = AK::BuildEditorWorkspaceProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor workspace persistence / autosave runtime foundation\n"
                  << result.summary << '\n'
                  << result.firstLoad.summary << '\n'
                  << result.save.summary << '\n'
                  << result.secondLoad.summary << '\n'
                  << result.repairedLoad.summary << '\n'
                  << result.diagnostics.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor workspace persistence / autosave runtime foundation defaults=" << result.defaultsUsed
              << " files=" << result.save.filesWritten
              << " reload=" << result.reloadOk
              << " repaired=" << result.repairOk
              << " issues=" << result.issueCount << '\n';
    std::cout << result.summary << '\n';
    std::cout << result.firstLoad.summary << '\n';
    std::cout << result.save.summary << '\n';
    std::cout << result.secondLoad.summary << '\n';
    std::cout << result.repairedLoad.summary << '\n';
    std::cout << result.diagnostics.summary << '\n';
    return 0;
}
