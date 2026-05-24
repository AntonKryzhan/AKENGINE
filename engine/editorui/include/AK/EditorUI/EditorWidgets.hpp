#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>
#include <AK/EditorUI/EditorTheme.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorWidgetKind
    {
        Label,
        Button,
        IconButton,
        Toggle,
        CheckBox,
        Dropdown,
        SearchBox,
        TextField,
        NumericField,
        Vec3Field,
        Foldout,
        Separator,
        TreeRow,
        AssetGridItem,
        ContextMenu,
        PopupPanel,
        ComponentHeader,
        InspectorObjectHeader,
        SceneToolbar
    };

    enum class EditorIconKind
    {
        None,
        Scene,
        Folder,
        Asset,
        Entity,
        Mesh,
        Camera,
        Light,
        Material,
        Shader,
        Script,
        Transform,
        Bounds,
        Console,
        Diagnostics,
        Search,
        Plus,
        Lock,
        Eye,
        Hand,
        Move,
        Rotate,
        Scale,
        Play,
        Pause,
        Step,
        Warning,
        Error
    };

    enum class EditorWidgetState : u32
    {
        Normal = 0,
        Hovered = 1u << 0u,
        Active = 1u << 1u,
        Focused = 1u << 2u,
        Disabled = 1u << 3u,
        Selected = 1u << 4u,
        Expanded = 1u << 5u,
        Dirty = 1u << 6u,
        Checked = 1u << 7u
    };

    constexpr EditorWidgetState operator|(EditorWidgetState a, EditorWidgetState b)
    {
        return static_cast<EditorWidgetState>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    constexpr bool HasEditorWidgetState(EditorWidgetState value, EditorWidgetState flag)
    {
        return (static_cast<u32>(value) & static_cast<u32>(flag)) != 0;
    }

    struct EditorWidgetSkin
    {
        i32 rowHeight = 21;
        i32 headerHeight = 24;
        i32 tabHeight = 23;
        i32 fieldHeight = 19;
        i32 iconSize = 16;
        i32 padding = 6;
        i32 indent = 16;
        i32 inspectorLabelWidth = 76;
        i32 assetCellWidth = 82;
        i32 assetCellHeight = 72;
    };

    struct EditorWidgetDesc
    {
        EditorWidgetKind kind = EditorWidgetKind::Label;
        EditorIconKind icon = EditorIconKind::None;
        EditorRect rect{};
        std::string id;
        std::string label;
        std::string value;
        std::string tooltip;
        i32 depth = 0;
        EditorWidgetState state = EditorWidgetState::Normal;
        bool commandTarget = false;
    };

    struct EditorContextMenuItem
    {
        std::string id;
        std::string label;
        EditorIconKind icon = EditorIconKind::None;
        bool separatorBefore = false;
        bool enabled = true;
        bool checked = false;
        bool destructive = false;
        std::vector<EditorContextMenuItem> children;
    };

    struct EditorContextMenuModel
    {
        std::string id;
        EditorRect anchor{};
        std::vector<EditorContextMenuItem> items;
        bool open = false;
    };

    struct EditorPopupModel
    {
        std::string id;
        std::string title;
        EditorRect anchor{};
        std::vector<EditorWidgetDesc> rows;
        bool open = false;
    };

    struct EditorWidgetFrame
    {
        EditorWidgetSkin skin{};
        std::vector<EditorWidgetDesc> hierarchyWidgets;
        std::vector<EditorWidgetDesc> inspectorWidgets;
        std::vector<EditorWidgetDesc> projectWidgets;
        std::vector<EditorWidgetDesc> sceneToolbarWidgets;
        std::vector<EditorContextMenuModel> contextMenus;
        std::vector<EditorPopupModel> popups;
        u64 revision = 1;
    };

    struct EditorWidgetDiagnostics
    {
        std::size_t hierarchyWidgetCount = 0;
        std::size_t inspectorWidgetCount = 0;
        std::size_t projectWidgetCount = 0;
        std::size_t sceneToolbarWidgetCount = 0;
        std::size_t contextMenuCount = 0;
        std::size_t popupCount = 0;
        std::size_t invalidRectCount = 0;
        std::size_t emptyIdCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorWidgetKind kind);
    const char* ToString(EditorIconKind kind);
    const char* ToString(EditorWidgetState state);

    EditorWidgetSkin MakeDefaultEditorWidgetSkin();
    EditorWidgetSkin MakeEditorWidgetSkinFromTheme(const EditorTheme& theme);
    EditorWidgetFrame BuildDefaultEditorWidgetFrame(i32 width, i32 height);
    std::vector<EditorWidgetDesc> BuildHierarchyTreeWidgets(EditorRect body, const std::vector<std::string>& hierarchyLines, const EditorWidgetSkin& skin);
    std::vector<EditorWidgetDesc> BuildInspectorPropertyWidgets(EditorRect body, const std::string& selectedName, const std::vector<std::string>& inspectorLines, const EditorWidgetSkin& skin);
    std::vector<EditorWidgetDesc> BuildProjectBrowserWidgets(EditorRect body, const std::vector<std::string>& assetLines, const EditorWidgetSkin& skin);
    std::vector<EditorWidgetDesc> BuildSceneViewToolbarWidgets(EditorRect body, const EditorWidgetSkin& skin);
    EditorContextMenuModel BuildDefaultHierarchyContextMenu(EditorRect anchor);
    EditorPopupModel BuildDefaultSceneStatsPopup(EditorRect anchor);
    EditorWidgetDiagnostics ValidateEditorWidgetFrame(const EditorWidgetFrame& frame);
    std::string FormatEditorWidget(const EditorWidgetDesc& widget);
    std::string FormatEditorWidgetDiagnostics(const EditorWidgetDiagnostics& diagnostics);
}
