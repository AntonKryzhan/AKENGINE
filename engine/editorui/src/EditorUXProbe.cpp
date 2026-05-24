#include <AK/EditorUI/EditorUXProbe.hpp>

#include <sstream>

namespace AK
{
    EditorUXProbeResult BuildEditorUXProbe()
    {
        EditorUXProbeResult result{};
        result.frame = BuildDefaultEditorUXFrame(1600, 900);
        result.diagnostics = ValidateEditorUXFrame(result.frame);
        if (!result.frame.workflows.empty())
        {
            result.firstWorkflow = FormatEditorPopupWorkflow(result.frame.workflows.front());
        }
        result.ok = result.diagnostics.ok && !result.firstWorkflow.empty();

        std::ostringstream out;
        out << "editor ux polish / popup context workflow foundation"
            << " workflows=" << result.diagnostics.workflowCount
            << " menuItems=" << result.diagnostics.menuItemCount
            << " popupRows=" << result.diagnostics.popupRowCount;
        result.summary = out.str();
        return result;
    }
}
