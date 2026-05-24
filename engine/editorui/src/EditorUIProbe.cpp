#include <AK/EditorUI/EditorUIProbe.hpp>

#include <AK/Input/InputActions.hpp>

#include <sstream>

namespace AK
{
    namespace
    {
        EditorSelectionItem MakeEntitySelection(u32 index, u32 generation, const char* name)
        {
            EditorSelectionItem item{};
            item.domain = EditorSelectionDomain::SceneEntity;
            item.entity = EntityId{index, generation};
            item.stableId = ToString(item.entity);
            item.displayName = name;
            return item;
        }
    }

    EditorUIProbeResult BuildEditorUIProbe()
    {
        EditorUIProbeResult result{};

        const InputMap inputMap = BuildDefaultEditorInputMap();
        const CommandRegistry commands = BuildDefaultEditorCommandRegistry();
        const EditorPanelRegistry panels = BuildDefaultEditorPanelRegistry();

        result.panelDiagnostics = BuildEditorPanelDiagnostics(panels);
        result.frame = BuildDefaultEditorFrameLayout(panels, 1600, 920);
        result.dockDiagnostics = ValidateEditorDockLayout(result.frame, panels);
        result.menu = BuildDefaultEditorMenuModel(commands);
        result.toolbar = BuildDefaultEditorToolbarModel(commands);
        result.status = BuildDefaultEditorStatusBarModel(result.dockDiagnostics);
        result.overlays = BuildDefaultSceneViewportOverlays(panels);
        result.chromeDiagnostics = ValidateEditorChrome(result.menu, result.toolbar, result.status, result.overlays, commands, panels);

        SetSelection(result.selection, MakeEntitySelection(1, 1, "Main Camera"));
        AddSelection(result.selection, MakeEntitySelection(2, 1, "Directional Light"));
        result.selectionDiagnostics = ValidateEditorSelection(result.selection);

        std::vector<EditorSelectionItem> sceneEntities = result.selection.items;
        sceneEntities.push_back(MakeEntitySelection(3, 1, "Planet Terrain Root"));
        sceneEntities.push_back(MakeEntitySelection(4, 1, "Destructible Wall"));
        const std::vector<std::string> assets = {
            "assets/models/ak_cube.glb",
            "assets/materials/default.akmat",
            "assets/scenes/sandbox.akscene",
            "assets/terrain/planet_dem.akdem"
        };
        result.searchIndex = BuildDefaultEditorSearchIndex(commands, panels, sceneEntities, assets);
        result.searchDiagnostics = ValidateEditorSearchIndex(result.searchIndex);
        result.commandSearchResults = SearchEditorIndex(result.searchIndex, {"save", 8, false});

        result.serializedLayout = SerializeEditorDockLayout(result.frame);
        const Result<EditorFrameLayout> loaded = DeserializeEditorDockLayout(result.serializedLayout, panels);
        result.deserializedOk = loaded.Ok();
        if (loaded.Ok())
        {
            const EditorDockDiagnostics loadedDiagnostics = ValidateEditorDockLayout(loaded.Value(), panels);
            result.deserializedOk = loadedDiagnostics.ok && loadedDiagnostics.tabCount == result.dockDiagnostics.tabCount;
        }

        const CommandDiagnostics commandDiagnostics = BuildCommandDiagnostics(commands, inputMap);
        result.ok = result.panelDiagnostics.ok
            && result.dockDiagnostics.ok
            && result.chromeDiagnostics.ok
            && result.selectionDiagnostics.ok
            && result.searchDiagnostics.ok
            && result.deserializedOk
            && commandDiagnostics.missingRequiredCommandCount == 0
            && !result.commandSearchResults.empty();

        std::ostringstream out;
        out << (result.ok ? "[ ok ]" : "[fail]")
            << " editor ui architecture foundation"
            << " panels=" << result.panelDiagnostics.panelCount
            << " nodes=" << result.dockDiagnostics.nodeCount
            << " tabs=" << result.dockDiagnostics.tabCount
            << " menu=" << result.chromeDiagnostics.menuItemCount
            << " toolbar=" << result.chromeDiagnostics.toolbarItemCount
            << " overlays=" << result.chromeDiagnostics.overlayCount
            << " search=" << result.searchDiagnostics.indexedRecordCount
            << " serialized=" << result.serializedLayout.size()
            << " loaded=" << (result.deserializedOk ? "true" : "false");
        result.summary = out.str();
        return result;
    }
}
