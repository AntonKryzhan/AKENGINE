#pragma once

#include <AK/EditorUI/EditorUX.hpp>

#include <string>

namespace AK
{
    struct EditorUXProbeResult
    {
        EditorUXFrame frame{};
        EditorUXDiagnostics diagnostics{};
        std::string firstWorkflow;
        std::string summary;
        bool ok = false;
    };

    EditorUXProbeResult BuildEditorUXProbe();
}
