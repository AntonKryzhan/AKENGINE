#pragma once

#include <AK/EditorUI/EditorSessionState.hpp>

namespace AK
{
    struct EditorSessionStateProbeResult
    {
        EditorSessionDiagnostics diagnostics{};
        EditorSessionLoadResult cleanLoad{};
        EditorSessionLoadResult repairedLoad{};
        bool unknownPanelDropped = false;
        bool unknownSelectionDropped = false;
        bool duplicateSelectionDropped = false;
        bool scrollClamped = false;
        bool transientDropped = false;
        bool activeTabsRestored = false;
        bool ok = false;
        std::string summary;
    };

    EditorSessionStateProbeResult BuildEditorSessionStateProbe();
}
