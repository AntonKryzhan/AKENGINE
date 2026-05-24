#pragma once

#include <AK/EditorUI/EditorActivity.hpp>

namespace AK
{
    struct EditorActivityProbeResult
    {
        bool ok = false;
        bool dedupeMerged = false;
        bool warningTracked = false;
        bool errorTracked = false;
        bool taskCompleted = false;
        bool capacityPruned = false;
        bool actionCommandBacked = false;
        EditorActivityDiagnostics diagnostics{};
        std::string summary;
    };

    EditorActivityProbeResult BuildEditorActivityProbe();
    std::string BuildEditorActivityProbeSummary();
}
