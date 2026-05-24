#include <AK/EditorUI/EditorPanelProbe.hpp>

#include <sstream>

namespace AK
{
    EditorPanelProbeResult BuildEditorPanelProbe()
    {
        EditorPanelProbeResult result{};
        result.bridge = BuildDefaultEditorRuntimeBridge(1600, 900);
        result.frame = BuildDefaultEditorPanelModelFrame(result.bridge);
        result.diagnostics = ValidateEditorPanelModelFrame(result.frame);
        result.propertySearchResults = SearchEditorIndex(result.frame.searchIndex, {"position", 8, true});
        result.assetSearchResults = SearchEditorIndex(result.frame.searchIndex, {"scan", 8, true});

        if (!result.frame.inspector.components.empty() && !result.frame.inspector.components.front().properties.empty())
        {
            result.firstProperty = FormatEditorProperty(result.frame.inspector.components.front().properties.front());
        }
        if (!result.frame.assets.items.empty())
        {
            result.firstAsset = FormatEditorAssetItem(result.frame.assets.items.front());
        }
        if (!result.frame.console.entries.empty())
        {
            result.firstConsoleEntry = FormatEditorConsoleEntry(result.frame.console.entries.front());
        }

        result.ok = result.diagnostics.ok
            && !result.propertySearchResults.empty()
            && !result.assetSearchResults.empty()
            && !result.firstProperty.empty()
            && !result.firstAsset.empty()
            && !result.firstConsoleEntry.empty();

        std::ostringstream out;
        out << (result.ok ? "[ ok ]" : "[fail]")
            << " editor panel data/property model foundation"
            << " hierarchy=" << result.diagnostics.hierarchyNodeCount
            << " components=" << result.diagnostics.componentCount
            << " properties=" << result.diagnostics.propertyCount
            << " ranges=" << result.diagnostics.rangedPropertyCount
            << " steps=" << result.diagnostics.steppedPropertyCount
            << " assets=" << result.diagnostics.assetCount
            << " consoleErrors=" << result.diagnostics.consoleErrorCount
            << " search=" << result.diagnostics.searchRecordCount;
        result.summary = out.str();
        return result;
    }
}
