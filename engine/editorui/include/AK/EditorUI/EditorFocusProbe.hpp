#pragma once

#include <string>

namespace AK
{
    struct EditorFocusProbeResult
    {
        bool ok = false;
        std::string summary;
    };

    EditorFocusProbeResult RunEditorFocusProbe();
    std::string BuildEditorFocusProbeSummary();
}
