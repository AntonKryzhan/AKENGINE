#pragma once

#include <AK/EditorUI/EditorPanelModels.hpp>

#include <string>

namespace AK
{
    struct EditorPanelProbeResult
    {
        EditorRuntimeBridge bridge{};
        EditorPanelModelFrame frame{};
        EditorPanelModelDiagnostics diagnostics{};
        std::vector<EditorSearchResult> propertySearchResults;
        std::vector<EditorSearchResult> assetSearchResults;
        std::string firstProperty;
        std::string firstAsset;
        std::string firstConsoleEntry;
        bool ok = false;
        std::string summary;
    };

    EditorPanelProbeResult BuildEditorPanelProbe();
}
