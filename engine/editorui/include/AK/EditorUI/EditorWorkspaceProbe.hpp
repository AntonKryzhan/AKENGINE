#pragma once

#include <AK/EditorUI/EditorWorkspace.hpp>

namespace AK
{
    struct EditorWorkspaceProbeResult
    {
        EditorWorkspaceLoadResult firstLoad{};
        EditorWorkspaceSaveResult save{};
        EditorWorkspaceLoadResult secondLoad{};
        EditorWorkspaceLoadResult repairedLoad{};
        EditorWorkspaceDiagnostics diagnostics{};
        usize issueCount = 0;
        bool directoriesCreated = false;
        bool defaultsUsed = false;
        bool filesWritten = false;
        bool reloadOk = false;
        bool repairOk = false;
        bool ok = false;
        std::string summary;
    };

    EditorWorkspaceProbeResult BuildEditorWorkspaceProbe();
}
