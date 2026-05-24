#pragma once

#include <AK/EditorUI/EditorRuntimeBridge.hpp>

#include <string>

namespace AK
{
    struct EditorRuntimeProbeResult
    {
        EditorRuntimeBridge bridge{};
        EditorRuntimeDiagnostics diagnostics{};
        EditorHitTestResult toolbarHit{};
        EditorHitTestResult sceneHit{};
        EditorHitTestResult overlayHit{};
        std::vector<CommandInvocation> pointerCommands;
        std::vector<EditorSearchResult> searchResults;
        std::string serializedLayout;
        bool deserializedOk = false;
        bool ok = false;
        std::string summary;
    };

    EditorRuntimeProbeResult BuildEditorRuntimeProbe();
}
