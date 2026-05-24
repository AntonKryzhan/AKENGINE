#include <AK/EditorUI/EditorWidgetProbe.hpp>

#include <sstream>

namespace AK
{
    EditorWidgetProbeResult BuildEditorWidgetProbe()
    {
        EditorWidgetProbeResult result{};
        result.frame = BuildDefaultEditorWidgetFrame(1600, 900);
        result.diagnostics = ValidateEditorWidgetFrame(result.frame);

        if (!result.frame.hierarchyWidgets.empty())
        {
            result.firstHierarchyWidget = FormatEditorWidget(result.frame.hierarchyWidgets.front());
        }
        if (!result.frame.inspectorWidgets.empty())
        {
            result.firstInspectorWidget = FormatEditorWidget(result.frame.inspectorWidgets.front());
        }
        if (!result.frame.projectWidgets.empty())
        {
            result.firstProjectWidget = FormatEditorWidget(result.frame.projectWidgets.front());
        }

        result.ok = result.diagnostics.ok
            && result.frame.contextMenus.size() == 1
            && result.frame.popups.size() == 1
            && !result.firstHierarchyWidget.empty()
            && !result.firstInspectorWidget.empty()
            && !result.firstProjectWidget.empty();

        std::ostringstream out;
        out << "editor widget kit / unity-like panel content foundation"
            << " hierarchy=" << result.diagnostics.hierarchyWidgetCount
            << " inspector=" << result.diagnostics.inspectorWidgetCount
            << " project=" << result.diagnostics.projectWidgetCount
            << " sceneToolbar=" << result.diagnostics.sceneToolbarWidgetCount
            << " contextMenus=" << result.diagnostics.contextMenuCount
            << " popups=" << result.diagnostics.popupCount;
        result.summary = out.str();
        return result;
    }
}
