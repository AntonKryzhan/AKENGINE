#pragma once

#include <AK/EditorUI/EditorWidgets.hpp>

#include <string>

namespace AK
{
    struct EditorWidgetProbeResult
    {
        EditorWidgetFrame frame{};
        EditorWidgetDiagnostics diagnostics{};
        std::string firstHierarchyWidget;
        std::string firstInspectorWidget;
        std::string firstProjectWidget;
        bool ok = false;
        std::string summary;
    };

    EditorWidgetProbeResult BuildEditorWidgetProbe();
}
