#pragma once

#include <string>

namespace AK
{
    struct EditorDragDropProbeResult
    {
        bool ok = false;
        std::string summary;
    };

    EditorDragDropProbeResult RunEditorDragDropProbe();
    std::string BuildEditorDragDropProbeSummary();
}
