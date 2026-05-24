#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorMenuItemKind
    {
        Command,
        Separator,
        Submenu
    };

    struct EditorMenuItem
    {
        EditorMenuItemKind kind = EditorMenuItemKind::Command;
        std::string path;
        std::string label;
        CommandId command = CommandId::CloseEditor;
        bool enabled = true;
        bool checked = false;
        bool destructive = false;
    };

    struct EditorMenuModel
    {
        std::vector<EditorMenuItem> items;
    };

    enum class EditorToolbarItemKind
    {
        Command,
        Separator,
        Toggle,
        Spacer
    };

    struct EditorToolbarItem
    {
        EditorToolbarItemKind kind = EditorToolbarItemKind::Command;
        std::string section;
        std::string label;
        std::string tooltip;
        CommandId command = CommandId::CloseEditor;
        bool enabled = true;
        bool active = false;
    };

    struct EditorToolbarModel
    {
        std::vector<EditorToolbarItem> items;
    };

    enum class EditorStatusSeverity
    {
        Info,
        Warning,
        Error
    };

    struct EditorStatusItem
    {
        std::string key;
        std::string text;
        EditorStatusSeverity severity = EditorStatusSeverity::Info;
        bool transient = false;
    };

    struct EditorStatusBarModel
    {
        std::vector<EditorStatusItem> items;
        std::string leftText;
        std::string rightText;
    };

    enum class EditorOverlayKind
    {
        ViewportToolbar,
        TransformTools,
        GridAndSnap,
        CameraMode,
        OrientationGizmo,
        FrameStats,
        SelectionOutline,
        WorldPartitionCells,
        PhysicsDebug,
        RenderMode
    };

    struct EditorOverlayDesc
    {
        u32 id = 0;
        EditorOverlayKind kind = EditorOverlayKind::ViewportToolbar;
        EditorPanelId targetPanel{};
        std::string name;
        EditorRect anchorRect{};
        i32 priority = 0;
        bool visible = true;
        bool interactive = false;
    };

    struct EditorChromeDiagnostics
    {
        std::size_t menuItemCount = 0;
        std::size_t toolbarItemCount = 0;
        std::size_t statusItemCount = 0;
        std::size_t overlayCount = 0;
        std::size_t invalidCommandCount = 0;
        std::size_t hiddenOverlayCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorMenuItemKind kind);
    const char* ToString(EditorToolbarItemKind kind);
    const char* ToString(EditorStatusSeverity severity);
    const char* ToString(EditorOverlayKind kind);

    EditorMenuModel BuildDefaultEditorMenuModel(const CommandRegistry& commands);
    EditorToolbarModel BuildDefaultEditorToolbarModel(const CommandRegistry& commands);
    EditorStatusBarModel BuildDefaultEditorStatusBarModel(const EditorDockDiagnostics& dockDiagnostics);
    std::vector<EditorOverlayDesc> BuildDefaultSceneViewportOverlays(const EditorPanelRegistry& panels);
    EditorChromeDiagnostics ValidateEditorChrome(const EditorMenuModel& menu, const EditorToolbarModel& toolbar, const EditorStatusBarModel& status, const std::vector<EditorOverlayDesc>& overlays, const CommandRegistry& commands, const EditorPanelRegistry& panels);
    std::string FormatEditorMenuItem(const EditorMenuItem& item);
    std::string FormatEditorToolbarItem(const EditorToolbarItem& item);
    std::string FormatEditorOverlay(const EditorOverlayDesc& overlay);
}
