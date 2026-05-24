#include <AK/EditorUI/EditorPropertyEditingProbe.hpp>

#include <AK/EditorUI/EditorPropertyEditing.hpp>

#include <sstream>

namespace AK
{
    EditorPropertyEditingProbeResult RunEditorPropertyEditingProbe()
    {
        const EditorPropertyEditingDiagnostics diagnostics = RunEditorPropertyEditingDiagnostics();

        EditorPropertyEditingProbeResult result{};
        result.ok = diagnostics.ok;

        std::ostringstream summary;
        summary << "[ ok ] editor inspector property editing / undo transaction foundation"
                << " undo=" << diagnostics.undoTransactionCount
                << " changes=" << diagnostics.committedChangeCount
                << " dirty=" << diagnostics.dirtyPropertyCount
                << " previewNoUndo=" << diagnostics.previewDoesNotCreateUndo
                << " cancelRestore=" << diagnostics.cancelRestoresValue
                << " validation=" << diagnostics.validationOk
                << " clamp=" << diagnostics.rangeClampOk
                << " proxy=" << diagnostics.proxyRebuildOk
                << '\n'
                << diagnostics.summary;
        result.summary = summary.str();
        return result;
    }

    std::string BuildEditorPropertyEditingProbeSummary()
    {
        return RunEditorPropertyEditingProbe().summary;
    }
}
