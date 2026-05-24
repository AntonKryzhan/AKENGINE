#include <AK/EditorUI/EditorPreferencesProbe.hpp>

#include <iostream>

int main()
{
    const AK::EditorPreferencesProbeResult result = AK::BuildEditorPreferencesProbe();
    if (!result.ok)
    {
        std::cerr << "[fail] editor preferences / startup profile foundation\n" << result.summary << '\n' << result.diagnostics.summary << '\n';
        return 1;
    }

    std::cout << "[ ok ] editor preferences / startup profile foundation issues=" << result.repairedIssues
              << " recent=" << result.cleanRecentProjects
              << " startup=" << result.startupPlanBuilt << '\n';
    std::cout << result.summary << '\n';
    std::cout << result.startupPlan.summary << '\n';
    std::cout << result.diagnostics.summary << '\n';
    return 0;
}
