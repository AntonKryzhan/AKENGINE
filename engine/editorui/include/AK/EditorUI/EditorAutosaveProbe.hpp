#pragma once

#include <AK/EditorUI/EditorAutosave.hpp>

namespace AK
{
    struct EditorAutosaveProbeResult
    {
        EditorAutosaveDiagnostics diagnostics{};
        EditorAutosaveRuntimeState state{};
        bool firstSkipped = false;
        bool triggered = false;
        bool sceneAutosave = false;
        bool workspaceSave = false;
        bool backupCreated = false;
        bool rotationPrunesOldest = false;
        bool notifyOk = false;
        bool ok = false;
        std::string summary;
    };

    EditorAutosaveProbeResult BuildEditorAutosaveProbe();
    std::string BuildEditorAutosaveProbeSummary();
}
