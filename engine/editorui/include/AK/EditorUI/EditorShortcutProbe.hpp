#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorShortcuts.hpp>

namespace AK
{
    struct EditorShortcutProbeResult
    {
        EditorShortcutLoadResult cleanLoad{};
        EditorShortcutLoadResult repairedLoad{};
        EditorShortcutDiagnostics diagnostics{};
        usize defaultBindingCount = 0;
        usize conflictCount = 0;
        bool overrideApplied = false;
        bool conflictDetected = false;
        bool invalidDropped = false;
        bool duplicateDropped = false;
        bool textEntryBlocked = false;
        bool inputMapBuilt = false;
        bool ok = false;
        std::string summary;
    };

    EditorShortcutProbeResult BuildEditorShortcutProbe();
}
