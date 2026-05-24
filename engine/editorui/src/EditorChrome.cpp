#include <AK/EditorUI/EditorChrome.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool CommandExists(const CommandRegistry& commands, CommandId id)
        {
            return commands.Find(id) != nullptr;
        }

        void AddMenuCommand(EditorMenuModel& menu, const char* path, const char* label, CommandId command, bool destructive = false)
        {
            menu.items.push_back({EditorMenuItemKind::Command, path, label, command, true, false, destructive});
        }

        void AddMenuSeparator(EditorMenuModel& menu, const char* path)
        {
            EditorMenuItem item{};
            item.kind = EditorMenuItemKind::Separator;
            item.path = path;
            item.enabled = false;
            menu.items.push_back(std::move(item));
        }

        void AddToolbarCommand(EditorToolbarModel& toolbar, const char* section, const char* label, const char* tooltip, CommandId command)
        {
            toolbar.items.push_back({EditorToolbarItemKind::Command, section, label, tooltip, command, true, false});
        }

        void AddToolbarSeparator(EditorToolbarModel& toolbar, const char* section)
        {
            EditorToolbarItem item{};
            item.kind = EditorToolbarItemKind::Separator;
            item.section = section;
            item.enabled = false;
            toolbar.items.push_back(std::move(item));
        }

        EditorPanelId PanelId(const EditorPanelRegistry& panels, const char* name)
        {
            const EditorPanelDesc* panel = panels.FindByName(name);
            return panel ? panel->id : EditorPanelId{};
        }
    }

    const char* ToString(EditorMenuItemKind kind)
    {
        switch (kind)
        {
            case EditorMenuItemKind::Command: return "command";
            case EditorMenuItemKind::Separator: return "separator";
            case EditorMenuItemKind::Submenu: return "submenu";
            default: return "unknown";
        }
    }

    const char* ToString(EditorToolbarItemKind kind)
    {
        switch (kind)
        {
            case EditorToolbarItemKind::Command: return "command";
            case EditorToolbarItemKind::Separator: return "separator";
            case EditorToolbarItemKind::Toggle: return "toggle";
            case EditorToolbarItemKind::Spacer: return "spacer";
            default: return "unknown";
        }
    }

    const char* ToString(EditorStatusSeverity severity)
    {
        switch (severity)
        {
            case EditorStatusSeverity::Info: return "info";
            case EditorStatusSeverity::Warning: return "warning";
            case EditorStatusSeverity::Error: return "error";
            default: return "unknown";
        }
    }

    const char* ToString(EditorOverlayKind kind)
    {
        switch (kind)
        {
            case EditorOverlayKind::ViewportToolbar: return "viewport-toolbar";
            case EditorOverlayKind::TransformTools: return "transform-tools";
            case EditorOverlayKind::GridAndSnap: return "grid-snap";
            case EditorOverlayKind::CameraMode: return "camera-mode";
            case EditorOverlayKind::OrientationGizmo: return "orientation-gizmo";
            case EditorOverlayKind::FrameStats: return "frame-stats";
            case EditorOverlayKind::SelectionOutline: return "selection-outline";
            case EditorOverlayKind::WorldPartitionCells: return "world-partition-cells";
            case EditorOverlayKind::PhysicsDebug: return "physics-debug";
            case EditorOverlayKind::RenderMode: return "render-mode";
            default: return "unknown";
        }
    }

    EditorMenuModel BuildDefaultEditorMenuModel(const CommandRegistry& commands)
    {
        (void)commands;
        EditorMenuModel menu{};
        AddMenuCommand(menu, "File/New Entity", "New Entity", CommandId::NewEntity);
        AddMenuCommand(menu, "File/Save Scene", "Save Scene", CommandId::SaveScene);
        AddMenuCommand(menu, "File/Load Scene", "Load Scene", CommandId::LoadScene);
        AddMenuSeparator(menu, "File/-1");
        AddMenuCommand(menu, "File/Exit", "Exit", CommandId::CloseEditor);

        AddMenuCommand(menu, "Edit/Undo", "Undo", CommandId::Undo);
        AddMenuCommand(menu, "Edit/Redo", "Redo", CommandId::Redo);
        AddMenuSeparator(menu, "Edit/-1");
        AddMenuCommand(menu, "Edit/Duplicate", "Duplicate Selection", CommandId::DuplicateSelection);
        AddMenuCommand(menu, "Edit/Delete", "Delete Selection", CommandId::DeleteSelection, true);
        AddMenuCommand(menu, "Edit/Rename", "Rename Selection", CommandId::RenameSelection);

        AddMenuCommand(menu, "GameObject/Create Empty", "Create Empty", CommandId::NewEntity);
        AddMenuCommand(menu, "GameObject/3D Object/Mesh", "Mesh", CommandId::NewMesh);
        AddMenuCommand(menu, "GameObject/Camera", "Camera", CommandId::NewCamera);
        AddMenuCommand(menu, "GameObject/Light", "Light", CommandId::NewLight);

        AddMenuCommand(menu, "Assets/Rescan", "Rescan Assets", CommandId::RescanAssets);
        AddMenuCommand(menu, "Window/Focus Selection", "Focus Selection", CommandId::FocusSelection);
        AddMenuCommand(menu, "Window/Reset Viewport", "Reset Viewport", CommandId::ResetViewport);
        AddMenuCommand(menu, "Window/Toggle Grid Snap", "Toggle Grid Snap", CommandId::ToggleGridSnap);
        AddMenuCommand(menu, "Window/Scene View/2D Mode", "2D Mode", CommandId::ViewScene2D);
        AddMenuCommand(menu, "Window/Scene View/3D Mode", "3D Mode", CommandId::ViewScene3D);
        AddMenuCommand(menu, "Window/Scene View/Frame All", "Frame All", CommandId::ViewFrameAll);
        AddMenuCommand(menu, "Window/Scene View/View Top", "View Top", CommandId::ViewAxisTop);
        AddMenuCommand(menu, "Window/Scene View/View Front", "View Front", CommandId::ViewAxisFront);
        AddMenuCommand(menu, "Window/Scene View/View Right", "View Right", CommandId::ViewAxisRight);

        AddMenuCommand(menu, "Tools/Transform/Translate", "Translate", CommandId::ToolTranslate);
        AddMenuCommand(menu, "Tools/Transform/Rotate", "Rotate", CommandId::ToolRotate);
        AddMenuCommand(menu, "Tools/Transform/Scale", "Scale", CommandId::ToolScale);
        AddMenuCommand(menu, "Tools/Transform/World Space", "World Space", CommandId::ToolSpaceWorld);
        AddMenuCommand(menu, "Tools/Transform/Local Space", "Local Space", CommandId::ToolSpaceLocal);
        return menu;
    }

    EditorToolbarModel BuildDefaultEditorToolbarModel(const CommandRegistry& commands)
    {
        (void)commands;
        EditorToolbarModel toolbar{};

        AddToolbarCommand(toolbar, "create", "Create", "Open the GameObject/Create workflow", CommandId::NewEntity);
        AddToolbarCommand(toolbar, "create", "Mesh", "Create a mesh entity", CommandId::NewMesh);
        AddToolbarCommand(toolbar, "create", "Camera", "Create a camera", CommandId::NewCamera);
        AddToolbarCommand(toolbar, "create", "Light", "Create a light", CommandId::NewLight);
        AddToolbarSeparator(toolbar, "create");

        AddToolbarCommand(toolbar, "tools", "Move", "Scene View translate tool", CommandId::ToolTranslate);
        AddToolbarCommand(toolbar, "tools", "Rot", "Scene View rotate tool", CommandId::ToolRotate);
        AddToolbarCommand(toolbar, "tools", "Scale", "Scene View scale tool", CommandId::ToolScale);
        AddToolbarSeparator(toolbar, "tools");
        AddToolbarCommand(toolbar, "space", "World", "Scene View world-space transform axes", CommandId::ToolSpaceWorld);
        AddToolbarCommand(toolbar, "space", "Local", "Scene View local transform axes", CommandId::ToolSpaceLocal);
        AddToolbarSeparator(toolbar, "space");

        AddToolbarCommand(toolbar, "viewport", "2D", "Use 2D orthographic Scene View", CommandId::ViewScene2D);
        AddToolbarCommand(toolbar, "viewport", "3D", "Use 3D perspective Scene View", CommandId::ViewScene3D);
        AddToolbarCommand(toolbar, "viewport", "Focus", "Focus selected entity", CommandId::FocusSelection);
        AddToolbarCommand(toolbar, "viewport", "Snap", "Toggle grid snap", CommandId::ToggleGridSnap);
        AddToolbarSeparator(toolbar, "viewport");

        AddToolbarCommand(toolbar, "file", "Save", "Save scene", CommandId::SaveScene);
        AddToolbarCommand(toolbar, "assets", "Rescan", "Rescan assets", CommandId::RescanAssets);
        return toolbar;
    }

    EditorStatusBarModel BuildDefaultEditorStatusBarModel(const EditorDockDiagnostics& dockDiagnostics)
    {
        EditorStatusBarModel status{};
        status.leftText = dockDiagnostics.ok ? "Editor layout ready" : "Editor layout has issues";
        status.rightText = dockDiagnostics.summary;
        status.items.push_back({"layout", dockDiagnostics.ok ? "Dock layout OK" : "Dock layout invalid", dockDiagnostics.ok ? EditorStatusSeverity::Info : EditorStatusSeverity::Error, false});
        status.items.push_back({"renderer", "GDI shell now, Vulkan/ImGui shell later", EditorStatusSeverity::Info, false});
        status.items.push_back({"save", "Layout serialization: AKLAYOUT 1", EditorStatusSeverity::Info, false});
        return status;
    }

    std::vector<EditorOverlayDesc> BuildDefaultSceneViewportOverlays(const EditorPanelRegistry& panels)
    {
        const EditorPanelId scene = PanelId(panels, "scene.viewport");
        std::vector<EditorOverlayDesc> overlays;
        overlays.push_back({1, EditorOverlayKind::ViewportToolbar, scene, "Scene", {10, 10, 144, 28}, 10, true, true});
        overlays.push_back({2, EditorOverlayKind::TransformTools, scene, "Transform Tools", {10, 48, 38, 136}, 20, true, true});
        overlays.push_back({3, EditorOverlayKind::GridAndSnap, scene, "Grid / Snap", {164, 10, 118, 28}, 30, true, true});
        overlays.push_back({4, EditorOverlayKind::CameraMode, scene, "Camera", {290, 10, 126, 28}, 40, true, true});
        overlays.push_back({5, EditorOverlayKind::OrientationGizmo, scene, "Orientation Gizmo", {0, 10, 104, 104}, 50, true, true});
        overlays.push_back({6, EditorOverlayKind::FrameStats, scene, "Frame Stats", {10, 0, 220, 58}, 60, false, false});
        overlays.push_back({7, EditorOverlayKind::SelectionOutline, scene, "Selection Outline", {0, 0, 0, 0}, 70, true, false});
        overlays.push_back({8, EditorOverlayKind::WorldPartitionCells, scene, "World Partition Cells", {0, 0, 0, 0}, 80, false, false});
        overlays.push_back({9, EditorOverlayKind::PhysicsDebug, scene, "Physics Debug", {0, 0, 0, 0}, 90, false, false});
        overlays.push_back({10, EditorOverlayKind::RenderMode, scene, "Shaded", {424, 10, 126, 28}, 100, true, true});
        return overlays;
    }

    EditorChromeDiagnostics ValidateEditorChrome(const EditorMenuModel& menu, const EditorToolbarModel& toolbar, const EditorStatusBarModel& status, const std::vector<EditorOverlayDesc>& overlays, const CommandRegistry& commands, const EditorPanelRegistry& panels)
    {
        EditorChromeDiagnostics diagnostics{};
        diagnostics.menuItemCount = menu.items.size();
        diagnostics.toolbarItemCount = toolbar.items.size();
        diagnostics.statusItemCount = status.items.size();
        diagnostics.overlayCount = overlays.size();

        for (const EditorMenuItem& item : menu.items)
        {
            if (item.kind == EditorMenuItemKind::Command && !CommandExists(commands, item.command))
            {
                ++diagnostics.invalidCommandCount;
            }
        }
        for (const EditorToolbarItem& item : toolbar.items)
        {
            if ((item.kind == EditorToolbarItemKind::Command || item.kind == EditorToolbarItemKind::Toggle) && !CommandExists(commands, item.command))
            {
                ++diagnostics.invalidCommandCount;
            }
        }
        for (const EditorOverlayDesc& overlay : overlays)
        {
            if (!overlay.visible)
            {
                ++diagnostics.hiddenOverlayCount;
            }
            if (!panels.Has(overlay.targetPanel))
            {
                ++diagnostics.invalidCommandCount;
            }
        }

        diagnostics.ok = diagnostics.menuItemCount >= 12
            && diagnostics.toolbarItemCount >= 8
            && diagnostics.statusItemCount >= 2
            && diagnostics.overlayCount >= 6
            && diagnostics.invalidCommandCount == 0;

        std::ostringstream out;
        out << "chrome menu=" << diagnostics.menuItemCount
            << " toolbar=" << diagnostics.toolbarItemCount
            << " status=" << diagnostics.statusItemCount
            << " overlays=" << diagnostics.overlayCount
            << " hiddenOverlays=" << diagnostics.hiddenOverlayCount
            << " invalid=" << diagnostics.invalidCommandCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string FormatEditorMenuItem(const EditorMenuItem& item)
    {
        std::ostringstream out;
        out << ToString(item.kind) << ' ' << item.path << " label=" << item.label;
        if (item.kind == EditorMenuItemKind::Command)
        {
            out << " command=" << ToString(item.command);
        }
        return out.str();
    }

    std::string FormatEditorToolbarItem(const EditorToolbarItem& item)
    {
        std::ostringstream out;
        out << item.section << ':' << item.label << " kind=" << ToString(item.kind);
        if (item.kind == EditorToolbarItemKind::Command || item.kind == EditorToolbarItemKind::Toggle)
        {
            out << " command=" << ToString(item.command);
        }
        return out.str();
    }

    std::string FormatEditorOverlay(const EditorOverlayDesc& overlay)
    {
        std::ostringstream out;
        out << overlay.id << ':' << overlay.name << " kind=" << ToString(overlay.kind)
            << " target=" << overlay.targetPanel.value
            << " visible=" << (overlay.visible ? "true" : "false")
            << " priority=" << overlay.priority;
        return out.str();
    }
}
