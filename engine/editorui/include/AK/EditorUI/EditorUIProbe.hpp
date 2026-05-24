#pragma once

#include <AK/EditorUI/EditorSearch.hpp>

#include <string>

namespace AK
{
    struct EditorUIProbeResult
    {
        EditorPanelDiagnostics panelDiagnostics{};
        EditorDockDiagnostics dockDiagnostics{};
        EditorChromeDiagnostics chromeDiagnostics{};
        EditorSelectionDiagnostics selectionDiagnostics{};
        EditorSearchDiagnostics searchDiagnostics{};
        EditorFrameLayout frame{};
        EditorMenuModel menu{};
        EditorToolbarModel toolbar{};
        EditorStatusBarModel status{};
        std::vector<EditorOverlayDesc> overlays;
        EditorSelectionModel selection{};
        EditorSearchIndex searchIndex{};
        std::vector<EditorSearchResult> commandSearchResults;
        std::string serializedLayout;
        bool deserializedOk = false;
        bool ok = false;
        std::string summary;
    };

    EditorUIProbeResult BuildEditorUIProbe();
}
