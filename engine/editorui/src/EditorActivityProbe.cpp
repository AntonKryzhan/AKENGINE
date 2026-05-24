#include <AK/EditorUI/EditorActivityProbe.hpp>

#include <sstream>

namespace AK
{
    EditorActivityProbeResult BuildEditorActivityProbe()
    {
        EditorActivityProbeResult result{};
        result.diagnostics = RunEditorActivityDiagnostics();
        result.dedupeMerged = result.diagnostics.dedupeMerged;
        result.warningTracked = result.diagnostics.warningTracked;
        result.errorTracked = result.diagnostics.errorTracked;
        result.taskCompleted = result.diagnostics.taskCompleted;
        result.capacityPruned = result.diagnostics.capacityPruned;
        result.actionCommandBacked = result.diagnostics.actionCommandBacked;
        result.ok = result.diagnostics.ok;

        std::ostringstream out;
        out << "editor-activity-probe dedupe=" << result.dedupeMerged
            << " warnings=" << result.warningTracked
            << " errors=" << result.errorTracked
            << " taskCompleted=" << result.taskCompleted
            << " capacity=" << result.capacityPruned
            << " actionCommand=" << result.actionCommandBacked
            << " ok=" << result.ok;
        result.summary = out.str();
        return result;
    }

    std::string BuildEditorActivityProbeSummary()
    {
        return BuildEditorActivityProbe().summary;
    }
}
