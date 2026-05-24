#pragma once

#include <string>

namespace AK
{
    struct EditorPropertyEditingProbeResult
    {
        bool ok = false;
        std::string summary;
    };

    EditorPropertyEditingProbeResult RunEditorPropertyEditingProbe();
    std::string BuildEditorPropertyEditingProbeSummary();
}
