#pragma once

#include <string>

namespace AK
{
    struct EditorCommandStateProbeResult
    {
        bool ok = false;
        std::string summary;
    };

    EditorCommandStateProbeResult RunEditorCommandStateProbe();
    std::string BuildEditorCommandStateProbeSummary();
}
