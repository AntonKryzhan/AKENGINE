#pragma once

#include <string>

namespace AK
{
    struct EditorUXRuntimeProbeResult
    {
        bool ok = false;
        std::string summary;
    };

    EditorUXRuntimeProbeResult RunEditorUXRuntimeProbe();
    std::string BuildEditorUXRuntimeProbeSummary();
}
