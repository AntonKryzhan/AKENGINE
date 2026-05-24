#include <AK/EditorUI/EditorCommandStateProbe.hpp>

#include <AK/EditorUI/EditorCommandState.hpp>

#include <sstream>

namespace AK
{
    EditorCommandStateProbeResult RunEditorCommandStateProbe()
    {
        const EditorCommandStateDiagnostics diagnostics = RunEditorCommandStateDiagnostics();

        EditorCommandStateProbeResult result{};
        result.ok = diagnostics.ok;

        std::ostringstream summary;
        summary << "[ ok ] editor command state / tooltip / modal foundation"
                << " commands=" << diagnostics.commandCount
                << " disabled=" << diagnostics.disabledCount
                << " checked=" << diagnostics.checkedCount
                << " modals=" << diagnostics.modalCount
                << " tooltip=" << diagnostics.tooltipTimingOk
                << '\n'
                << diagnostics.summary;
        result.summary = summary.str();
        return result;
    }

    std::string BuildEditorCommandStateProbeSummary()
    {
        return RunEditorCommandStateProbe().summary;
    }
}
