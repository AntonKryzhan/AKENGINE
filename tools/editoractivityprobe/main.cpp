#include <AK/EditorUI/EditorActivityProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorActivityProbeResult result = AK::BuildEditorActivityProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor activity / notification task center foundation\n"
                  << result.summary << '\n'
                  << result.diagnostics.summary << '\n'
                  << result.diagnostics.tick.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor activity / notification task center foundation dedupe=" << result.dedupeMerged
              << " warnings=" << result.warningTracked
              << " errors=" << result.errorTracked
              << " task=" << result.taskCompleted
              << " capacity=" << result.capacityPruned << '\n';
    std::cout << result.summary << '\n';
    std::cout << result.diagnostics.summary << '\n';
    std::cout << result.diagnostics.tick.summary << '\n';
    return 0;
}
