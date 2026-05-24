#pragma once

#include <AK/EditorUI/EditorInteraction.hpp>

#include <string>

namespace AK
{
    struct EditorInteractionProbeResult
    {
        EditorInteractionContext context{};
        EditorInteractionDiagnostics diagnostics{};
        EditorInteractionFrameResult hierarchySelection{};
        EditorInteractionFrameResult propertyCommit{};
        EditorInteractionFrameResult paletteAccept{};
        EditorInteractionFrameResult overlayToggle{};
        std::string paletteSummary;
        std::string propertySummary;
        bool ok = false;
        std::string summary;
    };

    EditorInteractionProbeResult BuildEditorInteractionProbe();
}
