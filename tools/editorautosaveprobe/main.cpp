#include <AK/EditorUI/EditorAutosaveProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorAutosaveProbeResult result = AK::BuildEditorAutosaveProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor autosave / backup runtime foundation\n"
                  << result.summary << '\n'
                  << result.diagnostics.summary << '\n'
                  << result.diagnostics.firstTick.summary << '\n'
                  << result.diagnostics.secondTick.summary << '\n'
                  << result.diagnostics.rotation.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor autosave / backup runtime foundation triggered=" << result.triggered
              << " scene=" << result.sceneAutosave
              << " workspace=" << result.workspaceSave
              << " backup=" << result.backupCreated
              << " rotation=" << result.rotationPrunesOldest << '\n';
    std::cout << result.summary << '\n';
    std::cout << result.diagnostics.summary << '\n';
    std::cout << result.diagnostics.firstTick.summary << '\n';
    std::cout << result.diagnostics.secondTick.summary << '\n';
    std::cout << result.diagnostics.rotation.summary << '\n';
    return 0;
}
