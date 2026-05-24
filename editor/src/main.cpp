#include <AK/Assets/AssetDatabase.hpp>
#include <AK/Assets/AssetRegistry.hpp>
#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Log.hpp>
#include <AK/Core/Time.hpp>
#include <AK/Diagnostics/Diagnostics.hpp>
#include <AK/Filesystem/FileSystem.hpp>
#include <AK/Settings/ProjectSettings.hpp>
#include <AK/Serialization/Schema.hpp>
#include <AK/Platform/Window.hpp>
#include <AK/Input/InputActions.hpp>
#include <AK/Jobs/JobSystem.hpp>
#include <AK/Memory/Memory.hpp>
#include <AK/Package/AssetPackage.hpp>
#include <AK/VFS/VirtualFileSystem.hpp>
#include <AK/Streaming/StreamingSystem.hpp>
#include <AK/Render/Renderer.hpp>
#include <AK/Render/RenderFoundation.hpp>
#include <AK/Resources/ResourceManager.hpp>
#include <AK/Math/Chernoff.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Math/Transform.hpp>
#include <AK/Scene/Scene.hpp>
#include <AK/Visibility/Bounds.hpp>
#include <AK/World/WorldCoordinates.hpp>
#include <AK/WorldTopology/WorldTopology.hpp>
#include <AK/Gravity/GravityField.hpp>
#include <AK/Terrain/Terrain.hpp>
#include <AK/Spline/Spline.hpp>
#include <AK/CSG/Boolean.hpp>
#include <AK/Texture/TextureSet.hpp>
#include <AK/NTC/NeuralTextureCompression.hpp>
#include <AK/WorldPartition/WorldPartition.hpp>
#include <AK/EditorUI/EditorChrome.hpp>
#include <AK/EditorUI/EditorDock.hpp>
#include <AK/EditorUI/EditorPanel.hpp>
#include <AK/EditorUI/EditorRuntimeBridge.hpp>
#include <AK/EditorUI/EditorUXRuntime.hpp>
#include <AK/EditorUI/EditorScrollClip.hpp>
#include <AK/EditorUI/EditorFocus.hpp>
#include <AK/EditorUI/EditorFocusProbe.hpp>
#include <AK/EditorUI/EditorDragDrop.hpp>
#include <AK/EditorUI/EditorDragDropProbe.hpp>
#include <AK/EditorUI/EditorCommandStateProbe.hpp>
#include <AK/EditorUI/EditorWorkspace.hpp>
#include <AK/EditorUI/EditorAutosave.hpp>
#include <AK/EditorUI/EditorAutosaveProbe.hpp>
#include <AK/EditorUI/EditorActivity.hpp>
#include <AK/EditorUI/EditorActivityProbe.hpp>
#include <AK/EditorUI/EditorDiagnosticsPanel.hpp>
#include <AK/EditorUI/EditorDiagnosticsPanelProbe.hpp>
#include <AK/EditorUI/EditorDiagnosticsView.hpp>
#include <AK/EditorUI/EditorDiagnosticsInteraction.hpp>
#include <AK/EditorUI/EditorCommandState.hpp>
#include <AK/EditorUI/EditorCommandPalette.hpp>
#include <AK/EditorUI/EditorCommandPaletteRuntime.hpp>
#include <AK/EditorUI/EditorMenuToolbarRuntime.hpp>
#include <AK/EditorUI/EditorTransformGizmo.hpp>
#include <AK/EditorUI/EditorSceneView.hpp>
#include <AK/EditorUI/EditorSearch.hpp>
#include <AK/EditorUI/EditorSelection.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr int ToolbarHeight = 38;
    constexpr int HierarchyWidth = 260;
    constexpr int InspectorWidth = 340;
    constexpr int AssetBrowserHeight = 170;
    constexpr int ConsoleHeight = 150;
    constexpr int PanelGap = 6;
    constexpr int HeaderHeight = 29;
    constexpr int RowHeight = 21;
    constexpr int VisibleHierarchyRows = 18;
    constexpr float DefaultViewportScale = 36.0f;
    constexpr float MinViewportScale = 8.0f;
    constexpr float MaxViewportScale = 160.0f;
    constexpr float DefaultSnapStep = 0.5f;
    constexpr float DefaultRotateSnapStep = 15.0f;
    constexpr float DefaultScaleSnapStep = 0.1f;
    constexpr std::size_t MaxUndoSnapshots = 64;
    constexpr double EditorWorldCellSize = AK::DefaultWorldCellSizeMeters;
    constexpr float EditorVisibilityHalfExtent = 32.0f;

    struct Rect
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    struct EditorLayout
    {
        Rect toolbar;
        Rect hierarchy;
        Rect inspector;
        Rect viewport;
        Rect viewportGrid;
        Rect assets;
        Rect console;
    };

    struct ViewportCamera
    {
        float centerX = 0.0f;
        float centerZ = 0.0f;
        float scale = DefaultViewportScale;
        AK::EditorSceneViewCamera sceneView = AK::MakeDefaultEditorSceneViewCamera();
    };

    struct ViewportWorldPoint
    {
        float x = 0.0f;
        float z = 0.0f;
    };

    struct EntitySnapshot
    {
        std::string name;
        AK::TransformComponent transform{};
        bool hasWorldPosition = false;
        AK::WorldPositionComponent worldPosition{};
        bool hasBounds = false;
        AK::BoundsComponent bounds{};
        bool hasCamera = false;
        AK::CameraComponent camera{};
        bool hasLight = false;
        AK::LightComponent light{};
        bool hasMesh = false;
        AK::MeshComponent mesh{};
    };

    struct SceneSnapshot
    {
        std::vector<EntitySnapshot> entities;
        std::string selectedName;
        std::size_t selectedIndex = 0;
    };

    enum class ToolbarAction
    {
        None,
        NewEntity,
        NewMesh,
        NewCamera,
        NewLight,
        DeleteEntity,
        SaveScene,
        LoadScene,
        RescanAssets
    };

    EditorLayout BuildEditorLayout(const AK::Window& window)
    {
        const int clientLeft = 0;
        const int clientTop = 0;
        const int clientRight = static_cast<int>(window.Width());
        const int clientBottom = static_cast<int>(window.Height());

        EditorLayout layout{};
        layout.toolbar = {clientLeft, clientTop, clientRight, clientTop + ToolbarHeight};

        const int top = layout.toolbar.bottom + PanelGap;
        const int bottom = clientBottom - PanelGap;
        const int left = clientLeft + PanelGap;
        const int right = clientRight - PanelGap;
        const int lowerTop = std::max(top + 180, bottom - AssetBrowserHeight - ConsoleHeight - PanelGap);
        const int middleBottom = lowerTop - PanelGap;

        layout.hierarchy = {left, top, left + HierarchyWidth, middleBottom};
        layout.inspector = {right - InspectorWidth, top, right, middleBottom};
        layout.viewport = {layout.hierarchy.right + PanelGap, top, layout.inspector.left - PanelGap, middleBottom};
        layout.viewportGrid = {
            layout.viewport.left + 12,
            layout.viewport.top + HeaderHeight + 12,
            layout.viewport.right - 12,
            layout.viewport.bottom - 12
        };
        layout.assets = {left, lowerTop, right, lowerTop + AssetBrowserHeight};
        layout.console = {left, layout.assets.bottom + PanelGap, right, bottom};
        return layout;
    }

    bool Contains(const Rect& rect, int x, int y)
    {
        return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
    }


    AK::EditorShellRect ToShellRect(AK::EditorRect rect)
    {
        return {rect.x, rect.y, rect.width, rect.height};
    }

    Rect ToLegacyRect(AK::EditorRect rect)
    {
        return {rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
    }

    AK::CommandId CommandForTransformGizmoMode(AK::EditorTransformGizmoMode mode)
    {
        switch (mode)
        {
            case AK::EditorTransformGizmoMode::Rotate:
                return AK::CommandId::ToolRotate;
            case AK::EditorTransformGizmoMode::Scale:
                return AK::CommandId::ToolScale;
            case AK::EditorTransformGizmoMode::Translate:
            default:
                return AK::CommandId::ToolTranslate;
        }
    }

    AK::EditorTransformGizmoMode TransformGizmoModeForCommand(AK::CommandId command)
    {
        switch (command)
        {
            case AK::CommandId::ToolRotate:
                return AK::EditorTransformGizmoMode::Rotate;
            case AK::CommandId::ToolScale:
                return AK::EditorTransformGizmoMode::Scale;
            case AK::CommandId::ToolTranslate:
            default:
                return AK::EditorTransformGizmoMode::Translate;
        }
    }

    AK::CommandId CommandForTransformGizmoSpace(AK::EditorTransformGizmoSpace space)
    {
        switch (space)
        {
            case AK::EditorTransformGizmoSpace::Local:
                return AK::CommandId::ToolSpaceLocal;
            case AK::EditorTransformGizmoSpace::World:
            default:
                return AK::CommandId::ToolSpaceWorld;
        }
    }

    AK::EditorTransformGizmoSpace TransformGizmoSpaceForCommand(AK::CommandId command)
    {
        switch (command)
        {
            case AK::CommandId::ToolSpaceLocal:
                return AK::EditorTransformGizmoSpace::Local;
            case AK::CommandId::ToolSpaceWorld:
            default:
                return AK::EditorTransformGizmoSpace::World;
        }
    }

    AK::CommandId CommandForSceneViewMode(AK::EditorSceneViewMode mode)
    {
        switch (mode)
        {
            case AK::EditorSceneViewMode::Mode3D:
                return AK::CommandId::ViewScene3D;
            case AK::EditorSceneViewMode::Mode2D:
            default:
                return AK::CommandId::ViewScene2D;
        }
    }

    AK::EditorSceneViewMode SceneViewModeForCommand(AK::CommandId command)
    {
        switch (command)
        {
            case AK::CommandId::ViewScene3D:
                return AK::EditorSceneViewMode::Mode3D;
            case AK::CommandId::ViewScene2D:
            default:
                return AK::EditorSceneViewMode::Mode2D;
        }
    }

    AK::EditorSceneViewAxis SceneViewAxisForCommand(AK::CommandId command)
    {
        switch (command)
        {
            case AK::CommandId::ViewAxisFront:
                return AK::EditorSceneViewAxis::Front;
            case AK::CommandId::ViewAxisRight:
                return AK::EditorSceneViewAxis::Right;
            case AK::CommandId::ViewAxisTop:
            default:
                return AK::EditorSceneViewAxis::Top;
        }
    }

    AK::EditorRect ToEditorRect(const Rect& rect)
    {
        return {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
    }

    AK::EditorSceneViewBounds ToEditorSceneViewBounds(AK::AABB3 bounds)
    {
        AK::EditorSceneViewBounds out{};
        out.valid = AK::IsValid(bounds);
        out.minX = bounds.min.x;
        out.minY = bounds.min.y;
        out.minZ = bounds.min.z;
        out.maxX = bounds.max.x;
        out.maxY = bounds.max.y;
        out.maxZ = bounds.max.z;
        return out;
    }

    AK::EditorSceneViewCamera BuildSceneViewCamera(const ViewportCamera& camera)
    {
        AK::EditorSceneViewCamera sceneView = camera.sceneView;
        sceneView.centerX = camera.centerX;
        sceneView.centerZ = camera.centerZ;
        sceneView.orthographicScale = camera.scale;
        return sceneView;
    }

    void ApplySceneViewCamera(ViewportCamera& camera, const AK::EditorSceneViewCamera& sceneView)
    {
        camera.sceneView = sceneView;
        camera.centerX = sceneView.centerX;
        camera.centerZ = sceneView.centerZ;
        camera.scale = sceneView.orthographicScale;
    }

    bool IsViewport3D(const ViewportCamera& camera)
    {
        return camera.sceneView.mode == AK::EditorSceneViewMode::Mode3D;
    }

    float SnapStepForTransformGizmoMode(AK::EditorTransformGizmoMode mode)
    {
        switch (mode)
        {
            case AK::EditorTransformGizmoMode::Rotate:
                return DefaultRotateSnapStep;
            case AK::EditorTransformGizmoMode::Scale:
                return DefaultScaleSnapStep;
            case AK::EditorTransformGizmoMode::Translate:
            default:
                return DefaultSnapStep;
        }
    }


    const AK::EditorPanelRuntimeState* FindRuntimePanelStateByName(const AK::EditorRuntimeBridge& bridge, const char* name, bool activeOnly = false)
    {
        const AK::EditorPanelDesc* panel = bridge.panels.FindByName(name);
        if (!panel)
        {
            return nullptr;
        }

        for (const AK::EditorPanelRuntimeState& state : bridge.plan.panels)
        {
            if (state.panel == panel->id && state.visible && (!activeOnly || state.active))
            {
                return &state;
            }
        }
        return nullptr;
    }

    Rect ActiveRuntimePanelBodyOrEmpty(const AK::EditorRuntimeBridge& bridge, const char* name)
    {
        if (const AK::EditorPanelRuntimeState* panel = FindRuntimePanelStateByName(bridge, name, true))
        {
            return ToLegacyRect(panel->bodyRect);
        }
        return {};
    }

    void ApplyEditorRuntimeLayoutToLegacyLayout(const AK::EditorRuntimeBridge& bridge, EditorLayout& layout)
    {
        if (const AK::EditorPanelRuntimeState* hierarchy = FindRuntimePanelStateByName(bridge, "scene.hierarchy", true))
        {
            layout.hierarchy = ToLegacyRect(hierarchy->bodyRect);
        }
        if (const AK::EditorPanelRuntimeState* inspector = FindRuntimePanelStateByName(bridge, "inspector", true))
        {
            layout.inspector = ToLegacyRect(inspector->bodyRect);
        }
        if (const AK::EditorPanelRuntimeState* viewport = FindRuntimePanelStateByName(bridge, "scene.viewport", true))
        {
            layout.viewport = ToLegacyRect(viewport->bodyRect);
            layout.viewportGrid = {
                layout.viewport.left + 12,
                layout.viewport.top + HeaderHeight + 13,
                layout.viewport.right - 12,
                layout.viewport.bottom - 12
            };
        }
        if (const AK::EditorPanelRuntimeState* assets = FindRuntimePanelStateByName(bridge, "assets.browser", true))
        {
            layout.assets = ToLegacyRect(assets->bodyRect);
        }
        if (const AK::EditorPanelRuntimeState* console = FindRuntimePanelStateByName(bridge, "console", true))
        {
            layout.console = ToLegacyRect(console->bodyRect);
        }
    }


    std::string EditorIconNameForRender(AK::EditorIconKind icon)
    {
        switch (icon)
        {
            case AK::EditorIconKind::Scene: return "scene";
            case AK::EditorIconKind::Folder: return "folder";
            case AK::EditorIconKind::Entity: return "entity";
            case AK::EditorIconKind::Mesh: return "mesh";
            case AK::EditorIconKind::Camera: return "camera";
            case AK::EditorIconKind::Light: return "light";
            case AK::EditorIconKind::Material: return "material";
            case AK::EditorIconKind::Shader: return "shader";
            case AK::EditorIconKind::Script: return "script";
            case AK::EditorIconKind::Transform: return "transform";
            case AK::EditorIconKind::Bounds: return "bounds";
            case AK::EditorIconKind::Console: return "console";
            case AK::EditorIconKind::Diagnostics: return "diagnostics";
            case AK::EditorIconKind::Search: return "search";
            case AK::EditorIconKind::Plus: return "plus";
            case AK::EditorIconKind::Lock: return "lock";
            case AK::EditorIconKind::Eye: return "eye";
            case AK::EditorIconKind::Warning: return "warning";
            case AK::EditorIconKind::Error: return "error";
            default: return "asset";
        }
    }

    AK::EditorShellRect ToShellRect(const AK::EditorUXPopupItemRuntime& item)
    {
        return {item.rect.x, item.rect.y, item.rect.width, item.rect.height};
    }

    std::vector<AK::EditorShellPopupDesc> BuildRenderPopupsFromEditorUXRuntime(const AK::EditorUXRuntime& runtime)
    {
        std::vector<AK::EditorShellPopupDesc> popups;
        if (!runtime.popupOpen || !runtime.surface.open)
        {
            return popups;
        }

        AK::EditorShellPopupDesc popup{};
        popup.id = runtime.surface.id;
        popup.title = runtime.surface.title;
        popup.rect = ToShellRect(runtime.surface.rect);
        popup.anchor = ToShellRect(runtime.surface.anchor);
        popup.open = runtime.surface.open;
        popup.popupPanel = runtime.surface.popupPanel;
        for (const AK::EditorUXPopupItemRuntime& item : runtime.surface.items)
        {
            AK::EditorShellPopupItemDesc out{};
            out.rect = ToShellRect(item);
            out.id = item.id;
            out.label = item.label;
            out.icon = EditorIconNameForRender(item.icon);
            out.enabled = item.enabled;
            out.checked = item.checked;
            out.destructive = item.destructive;
            out.separatorBefore = item.separatorBefore;
            out.hasSubmenu = item.hasSubmenu;
            out.hovered = runtime.hoveredItemId == item.id;
            popup.items.push_back(std::move(out));
        }
        popups.push_back(std::move(popup));
        return popups;
    }

    bool BuildRenderPopupFromMenuToolbarRuntime(const AK::EditorMenuToolbarRuntimeState& runtime, AK::EditorShellPopupDesc& popup)
    {
        if (!runtime.menuOpen || !runtime.surface.popup.open)
        {
            return false;
        }

        popup = {};
        popup.id = runtime.surface.popup.id;
        popup.title = runtime.surface.popup.title;
        popup.rect = ToShellRect(runtime.surface.popup.rect);
        popup.anchor = ToShellRect(runtime.surface.popup.anchor);
        popup.open = runtime.surface.popup.open;
        popup.popupPanel = false;
        for (const AK::EditorMenuToolbarCommandItemRuntime& item : runtime.surface.popup.items)
        {
            AK::EditorShellPopupItemDesc out{};
            out.rect = ToShellRect(item.rect);
            out.id = item.id;
            out.label = item.shortcutText.empty() ? item.label : (item.label + "    " + item.shortcutText);
            if (item.depth > 0)
            {
                out.label = std::string(static_cast<std::size_t>(item.depth * 2), ' ') + out.label;
            }
            out.icon = EditorIconNameForRender(item.icon);
            out.enabled = item.enabled;
            out.checked = item.checked;
            out.destructive = item.destructive;
            out.separatorBefore = item.separatorBefore;
            out.hasSubmenu = item.hasSubmenu;
            out.hovered = runtime.hover.id == item.id;
            popup.items.push_back(std::move(out));
        }
        return true;
    }


    AK::EditorCommandPaletteRenderDesc BuildRenderCommandPaletteSurface(const AK::EditorCommandPaletteSurface& surface)
    {
        AK::EditorCommandPaletteRenderDesc out{};
        out.open = surface.open;
        out.surfaceRect = ToShellRect(surface.surfaceRect);
        out.searchBoxRect = ToShellRect(surface.searchBoxRect);
        out.listRect = ToShellRect(surface.listRect);
        out.footerRect = ToShellRect(surface.footerRect);
        out.query = surface.query;
        out.placeholder = surface.placeholder;
        out.footerText = surface.footerText;
        out.visibleFirstRow = static_cast<std::uint32_t>(std::max(0, surface.visibleFirstRow));
        out.visibleRowCount = static_cast<std::uint32_t>(std::max(0, surface.visibleRowCount));
        out.selectedIndex = static_cast<std::uint32_t>(surface.selectedIndex);
        out.rowHeightPixels = surface.rowHeightPixels;
        out.revision = surface.revision;
        out.rows.reserve(surface.rows.size());
        for (const AK::EditorCommandPaletteRow& row : surface.rows)
        {
            AK::EditorCommandPaletteRowRenderDesc renderRow{};
            renderRow.icon = EditorIconNameForRender(row.icon);
            renderRow.kind = AK::ToString(row.resultKind);
            renderRow.title = row.title;
            renderRow.subtitle = row.subtitle;
            renderRow.rightText = row.rightText;
            renderRow.disabledReason = row.disabledReasonText;
            renderRow.selected = row.selected;
            renderRow.enabled = row.enabled;
            renderRow.destructive = row.destructive;
            out.rows.push_back(std::move(renderRow));
        }
        return out;
    }



    AK::EditorViewportGizmoRenderDesc BuildRenderTransformGizmo(const AK::EditorTransformGizmoSurface& surface, const AK::EditorTransformGizmoRuntime& runtime, int mouseX, int mouseY)
    {
        AK::EditorViewportGizmoRenderDesc out{};
        out.visible = surface.visible;
        out.mode = std::string(AK::ToString(surface.mode)) + "/" + AK::ToString(surface.space);
        out.status = surface.status;
        out.originX = surface.originScreenX;
        out.originY = surface.originScreenY;
        out.dragging = runtime.active;
        out.activeHandle = AK::ToString(runtime.handle);
        out.handles.reserve(surface.handles.size());

        for (const AK::EditorTransformGizmoHandleRect& handle : surface.handles)
        {
            if (!handle.visible)
            {
                continue;
            }

            AK::EditorViewportGizmoHandleRenderDesc renderHandle{};
            renderHandle.rect = ToShellRect(handle.rect);
            renderHandle.id = AK::ToString(handle.handle);
            renderHandle.label = handle.label;
            renderHandle.hovered = AK::EditorTransformGizmoRectContains(handle.rect, mouseX, mouseY);
            renderHandle.active = runtime.active && runtime.handle == handle.handle;
            out.handles.push_back(std::move(renderHandle));
        }

        return out;
    }

    std::vector<AK::EditorSelectionItem> BuildSceneSearchSelectionItems(const AK::Scene& scene)
    {
        std::vector<AK::EditorSelectionItem> items;
        items.reserve(scene.GetWorld().Entities().size());
        for (const AK::EntityRecord& record : scene.GetWorld().Entities())
        {
            AK::EditorSelectionItem item{};
            item.domain = AK::EditorSelectionDomain::SceneEntity;
            item.entity = record.id;
            item.stableId = AK::ToString(record.id);
            item.displayName = record.name;
            items.push_back(std::move(item));
        }
        return items;
    }

    Rect BuildToolbarSearchRect(const AK::EditorRuntimeBridge& bridge)
    {
        const AK::EditorFrameLayout& frame = bridge.plan.frame;
        return {frame.toolbar.x + frame.toolbar.width - 310, frame.toolbar.y + 7, frame.toolbar.x + frame.toolbar.width - 14, frame.toolbar.y + frame.toolbar.height - 7};
    }

    AK::EditorShellLayoutDesc BuildRenderShellLayoutFromEditorRuntime(const AK::EditorRuntimeBridge& bridge)
    {
        AK::EditorShellLayoutDesc shell{};
        shell.menuBar = ToShellRect(bridge.plan.frame.menuBar);
        shell.toolbar = ToShellRect(bridge.plan.frame.toolbar);
        shell.dockSpace = ToShellRect(bridge.plan.frame.dockSpace);
        shell.statusBar = ToShellRect(bridge.plan.frame.statusBar);
        shell.statusLeft = bridge.plan.status.leftText;
        shell.statusRight = bridge.plan.status.rightText;
        shell.hoverLabel = bridge.plan.hover.label;

        for (const AK::EditorPanelRuntimeState& state : bridge.plan.panels)
        {
            const AK::EditorPanelDesc* panel = bridge.panels.Find(state.panel);
            AK::EditorShellPanelDesc out{};
            out.id = state.panel.value;
            out.name = panel ? panel->name : std::string("panel.") + std::to_string(state.panel.value);
            out.title = panel ? panel->title : out.name;
            out.kind = panel ? AK::ToString(panel->kind) : "custom";
            out.tabRect = ToShellRect(state.tabRect);
            out.bodyRect = ToShellRect(state.bodyRect);
            out.visible = state.visible;
            out.active = state.active;
            out.focused = state.focused;
            shell.panels.push_back(std::move(out));
        }

        for (const AK::EditorDockSplitterHandle& splitter : bridge.plan.splitters)
        {
            AK::EditorShellSplitterDesc out{};
            out.rect = ToShellRect(splitter.rect);
            out.vertical = splitter.axis == AK::EditorDockSplitAxis::X;
            out.draggable = splitter.draggable;
            shell.splitters.push_back(out);
        }

        const int toolbarButtonWidth = 64;
        const int toolbarButtonGap = 4;
        int x = bridge.plan.frame.toolbar.x + 8;
        const int y = bridge.plan.frame.toolbar.y + 7;
        const int h = std::max(18, bridge.plan.frame.toolbar.height - 14);
        for (const AK::EditorToolbarItem& item : bridge.plan.toolbar.items)
        {
            if (item.kind != AK::EditorToolbarItemKind::Command && item.kind != AK::EditorToolbarItemKind::Toggle)
            {
                if (item.kind == AK::EditorToolbarItemKind::Separator)
                {
                    x += toolbarButtonGap + 6;
                }
                continue;
            }

            AK::EditorShellToolbarButtonDesc button{};
            const int labelWidth = item.label == "Create" ? 76 : (item.label == "Camera" ? 70 : toolbarButtonWidth);
            button.rect = {x, y, labelWidth, h};
            button.label = item.label.empty() ? AK::ToString(item.command) : item.label;
            button.enabled = item.enabled;
            button.active = item.active;
            button.destructive = item.command == AK::CommandId::DeleteSelection;
            for (const AK::EditorCommandRoute& route : bridge.plan.commandRoutes)
            {
                if (route.command == item.command && route.source == AK::CommandSource::Toolbar)
                {
                    button.enabled = route.enabled;
                    button.destructive = route.destructive;
                    break;
                }
            }
            shell.toolbarButtons.push_back(std::move(button));
            x += button.rect.width + toolbarButtonGap;
        }

        const AK::EditorPanelRuntimeState* scenePanel = FindRuntimePanelStateByName(bridge, "scene.viewport", true);
        for (const AK::EditorOverlayDesc& overlay : bridge.plan.overlays)
        {
            if (!overlay.visible || !scenePanel)
            {
                continue;
            }
            if (overlay.anchorRect.width <= 0 || overlay.anchorRect.height <= 0)
            {
                continue;
            }
            if (overlay.kind == AK::EditorOverlayKind::ViewportToolbar
                || overlay.kind == AK::EditorOverlayKind::GridAndSnap
                || overlay.kind == AK::EditorOverlayKind::CameraMode
                || overlay.kind == AK::EditorOverlayKind::RenderMode)
            {
                continue;
            }

            AK::EditorShellOverlayDesc out{};
            out.label = overlay.name;
            out.visible = overlay.visible;
            out.interactive = overlay.interactive;
            out.rect = ToShellRect(overlay.anchorRect);
            out.rect.x += scenePanel->bodyRect.x;
            out.rect.y += scenePanel->bodyRect.y;
            if (overlay.kind == AK::EditorOverlayKind::OrientationGizmo)
            {
                out.rect.x = scenePanel->bodyRect.x + std::max(0, scenePanel->bodyRect.width - overlay.anchorRect.width - 12);
                out.rect.y = scenePanel->bodyRect.y + 12;
            }
            if (out.rect.x + out.rect.width > scenePanel->bodyRect.x + scenePanel->bodyRect.width)
            {
                out.rect.x = scenePanel->bodyRect.x + std::max(0, scenePanel->bodyRect.width - out.rect.width - 12);
            }
            if (out.rect.y + out.rect.height > scenePanel->bodyRect.y + scenePanel->bodyRect.height)
            {
                out.rect.y = scenePanel->bodyRect.y + std::max(0, scenePanel->bodyRect.height - out.rect.height - 12);
            }
            shell.overlays.push_back(std::move(out));
        }

        shell.valid = !shell.panels.empty() && shell.menuBar.width > 0 && shell.toolbar.width > 0 && shell.dockSpace.width > 0;
        return shell;
    }

    ViewportWorldPoint ScreenToViewportWorld(const Rect& viewportGrid, const ViewportCamera& camera, int x, int y)
    {
        const AK::EditorSceneViewGroundPoint ground = AK::ScreenToEditorSceneViewGroundPoint(BuildSceneViewCamera(camera), ToEditorRect(viewportGrid), x, y, 0.0f);
        if (ground.valid)
        {
            return {ground.x, ground.z};
        }

        const int centerX = (viewportGrid.left + viewportGrid.right) / 2;
        const int centerY = (viewportGrid.top + viewportGrid.bottom) / 2;
        const float scale = std::clamp(camera.scale, MinViewportScale, MaxViewportScale);
        return {
            camera.centerX + static_cast<float>(x - centerX) / scale,
            camera.centerZ - static_cast<float>(y - centerY) / scale
        };
    }

    float SnapToGrid(float value, float step)
    {
        if (step <= 0.0f)
        {
            return value;
        }

        return std::round(value / step) * step;
    }

    void SnapTransformXZ(AK::TransformComponent& transform, float step)
    {
        transform.position.x = SnapToGrid(transform.position.x, step);
        transform.position.z = SnapToGrid(transform.position.z, step);
    }

    void MarkTransformDirty(AK::TransformComponent& transform, AK::u32 flags = AK::TransformDirty_All)
    {
        transform.dirtyFlags |= flags;
        ++transform.revision;
    }

    AK::EulerTransform ToEulerTransform(const AK::TransformComponent& transform)
    {
        return AK::MakeEulerTransform(transform.position, transform.rotation, transform.scale);
    }

    void ApplyEulerTransform(AK::TransformComponent& transform, const AK::EulerTransform& eulerTransform)
    {
        transform.position = eulerTransform.position;
        transform.rotation = eulerTransform.rotationDegrees;
        transform.scale = eulerTransform.scale;
    }

    AK::TransformSanitizeResult SanitizeTransformComponent(AK::TransformComponent& transform)
    {
        AK::EulerTransform eulerTransform = ToEulerTransform(transform);
        const AK::TransformSanitizeResult result = AK::SanitizeEulerTransform(eulerTransform);
        if (result.changed)
        {
            ApplyEulerTransform(transform, eulerTransform);
            MarkTransformDirty(transform);
        }
        return result;
    }

    std::size_t SanitizeSceneTransforms(AK::Scene& scene)
    {
        std::size_t fixedCount = 0;
        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            if (AK::TransformComponent* transform = scene.GetWorld().GetTransform(entity.id))
            {
                const AK::TransformSanitizeResult result = SanitizeTransformComponent(*transform);
                if (result.changed)
                {
                    ++fixedCount;
                }
            }
        }
        return fixedCount;
    }

    void FocusViewportOnEntity(const AK::Scene& scene, AK::EntityId entity, ViewportCamera& camera)
    {
        if (const AK::TransformComponent* transform = scene.GetWorld().GetTransform(entity))
        {
            AK::EditorSceneViewCamera sceneView = BuildSceneViewCamera(camera);
            AK::FocusEditorSceneViewCamera(sceneView, transform->position.x, transform->position.y, transform->position.z);
            ApplySceneViewCamera(camera, sceneView);
        }
    }

    std::filesystem::path ResolveProjectRoot()
    {
        std::filesystem::path current = std::filesystem::current_path();

        for (;;)
        {
            if (std::filesystem::exists(current / "CMakeLists.txt")
                && std::filesystem::exists(current / "engine")
                && std::filesystem::exists(current / "projects"))
            {
                return current;
            }

            if (!current.has_parent_path() || current.parent_path() == current)
            {
                break;
            }

            current = current.parent_path();
        }

        return std::filesystem::current_path();
    }

    void AddConsoleLine(std::vector<std::string>& lines, std::string line)
    {
        lines.insert(lines.begin(), std::move(line));
        if (lines.size() > 64)
        {
            lines.resize(64);
        }
    }

    AK::EntityId FindEntityByName(const AK::Scene& scene, const std::string& name)
    {
        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            if (entity.name == name)
            {
                return entity.id;
            }
        }

        return AK::InvalidEntity;
    }

    void SyncWorldPositionFromTransform(AK::Scene& scene, AK::EntityId entity)
    {
        AK::TransformComponent* transform = scene.GetWorld().GetTransform(entity);
        if (!transform)
        {
            return;
        }

        SanitizeTransformComponent(*transform);

        AK::WorldPositionComponent& worldPosition = scene.GetWorld().AddWorldPosition(entity);
        worldPosition.position.localX = static_cast<double>(transform->position.x);
        worldPosition.position.localY = static_cast<double>(transform->position.y);
        worldPosition.position.localZ = static_cast<double>(transform->position.z);
        worldPosition.position = AK::NormalizeWorldPosition(worldPosition.position, EditorWorldCellSize);
        worldPosition.authoritative = true;
    }

    void EnsureWorldPositionFromTransform(AK::Scene& scene, AK::EntityId entity)
    {
        const AK::TransformComponent* transform = scene.GetWorld().GetTransform(entity);
        if (!transform || scene.GetWorld().HasWorldPosition(entity))
        {
            return;
        }

        AK::WorldPositionComponent& worldPosition = scene.GetWorld().AddWorldPosition(entity);
        worldPosition.position = AK::MakeWorldPosition(
            static_cast<double>(transform->position.x),
            static_cast<double>(transform->position.y),
            static_cast<double>(transform->position.z),
            EditorWorldCellSize);
        worldPosition.authoritative = true;
    }

    void EnsureWorldPositions(AK::Scene& scene)
    {
        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            EnsureWorldPositionFromTransform(scene, entity.id);
        }
    }

    AK::Frustum3 MakeEditorVisibilityFrustum(const ViewportCamera& camera)
    {
        return AK::MakeOrthographicFrustum(
            camera.centerX - EditorVisibilityHalfExtent,
            camera.centerX + EditorVisibilityHalfExtent,
            -10000.0f,
            10000.0f,
            camera.centerZ - EditorVisibilityHalfExtent,
            camera.centerZ + EditorVisibilityHalfExtent);
    }

    AK::BoundsUpdateStats EnsureSceneBounds(AK::Scene& scene, bool force = false)
    {
        return AK::RebuildSceneBounds(scene.GetWorld(), force);
    }

    AK::VisibilityStats UpdateEditorVisibility(AK::Scene& scene, const ViewportCamera& camera)
    {
        return AK::UpdateFrustumVisibility(scene.GetWorld(), MakeEditorVisibilityFrustum(camera));
    }

    AK::EntityId EnsureEntityWithTransform(AK::Scene& scene, const std::string& name, const AK::Vec3& position)
    {
        AK::EntityId entity = FindEntityByName(scene, name);
        if (entity == AK::InvalidEntity)
        {
            entity = scene.GetWorld().CreateEntity(name);
        }

        AK::TransformComponent& transform = scene.GetWorld().AddTransform(entity);
        transform.position = position;
        EnsureWorldPositionFromTransform(scene, entity);
        return entity;
    }

    std::string LightTypeToText(AK::LightType type)
    {
        switch (type)
        {
            case AK::LightType::Directional:
                return "Directional";
            case AK::LightType::Point:
                return "Point";
            case AK::LightType::Spot:
                return "Spot";
            default:
                return "Unknown";
        }
    }

    std::string BuildComponentTag(const AK::Scene& scene, AK::EntityId entity)
    {
        std::string tag;
        if (scene.GetWorld().HasMesh(entity))
        {
            tag += "M";
        }
        if (scene.GetWorld().HasBounds(entity))
        {
            tag += "B";
        }
        if (scene.GetWorld().HasCamera(entity))
        {
            tag += "C";
        }
        if (scene.GetWorld().HasLight(entity))
        {
            tag += "L";
        }
        return tag.empty() ? "E" : tag;
    }

    AK::EditorViewportItemKind BuildViewportKind(const AK::Scene& scene, AK::EntityId entity)
    {
        if (scene.GetWorld().HasCamera(entity))
        {
            return AK::EditorViewportItemKind::Camera;
        }
        if (scene.GetWorld().HasLight(entity))
        {
            return AK::EditorViewportItemKind::Light;
        }
        if (scene.GetWorld().HasMesh(entity))
        {
            return AK::EditorViewportItemKind::Mesh;
        }
        return AK::EditorViewportItemKind::Entity;
    }

    void EnsureDefaultScene(AK::Scene& scene)
    {
        const AK::EntityId cameraEntity = EnsureEntityWithTransform(scene, "EditorCamera", {0.0f, 1.5f, -5.0f});
        AK::CameraComponent& camera = scene.GetWorld().AddCamera(cameraEntity);
        camera.primary = true;
        camera.verticalFovDegrees = 60.0f;
        camera.nearPlane = 0.05f;
        camera.farPlane = 1000.0f;

        const AK::EntityId cubeEntity = EnsureEntityWithTransform(scene, "SampleCube", {0.0f, 0.0f, 0.0f});
        AK::MeshComponent& cubeMesh = scene.GetWorld().AddMesh(cubeEntity);
        cubeMesh.mesh = "builtin:cube";
        cubeMesh.material = "builtin:default";

        const AK::EntityId lightEntity = EnsureEntityWithTransform(scene, "DirectionalLight", {2.0f, 4.0f, -2.0f});
        AK::LightComponent& light = scene.GetWorld().AddLight(lightEntity);
        light.type = AK::LightType::Directional;
        light.intensity = 3.0f;
        light.color = {1.0f, 0.96f, 0.86f};
    }

    std::string Vec3ToText(const char* label, const AK::Vec3& value)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << label << ": " << value.x << ", " << value.y << ", " << value.z;
        return out.str();
    }

    std::string TransformToText(const AK::TransformComponent* transform)
    {
        if (!transform)
        {
            return "Transform: none";
        }

        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << "P(" << transform->position.x << ", " << transform->position.y << ", " << transform->position.z << ")  "
            << "R(" << transform->rotation.x << ", " << transform->rotation.y << ", " << transform->rotation.z << ")  "
            << "S(" << transform->scale.x << ", " << transform->scale.y << ", " << transform->scale.z << ")";
        return out.str();
    }

    AK::AABB3 BuildEntityPreviewBounds(const AK::TransformComponent& transform)
    {
        const AK::AABB3 localBounds = AK::MakeAABB3FromCenterExtents(
            {0.0f, 0.0f, 0.0f},
            {0.5f, 0.5f, 0.5f});
        return AK::TransformAABB(AK::Mat4FromEulerTransform(ToEulerTransform(transform)), localBounds);
    }

    std::string BoundsToText(const AK::AABB3& bounds)
    {
        std::ostringstream out;
        out << "AABB min(" << AK::ToDebugString(bounds.min, 2) << ") max(" << AK::ToDebugString(bounds.max, 2) << ")";
        return out.str();
    }

    std::string BoundsMetricToText(const AK::AABB3& bounds)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << "AABB size(" << AK::ToDebugString(AK::Size(bounds), 2) << ") area=" << AK::SurfaceArea(bounds);
        return out.str();
    }

    std::string ChernoffCenterSampleToText(const AK::ChernoffProbeResult& probe)
    {
        if (probe.samples.empty())
        {
            return "Chernoff center: no samples";
        }

        const AK::ChernoffSample1D& sample = probe.samples[probe.samples.size() / 2];
        std::ostringstream out;
        out << std::fixed << std::setprecision(5)
            << "R(lambda)g at x=" << sample.x << " -> " << sample.value;
        return out.str();
    }

    std::string ChernoffTailToText(const AK::ChernoffProbeResult& probe)
    {
        std::ostringstream out;
        out << std::scientific << std::setprecision(2)
            << "Laplace tail estimate: " << probe.tailEstimate;
        return out.str();
    }

    std::vector<std::string> BuildInspectorLines(const AK::TransformComponent* transform, const AK::WorldPositionComponent* worldPositionComponent, const AK::CameraComponent* cameraComponent, const AK::LightComponent* lightComponent, const AK::MeshComponent* meshComponent, const AK::BoundsComponent* boundsComponent, const AK::BoundsUpdateStats& boundsStats, const AK::VisibilityStats& visibilityStats, const AK::ResourceStats& resourceStats, const std::string& resourceProbeSummary, const std::string& memoryProbeSummary, const std::string& diagnosticsProbeSummary, const AK::DiagnosticsSnapshot& diagnosticsSnapshot, const std::string& filesystemProbeSummary, const AK::ProjectLayout& projectLayout, const std::string& settingsProbeSummary, const AK::ProjectSettings& projectSettings, const AK::SettingsValidationReport& projectSettingsValidation, const std::string& serializationProbeSummary, const std::string& assetDatabaseProbeSummary, const AK::AssetManifestStats& assetManifestStats, const std::string& packageProbeSummary, const AK::PackageBuildStats& packageStats, const std::string& vfsProbeSummary, const AK::VirtualFileSystemStats& vfsStats, const std::string& streamingProbeSummary, const AK::StreamingStats& streamingStats, const std::string& worldPartitionProbeSummary, const AK::WorldPartitionStats& worldPartitionStats, const AK::WorldPartitionUpdateResult& worldPartitionUpdate, bool renameActive, const std::string& renameBuffer, const ViewportCamera& camera, bool snapEnabled, std::size_t undoDepth, std::size_t redoDepth, const AK::ChernoffProbeResult& mathProbe, const std::string& geometryProbeSummary, const std::string& transformProbeSummary, const std::string& largeWorldProbeSummary, const std::string& worldTopologyProbeSummary, const std::string& gravityProbeSummary, const std::string& terrainProbeSummary, const std::string& splineProbeSummary, const std::string& csgProbeSummary, const std::string& textureSetProbeSummary, const std::string& ntcProbeSummary, const AK::NtcIntegrationPlan& ntcFuturePlan, const std::string& inputProbeSummary, const std::string& commandProbeSummary, const std::string& activeInputSummary, const std::string& jobProbeSummary, const AK::JobSystemStats& jobStats, const AK::RenderFoundationProbe& renderProbe, const AK::FrameTiming& timing)
    {
        std::vector<std::string> lines;

        if (!transform)
        {
            lines.push_back("Transform: none");
            lines.push_back("Select an entity with TransformComponent.");
            return lines;
        }

        lines.push_back("TransformComponent");
        lines.push_back(Vec3ToText("Position", transform->position));
        lines.push_back(Vec3ToText("Rotation", transform->rotation));
        lines.push_back(Vec3ToText("Scale", transform->scale));
        lines.push_back("revision: " + std::to_string(transform->revision) + "  dirty: 0x" + [&]()
        {
            std::ostringstream out;
            out << std::hex << transform->dirtyFlags;
            return out.str();
        }());

        const AK::EulerTransform inspectorTransform = ToEulerTransform(*transform);
        const AK::Mat4 inspectorMatrix = AK::Mat4FromEulerTransform(inspectorTransform);
        lines.push_back(
            std::string("matrix finite: ")
            + (AK::IsFinite(inspectorMatrix) ? "yes" : "no")
            + "  usable: "
            + (AK::IsTransformUsable(inspectorTransform) ? "yes" : "no"));
        lines.push_back(transformProbeSummary);

        if (worldPositionComponent)
        {
            const AK::CameraRelativePosition cameraRelative = AK::ToCameraRelativeFloat(worldPositionComponent->position, AK::WorldPosition{}, {});
            lines.push_back("");
            lines.push_back("Large World Coordinates:");
            lines.push_back(AK::ToDebugString(worldPositionComponent->position, 3));
            lines.push_back("float step estimate: " + std::to_string(cameraRelative.estimatedFloatStepMeters) + " m");
            lines.push_back(std::string("precision risk: ") + (cameraRelative.precisionRisk ? "yes" : "no"));
        }

        if (meshComponent)
        {
            lines.push_back("");
            lines.push_back("MeshComponent");
            lines.push_back("Mesh: " + meshComponent->mesh);
            lines.push_back("Material: " + meshComponent->material);
        }

        if (boundsComponent)
        {
            lines.push_back("");
            lines.push_back("BoundsComponent");
            lines.push_back("Local " + BoundsToText(boundsComponent->localBounds));
            lines.push_back("World " + BoundsToText(boundsComponent->worldBounds));
            lines.push_back("Sphere r: " + std::to_string(boundsComponent->worldSphere.radius));
            lines.push_back("bounds rev: " + std::to_string(boundsComponent->revision) + "  dirty: 0x" + [&]()
            {
                std::ostringstream out;
                out << std::hex << boundsComponent->dirtyFlags;
                return out.str();
            }());
            lines.push_back(std::string("visible: ") + (boundsComponent->visible ? "yes" : "no") + "  culled: " + (boundsComponent->culled ? "yes" : "no"));
        }

        lines.push_back("");
        lines.push_back("Visibility Core:");
        lines.push_back("Bounds: " + AK::ToDebugString(boundsStats));
        lines.push_back("Frustum: " + AK::ToDebugString(visibilityStats));
        lines.push_back("");
        lines.push_back("Resource Core:");
        lines.push_back(AK::ToDebugString(resourceStats));
        lines.push_back("deferred release protects CPU/GPU lifetime");
        lines.push_back(resourceProbeSummary);
        lines.push_back("");
        lines.push_back("Memory Core:");
        lines.push_back(memoryProbeSummary);
        lines.push_back("Rule: frame scratch uses linear arenas; hot paths avoid hidden heap allocation.");
        lines.push_back("");
        lines.push_back("Diagnostics Core:");
        lines.push_back(diagnosticsSnapshot.summary);
        if (!diagnosticsSnapshot.zones.empty())
        {
            lines.push_back("hot zone: " + AK::ToDebugString(diagnosticsSnapshot.zones.front()));
        }
        lines.push_back(diagnosticsProbeSummary);
        lines.push_back("Rule: every heavy subsystem must expose timing/counter diagnostics.");
        lines.push_back("");
        lines.push_back("Filesystem Core:");
        lines.push_back(AK::ToDebugString(projectLayout));
        lines.push_back(filesystemProbeSummary);
        lines.push_back("Rule: project IO uses normalized paths + atomic writes + bounded scans.");
        lines.push_back("");
        lines.push_back("Project Settings Core:");
        lines.push_back(settingsProbeSummary);
        lines.push_back(AK::ToDebugString(projectSettingsValidation));
        lines.push_back("Profile: " + std::string(AK::ToString(projectSettings.buildProfile)));
        lines.push_back("Units: " + AK::ToDebugString(projectSettings.units));
        lines.push_back("Coordinates: " + AK::ToDebugString(projectSettings.coordinates));
        lines.push_back("Runtime: " + AK::ToDebugString(projectSettings.runtime));
        lines.push_back("Rule: units/axis/clip-space/time budgets live in settings, not hardcoded subsystems.");
        lines.push_back("");
        lines.push_back("Serialization Core:");
        lines.push_back(serializationProbeSummary);
        lines.push_back(AK::ToDebugString(AK::BuildSceneSchemaV6()));
        lines.push_back("Rule: every persistent file starts with magic + version and migrates explicitly.");
        lines.push_back("");
        lines.push_back("Asset Database Core:");
        lines.push_back(AK::ToDebugString(assetManifestStats));
        lines.push_back(assetDatabaseProbeSummary);
        lines.push_back("Rule: scenes reference GUIDs; imports produce deterministic cooked targets.");
        lines.push_back("");
        lines.push_back("Package Core:");
        lines.push_back(AK::ToDebugString(packageStats));
        lines.push_back(packageProbeSummary);
        lines.push_back("Rule: runtime reads deterministic .akpak packages, not loose source trees.");
        lines.push_back("");
        lines.push_back("Virtual File System:");
        lines.push_back(AK::ToDebugString(vfsStats));
        lines.push_back(vfsProbeSummary);
        lines.push_back("Rule: runtime asks VFS for GUID/logical path bytes, never raw package offsets.");
        lines.push_back("");
        lines.push_back("Streaming Core:");
        lines.push_back(AK::ToDebugString(streamingStats));
        lines.push_back(streamingProbeSummary);
        lines.push_back("Rule: loading is budgeted through VFS + ResourceManager, not blocking gameplay code.");
        lines.push_back("");
        lines.push_back("World Partition Core:");
        lines.push_back(AK::ToDebugString(worldPartitionStats));
        lines.push_back(AK::ToDebugString(worldPartitionUpdate));
        lines.push_back(worldPartitionProbeSummary);
        lines.push_back("Rule: large worlds stream deterministic cells, not whole maps.");
        lines.push_back("");
        lines.push_back("Render Core:");
        lines.push_back(renderProbe.summary);
        lines.push_back("Depth: " + AK::ToDebugString(renderProbe.depthReport));
        lines.push_back("Graph: " + AK::ToDebugString(renderProbe.graphStats));
        lines.push_back("Rule: CPU double world -> camera-relative GPU float");

        if (cameraComponent)
        {
            lines.push_back("");
            lines.push_back("CameraComponent");
            lines.push_back("FOV: " + std::to_string(static_cast<int>(cameraComponent->verticalFovDegrees)) + " deg");
            lines.push_back("Near/Far: " + std::to_string(cameraComponent->nearPlane) + " / " + std::to_string(cameraComponent->farPlane));
            lines.push_back(std::string("Primary: ") + (cameraComponent->primary ? "yes" : "no"));
        }

        if (lightComponent)
        {
            lines.push_back("");
            lines.push_back("LightComponent");
            lines.push_back("Type: " + LightTypeToText(lightComponent->type));
            lines.push_back("Intensity: " + std::to_string(lightComponent->intensity));
            lines.push_back("Color: " + AK::ToDebugString(lightComponent->color, 2));
        }

        const AK::AABB3 previewBounds = boundsComponent ? boundsComponent->worldBounds : BuildEntityPreviewBounds(*transform);
        lines.push_back("");
        lines.push_back("Geometry Core:");
        lines.push_back(BoundsToText(previewBounds));
        lines.push_back(BoundsMetricToText(previewBounds));
        lines.push_back(geometryProbeSummary);
        lines.push_back("");
        lines.push_back("Large World Core:");
        lines.push_back(largeWorldProbeSummary);
        lines.push_back("CPU world: cell+double, render: camera-relative float");
        lines.push_back("");
        lines.push_back("World Topology Core:");
        lines.push_back(worldTopologyProbeSummary);
        lines.push_back("Modes: planar terrain, spherical planet, toroidal wrap.");
        lines.push_back("Rule: topology selects the world model, gravity samples define local up/down.");
        lines.push_back("");
        lines.push_back("Gravity / Surface Core:");
        lines.push_back(gravityProbeSummary);
        lines.push_back("Fields: uniform, planet, point, spherical zone, zero gravity.");
        lines.push_back("Movement: planar XZ, planet tangent-frame, toroidal wrap surface.");
        lines.push_back("Rule: gameplay/physics must sample local gravity; never assume global -Y.");
        lines.push_back("");
        lines.push_back("Terrain Modes Core:");
        lines.push_back(terrainProbeSummary);
        lines.push_back("Modes: heightfield, planet cube-sphere, voxel, mesh, procedural, toroidal.");
        lines.push_back("Rule: terrain is a policy layer; never assume one flat heightmap fits all worlds.");
        lines.push_back("");
        lines.push_back("Spline Core:");
        lines.push_back(splineProbeSummary);
        lines.push_back("Uses: roads, rivers, rails, cables, camera rails, patrol paths, terrain deformation.");
        lines.push_back("Rule: paths are authored as splines with frames/length/closest-point queries, not gameplay arrays.");
        lines.push_back("");
        lines.push_back("CSG / Destruction Boolean Core:");
        lines.push_back(csgProbeSummary);
        lines.push_back("Modes: primitive SDF booleans, mesh odd-even voxelization, voxel boolean combine.");
        lines.push_back("Physics: result voxels extract greedy AABB collision proxies for broadphase/destruction.");
        lines.push_back("Rule: runtime destruction uses bounded voxel CSG + proxy rebuild; high-quality render remesh is an offline/future stage.");
        lines.push_back("");
        lines.push_back("Texture Set Core:");
        lines.push_back(textureSetProbeSummary);
        lines.push_back("Rule: materials are imported as aligned texture sets, not unrelated single images.");
        lines.push_back("");
        lines.push_back("Neural Texture Compression Core:");
        lines.push_back(ntcProbeSummary);
        lines.push_back(AK::ToDebugString(ntcFuturePlan));
        lines.push_back("Stages: foundation -> SDK cooker bridge -> Vulkan detection -> shader decode -> temporal filtering -> material policy.");
        lines.push_back("Rule: NTC is optional and must keep BCn fallback; sample-mode decode waits for Vulkan/shader/TAA support.");
        lines.push_back("");
        lines.push_back("Time Core:");
        lines.push_back("frame: " + std::to_string(timing.frameIndex) + "  tick: " + std::to_string(timing.fixedTick));
        lines.push_back("dt: " + std::to_string(timing.frameDeltaSeconds * 1000.0) + " ms  fixed: " + std::to_string(timing.fixedDeltaSeconds * 1000.0) + " ms");
        lines.push_back("fixed steps: " + std::to_string(timing.fixedStepsThisFrame) + "  pending: " + std::to_string(timing.pendingFixedSteps));
        lines.push_back("alpha: " + std::to_string(timing.interpolationAlpha) + "  dropped: " + std::to_string(timing.droppedFixedSteps));
        lines.push_back(std::string("delta clamp: ") + (timing.frameDeltaClamped ? "yes" : "no"));
        lines.push_back("");
        lines.push_back("Input Core:");
        lines.push_back(inputProbeSummary);
        lines.push_back(activeInputSummary);
        lines.push_back("Actions are routed through InputMap/Context, not hardwired UI keys.");
        lines.push_back("");
        lines.push_back("Command Core:");
        lines.push_back(commandProbeSummary);
        lines.push_back("Shortcuts/toolbars route to CommandId before editor execution.");
        lines.push_back("");
        lines.push_back("Job Core:");
        lines.push_back(AK::ToDebugString(jobStats));
        lines.push_back(jobProbeSummary);
        lines.push_back("Rule: parallel work must have explicit sync points.");
        lines.push_back("");
        lines.push_back("Math Core:");
        lines.push_back("Chernoff/Laplace resolvent probe active");
        lines.push_back(ChernoffCenterSampleToText(mathProbe));
        lines.push_back(ChernoffTailToText(mathProbe));
        lines.push_back("");

        if (renameActive)
        {
            lines.push_back("Rename mode: " + renameBuffer + "_");
            lines.push_back("Type name, Enter: apply, Esc: cancel");
        }
        else
        {
            lines.push_back("Editing:");
            lines.push_back("Viewport click: select entity");
            lines.push_back("LMB drag: move selected entity in X/Z");
            lines.push_back("RMB/MMB drag: pan viewport");
            lines.push_back("Mouse wheel: zoom viewport");
            lines.push_back(std::string("G: grid snap ") + (snapEnabled ? "ON" : "OFF"));
            lines.push_back("F: focus selected, Home: reset view, Window/Scene View: frame and axis snaps");
            lines.push_back("Ctrl+D: duplicate selected entity");
            lines.push_back("Ctrl+Z: undo (" + std::to_string(undoDepth) + ")");
            lines.push_back("Ctrl+Y: redo (" + std::to_string(redoDepth) + ")");
            lines.push_back(std::string("Scene View: ") + AK::ToString(camera.sceneView.mode) + " / " + AK::ToString(camera.sceneView.projection));
            lines.push_back("View: X=" + std::to_string(camera.centerX) + " Z=" + std::to_string(camera.centerZ));
            lines.push_back("Zoom: " + std::to_string(static_cast<int>(camera.scale)) + " px/unit");
            if (camera.sceneView.mode == AK::EditorSceneViewMode::Mode3D)
            {
                lines.push_back("3D Camera: X=" + std::to_string(camera.sceneView.positionX) + " Y=" + std::to_string(camera.sceneView.positionY) + " Z=" + std::to_string(camera.sceneView.positionZ));
                lines.push_back("3D Look: yaw=" + std::to_string(camera.sceneView.yawDegrees) + " pitch=" + std::to_string(camera.sceneView.pitchDegrees));
            }
            lines.push_back("WASD: move X/Z, Q/E: move Y");
            lines.push_back("Shift: faster transform step");
            lines.push_back("Left/Right: rotate Y");
            lines.push_back("I/K: rotate X, J: rotate Z");
            lines.push_back("+/-: uniform scale");
            lines.push_back("F2: rename selected entity");
            lines.push_back("Delete twice: confirmed delete");
        }

        return lines;
    }

    std::vector<std::string> BuildHierarchyItems(const AK::Scene& scene, AK::EntityId selected)
    {
        std::vector<std::string> items;
        items.reserve(scene.GetWorld().Entities().size());

        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            items.push_back((entity.id == selected ? "> " : "  ") + std::string("[") + BuildComponentTag(scene, entity.id) + "] " + entity.name + "  #" + AK::ToString(entity.id));
        }

        return items;
    }

    std::vector<std::string> BuildAssetItems(const AK::AssetRegistry& registry)
    {
        std::vector<std::string> items;
        items.reserve(registry.Assets().size());

        for (const AK::AssetRecord& asset : registry.Assets())
        {
            items.push_back(asset.type + "  " + AK::ToString(asset.guid) + "  " + asset.path.generic_string());
        }

        return items;
    }


    AK::EditorConsoleSeverity ConsoleSeverityFromText(const std::string& line)
    {
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        if (lower.find("error") != std::string::npos || lower.find("failed") != std::string::npos || lower.find("fail") != std::string::npos)
        {
            return AK::EditorConsoleSeverity::Error;
        }
        if (lower.find("warning") != std::string::npos || lower.find("repaired") != std::string::npos || lower.find("missing") != std::string::npos)
        {
            return AK::EditorConsoleSeverity::Warning;
        }
        return AK::EditorConsoleSeverity::Info;
    }

    AK::EditorPanelModelFrame BuildLiveDiagnosticsPanelFrame(const std::vector<std::string>& consoleLines,
                                                             const AK::DiagnosticsSnapshot& snapshot,
                                                             const AK::JobSystemStats& jobStats,
                                                             const AK::StreamingStats& streamingStats,
                                                             const AK::WorldPartitionStats& partitionStats,
                                                             const AK::ResourceStats& resourceStats,
                                                             AK::u64 frameIndex)
    {
        AK::EditorPanelModelFrame frame{};
        frame.console.entries.reserve(consoleLines.size());
        for (const std::string& line : consoleLines)
        {
            AK::EditorConsoleEntry entry{};
            entry.severity = ConsoleSeverityFromText(line);
            entry.channel = "Editor";
            entry.message = line;
            entry.frame = frameIndex;
            frame.console.entries.push_back(std::move(entry));
        }

        auto pushMetric = [&](std::string key, std::string label, double value, AK::EditorMetricUnit unit, bool warning)
        {
            AK::EditorMetric metric{};
            metric.key = std::move(key);
            metric.label = std::move(label);
            metric.value = value;
            metric.unit = unit;
            metric.warning = warning;
            frame.diagnostics.metrics.push_back(std::move(metric));
        };

        pushMetric("frame.ms", "Frame ms", snapshot.frameMilliseconds, AK::EditorMetricUnit::Milliseconds, snapshot.frameMilliseconds > 18.0);
        pushMetric("zones", "Profile zones", static_cast<double>(snapshot.zones.size()), AK::EditorMetricUnit::Count, false);
        pushMetric("events", "Diagnostic events", static_cast<double>(snapshot.recentEvents.size()), AK::EditorMetricUnit::Count, snapshot.recentEvents.size() > 96);
        pushMetric("jobs.pending", "Pending jobs", static_cast<double>(jobStats.pendingJobs), AK::EditorMetricUnit::Count, jobStats.pendingJobs > 0);
        pushMetric("streaming.queued", "Streaming queued", static_cast<double>(streamingStats.queued), AK::EditorMetricUnit::Count, streamingStats.queued > 0);
        pushMetric("partition.active", "Active cells", static_cast<double>(partitionStats.active), AK::EditorMetricUnit::Count, false);
        pushMetric("resources.resident", "Resident resources", static_cast<double>(resourceStats.resident), AK::EditorMetricUnit::Count, false);

        frame.diagnostics.frameIndex = frameIndex;
        for (const AK::DiagnosticEvent& event : snapshot.recentEvents)
        {
            frame.diagnostics.recentEvents.push_back(event.category + ": " + event.message);
        }
        frame.revision = frameIndex + 1;
        return frame;
    }

    void FillRenderDiagnosticsFromView(AK::EditorFrameDesc& frame, const AK::EditorDiagnosticsViewState& view)
    {
        frame.diagnosticsStatus = view.summary;
        frame.diagnosticsUnreadCount = view.unreadCount;
        frame.diagnosticsWarningCount = view.warningCount;
        frame.diagnosticsErrorCount = view.errorCount;
        frame.diagnosticsActiveTaskCount = view.activeTaskCount;

        frame.diagnosticsMetrics.clear();
        frame.diagnosticsMetrics.reserve(view.metrics.size());
        for (const AK::EditorDiagnosticsViewMetricTile& metric : view.metrics)
        {
            AK::EditorDiagnosticsMetricRenderDesc renderMetric{};
            renderMetric.label = metric.label;
            renderMetric.value = metric.valueText;
            renderMetric.source = metric.source;
            renderMetric.severity = AK::ToString(metric.severity);
            renderMetric.pinned = metric.pinned;
            frame.diagnosticsMetrics.push_back(std::move(renderMetric));
        }

        frame.diagnosticsRows.clear();
        frame.diagnosticsRows.reserve(view.rows.size());
        for (const AK::EditorDiagnosticsViewRowSurface& row : view.rows)
        {
            AK::EditorDiagnosticsRowRenderDesc renderRow{};
            renderRow.icon = row.icon;
            renderRow.severity = AK::ToString(row.severity);
            renderRow.kind = AK::ToString(row.kind);
            renderRow.source = row.sourceLabel;
            renderRow.title = row.title;
            renderRow.subtitle = row.subtitle;
            renderRow.message = row.title;
            renderRow.detail = row.detail;
            renderRow.badges = row.badges;
            renderRow.unread = row.unread;
            renderRow.sticky = row.sticky;
            renderRow.active = row.active;
            renderRow.selected = row.selected;
            frame.diagnosticsRows.push_back(std::move(renderRow));
        }
    }

    std::vector<AK::EditorViewportItem> BuildViewportItems(const AK::Scene& scene, AK::EntityId selected)
    {
        std::vector<AK::EditorViewportItem> items;
        items.reserve(scene.GetWorld().Entities().size());

        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            const AK::TransformComponent* transform = scene.GetWorld().GetTransform(entity.id);
            if (!transform)
            {
                continue;
            }

            AK::EditorViewportItem item{};
            item.name = entity.name;
            item.x = transform->position.x;
            item.y = transform->position.y;
            item.z = transform->position.z;
            item.selected = entity.id == selected;
            item.kind = BuildViewportKind(scene, entity.id);
            if (const AK::BoundsComponent* bounds = scene.GetWorld().GetBounds(entity.id))
            {
                item.hasBounds = AK::IsValid(bounds->worldBounds);
                item.culled = bounds->culled;
                item.minX = bounds->worldBounds.min.x;
                item.minZ = bounds->worldBounds.min.z;
                item.maxX = bounds->worldBounds.max.x;
                item.maxZ = bounds->worldBounds.max.z;
            }
            items.push_back(std::move(item));
        }

        return items;
    }

    const AK::EntityRecord* FindEntityRecord(const AK::Scene& scene, AK::EntityId id)
    {
        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            if (entity.id == id)
            {
                return &entity;
            }
        }

        return nullptr;
    }

    std::size_t EntityIndexOf(const AK::Scene& scene, AK::EntityId id)
    {
        const auto& entities = scene.GetWorld().Entities();
        for (std::size_t i = 0; i < entities.size(); ++i)
        {
            if (entities[i].id == id)
            {
                return i;
            }
        }

        return entities.empty() ? 0 : entities.size() - 1;
    }

    AK::EntityId EntityAtIndex(const AK::Scene& scene, std::size_t index)
    {
        const auto& entities = scene.GetWorld().Entities();
        if (entities.empty())
        {
            return AK::InvalidEntity;
        }

        index = std::min(index, entities.size() - 1);
        return entities[index].id;
    }

    SceneSnapshot CaptureSceneSnapshot(const AK::Scene& scene, AK::EntityId selected)
    {
        SceneSnapshot snapshot{};
        snapshot.selectedIndex = EntityIndexOf(scene, selected);

        const AK::EntityRecord* selectedRecord = FindEntityRecord(scene, selected);
        if (selectedRecord)
        {
            snapshot.selectedName = selectedRecord->name;
        }

        snapshot.entities.reserve(scene.GetWorld().Entities().size());
        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            const AK::TransformComponent* transform = scene.GetWorld().GetTransform(entity.id);
            if (!transform)
            {
                continue;
            }

            EntitySnapshot entitySnapshot{};
            entitySnapshot.name = entity.name;
            entitySnapshot.transform = *transform;

            if (const AK::WorldPositionComponent* worldPosition = scene.GetWorld().GetWorldPosition(entity.id))
            {
                entitySnapshot.hasWorldPosition = true;
                entitySnapshot.worldPosition = *worldPosition;
            }

            if (const AK::BoundsComponent* bounds = scene.GetWorld().GetBounds(entity.id))
            {
                entitySnapshot.hasBounds = true;
                entitySnapshot.bounds = *bounds;
            }

            if (const AK::CameraComponent* camera = scene.GetWorld().GetCamera(entity.id))
            {
                entitySnapshot.hasCamera = true;
                entitySnapshot.camera = *camera;
            }

            if (const AK::LightComponent* light = scene.GetWorld().GetLight(entity.id))
            {
                entitySnapshot.hasLight = true;
                entitySnapshot.light = *light;
            }

            if (const AK::MeshComponent* mesh = scene.GetWorld().GetMesh(entity.id))
            {
                entitySnapshot.hasMesh = true;
                entitySnapshot.mesh = *mesh;
            }

            snapshot.entities.push_back(std::move(entitySnapshot));
        }

        return snapshot;
    }

    void RestoreSceneSnapshot(AK::Scene& scene, const SceneSnapshot& snapshot, AK::EntityId& selected)
    {
        scene.GetWorld().Clear();

        selected = AK::InvalidEntity;
        std::size_t createdIndex = 0;
        for (const EntitySnapshot& entitySnapshot : snapshot.entities)
        {
            const AK::EntityId entity = scene.GetWorld().CreateEntity(entitySnapshot.name);
            scene.GetWorld().AddTransform(entity) = entitySnapshot.transform;
            if (entitySnapshot.hasWorldPosition)
            {
                scene.GetWorld().AddWorldPosition(entity) = entitySnapshot.worldPosition;
            }
            else
            {
                EnsureWorldPositionFromTransform(scene, entity);
            }
            if (entitySnapshot.hasBounds)
            {
                scene.GetWorld().AddBounds(entity) = entitySnapshot.bounds;
            }
            if (entitySnapshot.hasCamera)
            {
                scene.GetWorld().AddCamera(entity) = entitySnapshot.camera;
            }
            if (entitySnapshot.hasLight)
            {
                scene.GetWorld().AddLight(entity) = entitySnapshot.light;
            }
            if (entitySnapshot.hasMesh)
            {
                scene.GetWorld().AddMesh(entity) = entitySnapshot.mesh;
            }

            if (!snapshot.selectedName.empty() && entitySnapshot.name == snapshot.selectedName)
            {
                selected = entity;
            }
            else if (selected == AK::InvalidEntity && createdIndex == snapshot.selectedIndex)
            {
                selected = entity;
            }

            ++createdIndex;
        }

        SanitizeSceneTransforms(scene);

        if (selected == AK::InvalidEntity)
        {
            selected = EntityAtIndex(scene, snapshot.selectedIndex);
        }
    }

    AK::EntityId SelectRelativeEntity(const AK::Scene& scene, AK::EntityId current, int delta)
    {
        const auto& entities = scene.GetWorld().Entities();
        if (entities.empty())
        {
            return AK::InvalidEntity;
        }

        const std::size_t currentIndex = EntityIndexOf(scene, current);
        std::size_t nextIndex = currentIndex;
        if (delta < 0)
        {
            const std::size_t amount = static_cast<std::size_t>(-(delta + 1)) + 1;
            nextIndex = amount >= currentIndex ? 0 : currentIndex - amount;
        }
        else if (delta > 0)
        {
            const std::size_t maxIndex = entities.size() - 1;
            const std::size_t amount = static_cast<std::size_t>(delta);
            nextIndex = amount >= maxIndex - currentIndex ? maxIndex : currentIndex + amount;
        }

        return entities[nextIndex].id;
    }

    ToolbarAction HitTestToolbar(const AK::Window& window)
    {
        const AK::WindowInput& input = window.Input();
        if (!input.mouseLeftPressed)
        {
            return ToolbarAction::None;
        }

        const int y = 20;
        const int h = 16;
        int x = 12;
        const Rect newButton{x, y, x + 58, y + h};
        x += 64;
        const Rect meshButton{x, y, x + 58, y + h};
        x += 64;
        const Rect cameraButton{x, y, x + 74, y + h};
        x += 80;
        const Rect lightButton{x, y, x + 58, y + h};
        x += 64;
        const Rect deleteButton{x, y, x + 68, y + h};
        x += 74;
        const Rect saveButton{x, y, x + 58, y + h};
        x += 64;
        const Rect loadButton{x, y, x + 58, y + h};
        x += 64;
        const Rect rescanButton{x, y, x + 74, y + h};

        if (Contains(newButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::NewEntity;
        }
        if (Contains(meshButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::NewMesh;
        }
        if (Contains(cameraButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::NewCamera;
        }
        if (Contains(lightButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::NewLight;
        }
        if (Contains(deleteButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::DeleteEntity;
        }
        if (Contains(saveButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::SaveScene;
        }
        if (Contains(loadButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::LoadScene;
        }
        if (Contains(rescanButton, input.mouseX, input.mouseY))
        {
            return ToolbarAction::RescanAssets;
        }

        return ToolbarAction::None;
    }

    AK::CommandId CommandFromToolbarAction(ToolbarAction action)
    {
        switch (action)
        {
            case ToolbarAction::NewEntity:
                return AK::CommandId::NewEntity;
            case ToolbarAction::NewMesh:
                return AK::CommandId::NewMesh;
            case ToolbarAction::NewCamera:
                return AK::CommandId::NewCamera;
            case ToolbarAction::NewLight:
                return AK::CommandId::NewLight;
            case ToolbarAction::DeleteEntity:
                return AK::CommandId::DeleteSelection;
            case ToolbarAction::SaveScene:
                return AK::CommandId::SaveScene;
            case ToolbarAction::LoadScene:
                return AK::CommandId::LoadScene;
            case ToolbarAction::RescanAssets:
                return AK::CommandId::RescanAssets;
            case ToolbarAction::None:
            default:
                return AK::CommandId::Count;
        }
    }

    AK::EntityId HitTestHierarchy(const AK::Window& window, const AK::Scene& scene, const EditorLayout& layout, int hierarchyScrollPixels)
    {
        const AK::WindowInput& input = window.Input();
        if (!input.mouseLeftPressed)
        {
            return AK::InvalidEntity;
        }

        constexpr int hierarchyHeaderHeight = 32;
        constexpr int hierarchyVisualRowHeight = 22;
        const int firstEntityRowTop = layout.hierarchy.top + hierarchyHeaderHeight + hierarchyVisualRowHeight;

        if (!Contains(layout.hierarchy, input.mouseX, input.mouseY) || input.mouseY < firstEntityRowTop)
        {
            return AK::InvalidEntity;
        }

        const int row = (input.mouseY - firstEntityRowTop + std::max(0, hierarchyScrollPixels)) / hierarchyVisualRowHeight;
        const int visibleRows = std::max(1, (layout.hierarchy.bottom - firstEntityRowTop) / hierarchyVisualRowHeight);
        const int maxRows = std::min<int>(visibleRows + std::max(0, hierarchyScrollPixels) / hierarchyVisualRowHeight + 1, static_cast<int>(scene.GetWorld().Entities().size()));
        if (row < 0 || row >= maxRows)
        {
            return AK::InvalidEntity;
        }

        return scene.GetWorld().Entities()[static_cast<std::size_t>(row)].id;
    }

    AK::EntityId HitTestViewport(const AK::Window& window, const AK::Scene& scene, const ViewportCamera& camera, const EditorLayout& layout)
    {
        const AK::WindowInput& input = window.Input();
        if (!input.mouseLeftPressed)
        {
            return AK::InvalidEntity;
        }

        if (!Contains(layout.viewportGrid, input.mouseX, input.mouseY))
        {
            return AK::InvalidEntity;
        }

        const int centerX = (layout.viewportGrid.left + layout.viewportGrid.right) / 2;
        const int centerY = (layout.viewportGrid.top + layout.viewportGrid.bottom) / 2;
        const float scale = std::clamp(camera.scale, MinViewportScale, MaxViewportScale);

        AK::EntityId nearest = AK::InvalidEntity;
        float nearestDistanceSq = 18.0f * 18.0f;

        for (const AK::EntityRecord& entity : scene.GetWorld().Entities())
        {
            const AK::TransformComponent* transform = scene.GetWorld().GetTransform(entity.id);
            if (!transform)
            {
                continue;
            }

            const int x = centerX + static_cast<int>(std::round((transform->position.x - camera.centerX) * scale));
            const int y = centerY - static_cast<int>(std::round((transform->position.z - camera.centerZ) * scale));
            const float dx = static_cast<float>(input.mouseX - x);
            const float dy = static_cast<float>(input.mouseY - y);
            const float distanceSq = dx * dx + dy * dy;

            if (distanceSq < nearestDistanceSq)
            {
                nearestDistanceSq = distanceSq;
                nearest = entity.id;
            }
        }

        return nearest;
    }

    std::uint32_t BuildNextEntitySerial(const AK::Scene& scene)
    {
        std::uint32_t serial = 1;
        while (FindEntityByName(scene, "Entity_" + std::to_string(serial)) != AK::InvalidEntity)
        {
            ++serial;
        }
        return serial;
    }

    std::string BuildUniqueDuplicateName(const AK::Scene& scene, const std::string& baseName)
    {
        std::uint32_t serial = 1;
        for (;;)
        {
            const std::string candidate = baseName + "_Copy" + std::to_string(serial);
            if (FindEntityByName(scene, candidate) == AK::InvalidEntity)
            {
                return candidate;
            }
            ++serial;
        }
    }

    bool IsPrintableNameCharacter(char value)
    {
        const unsigned char c = static_cast<unsigned char>(value);
        return c >= 32 && c < 127 && value != '"';
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    AK::LogInfo("AK Editor starting");

    const std::filesystem::path projectRoot = ResolveProjectRoot();
    AK::LogInfo("Project root: " + projectRoot.string());

    const AK::ProjectLayout projectLayout = AK::BuildProjectLayout(projectRoot);
    const std::string filesystemProbeSummary = AK::BuildFilesystemProbeSummary();
    const std::filesystem::path projectSettingsPath = projectRoot / "projects" / "Sandbox" / "project.aksettings";
    AK::ProjectSettings projectSettings = AK::MakeDefaultProjectSettings();
    AK::Result<AK::ProjectSettings> loadedProjectSettings = AK::LoadProjectSettings(projectSettingsPath);
    if (loadedProjectSettings)
    {
        projectSettings = loadedProjectSettings.Value();
    }
    const AK::SettingsValidationReport projectSettingsValidation = AK::ValidateProjectSettings(projectSettings);
    const AK::Result<void> startupSettingsWrite = AK::SaveProjectSettingsAtomic(projectSettingsPath, projectSettings);
    const std::string settingsProbeSummary = AK::BuildSettingsProbeSummary();
    const std::string serializationProbeSummary = AK::BuildSerializationProbeSummary();

    AK::Window window({"AK Engine Editor v10.6", 1366, 768});
    if (!window.IsOpen())
    {
        AK::LogError("Editor window failed to open");
        return 1;
    }

    AK::Renderer renderer;
    if (!renderer.Initialize({"AK Editor"}, window))
    {
        AK::LogError("Renderer failed to initialize");
        return 1;
    }

    AK::Scene scene("Sandbox");
    const std::filesystem::path sandboxScenePath = projectRoot / "projects" / "Sandbox" / "scene.akscene";
    const std::filesystem::path autosavePath = projectRoot / "projects" / "Sandbox" / "autosave.akscene";

    if (std::filesystem::exists(autosavePath))
    {
        scene.LoadFromFile(autosavePath);
    }
    else if (std::filesystem::exists(sandboxScenePath))
    {
        scene.LoadFromFile(sandboxScenePath);
    }

    EnsureDefaultScene(scene);
    const std::size_t startupSanitizedTransforms = SanitizeSceneTransforms(scene);
    EnsureWorldPositions(scene);
    AK::BoundsUpdateStats startupBoundsStats = EnsureSceneBounds(scene, true);

    AK::EntityId selectedEntity = FindEntityByName(scene, "SampleCube");
    if (selectedEntity == AK::InvalidEntity)
    {
        selectedEntity = EntityAtIndex(scene, 0);
    }

    std::uint32_t nextEntitySerial = BuildNextEntitySerial(scene);

    AK::AssetRegistry registry;
    registry.ScanDirectory(projectRoot / "assets");
    AK::AssetManifest assetManifest = AK::BuildAssetManifest(projectLayout);
    const AK::Result<void> startupManifestWrite = AK::WriteAssetManifestText(assetManifest.manifestPath, assetManifest);
    const AK::Result<AK::PackageBuildResult> startupPackageWrite = AK::WriteAssetPackage(AK::BuildDefaultPackagePath(projectLayout, "sandbox"), projectLayout, assetManifest);
    AK::PackageBuildResult startupPackage{};
    if (startupPackageWrite)
    {
        startupPackage = startupPackageWrite.Value();
    }
    const std::string assetDatabaseProbeSummary = AK::BuildAssetDatabaseProbeSummary();
    const std::string packageProbeSummary = AK::BuildPackageProbeSummary();

    AK::VirtualFileSystem startupVfs;
    AK::Result<void> startupVfsPackageMount = startupPackageWrite
        ? startupVfs.MountPackage("sandbox-pak", startupPackage.packagePath)
        : AK::Result<void>(AK::MakeError(AK::ErrorCode::InvalidState, "startup package was not written"));
    AK::AssetImportPolicy startupVfsImportPolicy{};
    startupVfsImportPolicy.hashSourceContents = true;
    AK::Result<void> startupVfsLooseMount = startupVfs.MountLooseDirectory("assets-loose", projectLayout, startupVfsImportPolicy);
    const AK::VirtualFileSystemStats startupVfsStats = startupVfs.Stats();
    const std::string vfsProbeSummary = AK::BuildVirtualFileSystemProbeSummary();
    const std::string streamingProbeSummary = AK::BuildStreamingProbeSummary();
    const std::string worldPartitionProbeSummary = AK::BuildWorldPartitionProbeSummary();

    AK::WorldPartitionConfig worldPartitionConfig{};
    worldPartitionConfig.cellSizeMeters = EditorWorldCellSize;
    worldPartitionConfig.activeRadiusCells = 1;
    worldPartitionConfig.preloadRadiusCells = 2;
    worldPartitionConfig.unloadRadiusCells = 4;
    worldPartitionConfig.maxCellsActivatedPerUpdate = 8;
    worldPartitionConfig.maxCellsEvictedPerUpdate = 8;
    AK::WorldPartition editorWorldPartition(worldPartitionConfig);
    std::size_t partitionAssetIndex = 0;
    for (const AK::AssetManifestRecord& record : assetManifest.records)
    {
        const AK::i64 x = static_cast<AK::i64>(partitionAssetIndex % 5u) - 2;
        const AK::i64 z = static_cast<AK::i64>((partitionAssetIndex / 5u) % 5u) - 2;
        AK::WorldPartitionAssetRef ref{};
        ref.guid = record.guid;
        ref.logicalPath = record.sourcePath.generic_string();
        ref.estimatedBytes = record.sourceFingerprint.sizeBytes;
        editorWorldPartition.AddAssetRef({x, z, 0}, std::move(ref));
        ++partitionAssetIndex;
    }

    const AK::ChernoffProbeResult mathProbe = AK::BuildDefaultChernoffProbe();
    const std::string geometryProbeSummary = AK::BuildGeometryProbeSummary();
    const std::string largeWorldProbeSummary = AK::BuildLargeWorldProbeSummary();
    const std::string worldTopologyProbeSummary = AK::BuildWorldTopologyProbeSummary();
    const std::string gravityProbeSummary = AK::BuildGravityProbeSummary();
    const std::string terrainProbeSummary = AK::BuildTerrainProbeSummary();
    const std::string splineProbeSummary = AK::BuildSplineProbeSummary();
    const std::string csgProbeSummary = AK::BuildCsgProbeSummary();
    const std::string textureSetProbeSummary = AK::BuildTextureSetProbeSummary();
    const AK::NtcProbeResult ntcProbe = AK::BuildNtcProbe();
    const std::string ntcProbeSummary = ntcProbe.summary;
    const std::string transformProbeSummary = AK::BuildTransformProbeSummary();
    const std::string boundsProbeSummary = AK::BuildBoundsVisibilityProbeSummary();
    const std::string resourceProbeSummary = AK::BuildResourceProbeSummary();
    const std::string memoryProbeSummary = AK::BuildMemoryProbeSummary();
    const std::string diagnosticsProbeSummary = AK::BuildDiagnosticsProbeSummary();
    const AK::InputMap editorInputMap = AK::BuildDefaultEditorInputMap();
    const AK::InputDiagnostics inputDiagnostics = AK::BuildInputDiagnostics(editorInputMap);
    const std::string inputProbeSummary = inputDiagnostics.summary;
    const AK::CommandRegistry commandRegistry = AK::BuildDefaultEditorCommandRegistry();
    const AK::CommandDiagnostics commandDiagnostics = AK::BuildCommandDiagnostics(commandRegistry, editorInputMap);
    const std::string commandProbeSummary = commandDiagnostics.summary;
    AK::EditorRuntimeBridge editorRuntimeBridge = AK::BuildDefaultEditorRuntimeBridge(static_cast<AK::i32>(window.Width()), static_cast<AK::i32>(window.Height()));
    AK::EditorWorkspacePaths editorWorkspacePaths = AK::BuildEditorWorkspacePaths(projectRoot.string());
    AK::EditorWorkspaceLoadResult editorWorkspaceLoad = AK::LoadEditorWorkspace(editorWorkspacePaths, editorRuntimeBridge.panels, static_cast<AK::i32>(window.Width()), static_cast<AK::i32>(window.Height()));
    editorWorkspacePaths = editorWorkspaceLoad.paths;
    AK::EditorAutosavePolicy editorAutosavePolicy = AK::BuildEditorAutosavePolicyFromPreferences(editorWorkspaceLoad.preferences.profile);
    AK::EditorAutosavePaths editorAutosavePaths = AK::BuildEditorAutosavePaths(editorWorkspacePaths, (projectRoot / "projects" / "Sandbox" / "autosave.akscene").string());
    AK::EditorAutosaveRuntimeState editorAutosaveState = AK::MakeDefaultEditorAutosaveRuntimeState();
    const std::string editorAutosaveProbeSummary = AK::BuildEditorAutosaveProbeSummary();
    AK::EditorActivityCenterState editorActivityCenter = AK::MakeDefaultEditorActivityCenterState();
    const AK::EditorActivityCenterPolicy editorActivityPolicy = AK::MakeDefaultEditorActivityCenterPolicy();
    const AK::EditorActivityDiagnostics editorActivityDiagnostics = AK::RunEditorActivityDiagnostics();
    const std::string editorActivityProbeSummary = AK::BuildEditorActivityProbeSummary();
    const std::string editorDiagnosticsPanelProbeSummary = AK::BuildEditorDiagnosticsPanelProbeSummary();
    AK::EditorRuntimeDiagnostics editorRuntimeDiagnostics = AK::ValidateEditorRuntimeBridge(editorRuntimeBridge);
    AK::EditorUXRuntime editorUXRuntime{};
    AK::EditorUXRuntimeDiagnostics editorUXDiagnostics{};
    AK::EditorFocusState editorFocusState = AK::MakeDefaultEditorFocusState();
    AK::EditorFocusDiagnostics editorFocusDiagnostics = AK::ValidateEditorFocusState(editorFocusState);
    const std::string editorFocusProbeSummary = AK::BuildEditorFocusProbeSummary();
    const AK::EditorDragDropDiagnostics editorDragDropDiagnostics = AK::RunEditorDragDropDiagnostics();
    const std::string editorDragDropProbeSummary = AK::BuildEditorDragDropProbeSummary();
    const std::string editorCommandStateProbeSummary = AK::BuildEditorCommandStateProbeSummary();
    AK::EditorCommandPaletteRuntimeState editorCommandPaletteRuntime = AK::MakeDefaultEditorCommandPaletteRuntimeState();
    const std::string editorCommandPaletteRuntimeProbeSummary = AK::BuildEditorCommandPaletteRuntimeProbeSummary();
    AK::EditorMenuToolbarRuntimeState editorMenuToolbarRuntime = AK::MakeDefaultEditorMenuToolbarRuntimeState();
    const std::string editorMenuToolbarRuntimeProbeSummary = AK::BuildEditorMenuToolbarRuntimeProbeSummary();
    AK::u32 editorRuntimeWidth = window.Width();
    AK::u32 editorRuntimeHeight = window.Height();
    const std::string jobProbeSummary = AK::BuildJobSystemProbeSummary();

    AK::DiagnosticsHub diagnosticsHub(128);
    diagnosticsHub.AddEvent(AK::DiagnosticSeverity::Info, "Editor", "diagnostics core initialized");

    AK::JobSystemConfig editorJobConfig{};
    editorJobConfig.workerCount = std::min<AK::u32>(4, std::max<AK::u32>(1, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1));
    editorJobConfig.maxWorkerCount = 4;
    AK::JobSystem editorJobSystem(editorJobConfig);

    std::vector<std::string> consoleLines;
    AK::EditorActivityPostDesc startupActivity{};
    startupActivity.source = AK::EditorActivitySource::Editor;
    startupActivity.severity = AK::EditorActivitySeverity::Success;
    startupActivity.title = "Editor started";
    startupActivity.message = "Workspace, autosave and activity center are ready.";
    AK::PostEditorActivityNotification(editorActivityCenter, editorActivityPolicy, startupActivity, 0.0);

    AddConsoleLine(consoleLines, "Renderer backend: layout-driven Win32/GDI bridge");
    AddConsoleLine(consoleLines, "EditorUI runtime: " + editorRuntimeDiagnostics.summary);
    AddConsoleLine(consoleLines, diagnosticsProbeSummary);
    AddConsoleLine(consoleLines, filesystemProbeSummary);
    AddConsoleLine(consoleLines, "Project layout: " + AK::ToDebugString(projectLayout));
    AddConsoleLine(consoleLines, settingsProbeSummary);
    AddConsoleLine(consoleLines, serializationProbeSummary);
    AddConsoleLine(consoleLines, "Project settings: " + AK::ToDebugString(projectSettings));
    AddConsoleLine(consoleLines, AK::ToDebugString(projectSettingsValidation));
    if (!loadedProjectSettings)
    {
        AddConsoleLine(consoleLines, "Project settings created: " + projectSettingsPath.string());
    }
    if (!startupSettingsWrite)
    {
        AddConsoleLine(consoleLines, "Project settings write failed: " + startupSettingsWrite.GetError().message);
    }
    AddConsoleLine(consoleLines, assetDatabaseProbeSummary);
    AddConsoleLine(consoleLines, "Asset database: " + AK::ToDebugString(assetManifest.stats));
    if (!startupManifestWrite)
    {
        AddConsoleLine(consoleLines, "Asset manifest write failed: " + startupManifestWrite.GetError().message);
    }
    else
    {
        AddConsoleLine(consoleLines, "Asset manifest: " + assetManifest.manifestPath.string());
    }
    if (!startupPackageWrite)
    {
        AddConsoleLine(consoleLines, "Asset package write failed: " + startupPackageWrite.GetError().message);
    }
    else
    {
        AddConsoleLine(consoleLines, "Asset package: " + startupPackage.packagePath.string());
        AddConsoleLine(consoleLines, AK::ToDebugString(startupPackage.stats));
    }
    AddConsoleLine(consoleLines, packageProbeSummary);
    if (!startupVfsPackageMount)
    {
        AddConsoleLine(consoleLines, "VFS package mount failed: " + startupVfsPackageMount.GetError().message);
    }
    if (!startupVfsLooseMount)
    {
        AddConsoleLine(consoleLines, "VFS loose mount failed: " + startupVfsLooseMount.GetError().message);
    }
    AddConsoleLine(consoleLines, "VFS: " + AK::ToDebugString(startupVfsStats));
    AddConsoleLine(consoleLines, vfsProbeSummary);
    AddConsoleLine(consoleLines, streamingProbeSummary);
    AddConsoleLine(consoleLines, worldPartitionProbeSummary);
    AddConsoleLine(consoleLines, "World partition assets attached: " + std::to_string(partitionAssetIndex));
    AddConsoleLine(consoleLines, jobProbeSummary);
    AddConsoleLine(consoleLines, "Editor jobs: " + AK::ToDebugString(editorJobSystem.Stats()));
    AddConsoleLine(consoleLines, commandProbeSummary);
    AddConsoleLine(consoleLines, inputProbeSummary);
    const AK::RenderFoundationProbe renderProbe = AK::BuildRenderFoundationProbe();
    AddConsoleLine(consoleLines, renderProbe.summary);
    AddConsoleLine(consoleLines, "Depth policy: " + AK::ToDebugString(renderProbe.depthReport));
    AddConsoleLine(consoleLines, "RenderGraph: " + AK::ToDebugString(renderProbe.graphStats));
    AddConsoleLine(consoleLines, resourceProbeSummary);
    AddConsoleLine(consoleLines, memoryProbeSummary);
    AddConsoleLine(consoleLines, boundsProbeSummary);
    AddConsoleLine(consoleLines, "Startup bounds: " + AK::ToDebugString(startupBoundsStats));
    AddConsoleLine(consoleLines, transformProbeSummary);
    if (startupSanitizedTransforms > 0)
    {
        AddConsoleLine(consoleLines, "Sanitized startup transforms: " + std::to_string(startupSanitizedTransforms));
    }
    AddConsoleLine(consoleLines, largeWorldProbeSummary);
    AddConsoleLine(consoleLines, worldTopologyProbeSummary);
    AddConsoleLine(consoleLines, gravityProbeSummary);
    AddConsoleLine(consoleLines, terrainProbeSummary);
    AddConsoleLine(consoleLines, splineProbeSummary);
    AddConsoleLine(consoleLines, textureSetProbeSummary);
    AddConsoleLine(consoleLines, ntcProbeSummary);
    AddConsoleLine(consoleLines, geometryProbeSummary);
    AddConsoleLine(consoleLines, mathProbe.summary);
    AddConsoleLine(consoleLines, ChernoffCenterSampleToText(mathProbe));
    AddConsoleLine(consoleLines, "Asset scan complete: " + std::to_string(registry.Assets().size()) + " file(s)");
    AddConsoleLine(consoleLines, "Scene path: " + sandboxScenePath.string());
    AddConsoleLine(consoleLines, "Project root: " + projectRoot.string());
    AddConsoleLine(consoleLines, editorFocusProbeSummary);
    AddConsoleLine(consoleLines, editorDragDropProbeSummary);
    AddConsoleLine(consoleLines, editorDragDropDiagnostics.summary);
    AddConsoleLine(consoleLines, editorCommandStateProbeSummary);
    AddConsoleLine(consoleLines, editorCommandPaletteRuntimeProbeSummary);
    AddConsoleLine(consoleLines, editorMenuToolbarRuntimeProbeSummary);
    AddConsoleLine(consoleLines, editorWorkspaceLoad.summary);
    AddConsoleLine(consoleLines, AK::FormatEditorAutosavePolicy(editorAutosavePolicy));
    AddConsoleLine(consoleLines, AK::FormatEditorAutosavePaths(editorAutosavePaths));
    AddConsoleLine(consoleLines, editorAutosaveProbeSummary);
    AddConsoleLine(consoleLines, editorActivityProbeSummary);
    AddConsoleLine(consoleLines, editorActivityDiagnostics.summary);
    AddConsoleLine(consoleLines, editorDiagnosticsPanelProbeSummary);
    AddConsoleLine(consoleLines, "AK Editor v9.7 diagnostics view / panel rendering initialized");
    AddConsoleLine(consoleLines, AK::BuildEditorDiagnosticsInteractionProbeSummary());

    AK::Stopwatch titleTimer;
    AK::Stopwatch frameTimer;
    AK::GameClock gameClock({projectSettings.runtime.fixedDeltaSeconds, projectSettings.runtime.maxFrameDeltaSeconds, projectSettings.runtime.maxFixedStepsPerFrame});
    AK::ResourceManager resourceManager({3});
    resourceManager.Create({AK::ResourceType::Mesh, AK::BuildAssetGuidFromNormalizedPath("builtin:cube"), "builtin:cube", 65536}, 0);
    resourceManager.Create({AK::ResourceType::Material, AK::BuildAssetGuidFromNormalizedPath("builtin:default"), "builtin:default", 2048}, 0);

    AK::StreamingSystem startupStreaming(&startupVfs, &resourceManager);
    std::size_t startupStreamingQueued = 0;
    for (const AK::AssetManifestRecord& record : assetManifest.records)
    {
        if (startupStreamingQueued >= 4)
        {
            break;
        }
        if (record.kind == AK::AssetKind::Mesh || record.kind == AK::AssetKind::Texture || record.kind == AK::AssetKind::Material || record.kind == AK::AssetKind::Shader)
        {
            const AK::u64 requestId = startupStreaming.RequestLoad(record.guid, AK::ResourceType::Unknown, startupStreamingQueued == 0 ? AK::StreamingPriority::High : AK::StreamingPriority::Normal, record.sourcePath.generic_string(), 0);
            (void)requestId;
            ++startupStreamingQueued;
        }
    }
    AK::StreamingBudget startupStreamingBudget{};
    startupStreamingBudget.maxLoadsPerUpdate = 4;
    startupStreamingBudget.maxBytesPerUpdate = 32ull * 1024ull * 1024ull;
    const AK::StreamingUpdateResult startupStreamingUpdate = startupStreaming.Update(startupStreamingBudget, 0);
    const AK::StreamingStats startupStreamingStats = startupStreaming.Stats();
    AddConsoleLine(consoleLines, "Streaming startup: " + AK::ToDebugString(startupStreamingUpdate));
    std::uint64_t frameIndex = 0;
    bool dirty = false;
    bool renameActive = false;
    std::string renameBuffer;
    AK::EntityId pendingDeleteEntity = AK::InvalidEntity;
    std::string statusLine = "Ready";
    ViewportCamera viewportCamera{};
    AK::EditorScrollState hierarchyScroll = AK::MakeEditorScrollState(AK::EditorScrollPanelKind::Hierarchy, 0, 0, RowHeight);
    AK::EditorScrollState inspectorScroll = AK::MakeEditorScrollState(AK::EditorScrollPanelKind::Inspector, 0, 0, RowHeight);
    AK::EditorScrollState projectGridScroll = AK::MakeEditorScrollState(AK::EditorScrollPanelKind::ProjectGrid, 0, 0, 74);
    AK::EditorScrollState consoleScroll = AK::MakeEditorScrollState(AK::EditorScrollPanelKind::Console, 0, 0, RowHeight);
    AK::EditorDiagnosticsInteractionState diagnosticsInteractionState = AK::MakeDefaultEditorDiagnosticsInteractionState();
    AK::EditorDiagnosticsPanelState diagnosticsPanelStateForInput{};
    bool snapEnabled = false;
    AK::EditorTransformGizmoMode activeTransformGizmoMode = AK::EditorTransformGizmoMode::Translate;
    AK::EditorTransformGizmoSpace activeTransformGizmoSpace = AK::EditorTransformGizmoSpace::World;
    std::uint64_t dirtyRevision = 0;
    std::uint64_t lastAutosaveRevision = 0;
    bool draggingEntity = false;
    bool panningViewport = false;
    AK::EntityId draggedEntity = AK::InvalidEntity;
    float dragOffsetX = 0.0f;
    float dragOffsetZ = 0.0f;
    int previousMouseX = 0;
    int previousMouseY = 0;
    bool dragUndoCaptured = false;
    AK::EditorTransformGizmoRuntime transformGizmoRuntime{};
    std::vector<SceneSnapshot> undoStack;
    std::vector<SceneSnapshot> redoStack;

    FocusViewportOnEntity(scene, selectedEntity, viewportCamera);

    auto markDirty = [&]()
    {
        dirty = true;
        ++dirtyRevision;
        pendingDeleteEntity = AK::InvalidEntity;
    };

    auto pushUndoSnapshot = [&](const std::string& reason)
    {
        undoStack.push_back(CaptureSceneSnapshot(scene, selectedEntity));
        if (undoStack.size() > MaxUndoSnapshots)
        {
            undoStack.erase(undoStack.begin());
        }
        redoStack.clear();
        statusLine = "Undo checkpoint: " + reason;
    };

    auto restoreFromHistory = [&](const SceneSnapshot& snapshot)
    {
        RestoreSceneSnapshot(scene, snapshot, selectedEntity);
        nextEntitySerial = BuildNextEntitySerial(scene);
        FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
        renameActive = false;
        pendingDeleteEntity = AK::InvalidEntity;
        draggingEntity = false;
        draggedEntity = AK::InvalidEntity;
        panningViewport = false;
        dragUndoCaptured = false;
        dirty = true;
        ++dirtyRevision;
    };

    auto undoSceneChange = [&]()
    {
        if (undoStack.empty())
        {
            statusLine = "Undo unavailable";
            return;
        }

        redoStack.push_back(CaptureSceneSnapshot(scene, selectedEntity));
        const SceneSnapshot snapshot = undoStack.back();
        undoStack.pop_back();
        restoreFromHistory(snapshot);
        statusLine = "Undo applied";
        AddConsoleLine(consoleLines, statusLine);
    };

    auto redoSceneChange = [&]()
    {
        if (redoStack.empty())
        {
            statusLine = "Redo unavailable";
            return;
        }

        undoStack.push_back(CaptureSceneSnapshot(scene, selectedEntity));
        if (undoStack.size() > MaxUndoSnapshots)
        {
            undoStack.erase(undoStack.begin());
        }
        const SceneSnapshot snapshot = redoStack.back();
        redoStack.pop_back();
        restoreFromHistory(snapshot);
        statusLine = "Redo applied";
        AddConsoleLine(consoleLines, statusLine);
    };

    auto createEntity = [&]()
    {
        pushUndoSnapshot("create entity");
        const std::string name = "Entity_" + std::to_string(nextEntitySerial++);
        selectedEntity = scene.GetWorld().CreateEntity(name);
        AK::TransformComponent& transform = scene.GetWorld().AddTransform(selectedEntity);
        const int xOffset = static_cast<int>(nextEntitySerial % 5) - 2;
        const int zOffset = static_cast<int>(nextEntitySerial % 3) - 1;
        transform.position = {static_cast<float>(xOffset), 0.0f, static_cast<float>(zOffset)};
        SyncWorldPositionFromTransform(scene, selectedEntity);
        FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
        markDirty();
        statusLine = "Created " + name;
        AddConsoleLine(consoleLines, statusLine);
    };

    auto createMeshEntity = [&]()
    {
        pushUndoSnapshot("create mesh entity");
        const std::string name = "Mesh_" + std::to_string(nextEntitySerial++);
        selectedEntity = scene.GetWorld().CreateEntity(name);
        AK::TransformComponent& transform = scene.GetWorld().AddTransform(selectedEntity);
        transform.position = {0.5f * static_cast<float>(nextEntitySerial % 7), 0.0f, 0.5f * static_cast<float>(nextEntitySerial % 5)};
        transform.scale = {1.0f, 1.0f, 1.0f};
        SyncWorldPositionFromTransform(scene, selectedEntity);
        AK::MeshComponent& mesh = scene.GetWorld().AddMesh(selectedEntity);
        mesh.mesh = "builtin:cube";
        mesh.material = "builtin:default";
        FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
        markDirty();
        statusLine = "Created mesh entity " + name;
        AddConsoleLine(consoleLines, statusLine);
    };

    auto createCameraEntity = [&]()
    {
        pushUndoSnapshot("create camera entity");
        const std::string name = "Camera_" + std::to_string(nextEntitySerial++);
        selectedEntity = scene.GetWorld().CreateEntity(name);
        AK::TransformComponent& transform = scene.GetWorld().AddTransform(selectedEntity);
        transform.position = {viewportCamera.centerX, 1.5f, viewportCamera.centerZ - 5.0f};
        SyncWorldPositionFromTransform(scene, selectedEntity);
        AK::CameraComponent& camera = scene.GetWorld().AddCamera(selectedEntity);
        camera.verticalFovDegrees = 60.0f;
        camera.nearPlane = 0.05f;
        camera.farPlane = 1000.0f;
        camera.primary = scene.GetWorld().CameraCount() == 1;
        FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
        markDirty();
        statusLine = "Created camera entity " + name;
        AddConsoleLine(consoleLines, statusLine);
    };

    auto createLightEntity = [&]()
    {
        pushUndoSnapshot("create light entity");
        const std::string name = "Light_" + std::to_string(nextEntitySerial++);
        selectedEntity = scene.GetWorld().CreateEntity(name);
        AK::TransformComponent& transform = scene.GetWorld().AddTransform(selectedEntity);
        transform.position = {viewportCamera.centerX + 2.0f, 3.0f, viewportCamera.centerZ - 2.0f};
        SyncWorldPositionFromTransform(scene, selectedEntity);
        AK::LightComponent& light = scene.GetWorld().AddLight(selectedEntity);
        light.type = AK::LightType::Directional;
        light.intensity = 2.5f;
        light.color = {1.0f, 0.96f, 0.86f};
        FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
        markDirty();
        statusLine = "Created light entity " + name;
        AddConsoleLine(consoleLines, statusLine);
    };

    auto duplicateSelected = [&]()
    {
        const AK::EntityRecord* record = FindEntityRecord(scene, selectedEntity);
        const AK::TransformComponent* sourceTransform = scene.GetWorld().GetTransform(selectedEntity);
        if (!record || !sourceTransform)
        {
            statusLine = "Duplicate failed: selected entity has no transform";
            AddConsoleLine(consoleLines, statusLine);
            return;
        }

        pushUndoSnapshot("duplicate entity");
        const AK::EntityId sourceEntity = selectedEntity;
        const AK::TransformComponent copiedTransform = *sourceTransform;
        const std::string newName = BuildUniqueDuplicateName(scene, record->name);
        selectedEntity = scene.GetWorld().CreateEntity(newName);
        AK::TransformComponent& transform = scene.GetWorld().AddTransform(selectedEntity);
        transform = copiedTransform;
        transform.position.x += 0.5f;
        transform.position.z += 0.5f;
        MarkTransformDirty(transform);

        if (const AK::WorldPositionComponent* worldPosition = scene.GetWorld().GetWorldPosition(sourceEntity))
        {
            AK::WorldPositionComponent copiedWorldPosition = *worldPosition;
            copiedWorldPosition.position = AK::AddLocalOffset(copiedWorldPosition.position, {0.5f, 0.0f, 0.5f}, EditorWorldCellSize);
            scene.GetWorld().AddWorldPosition(selectedEntity) = copiedWorldPosition;
        }
        else
        {
            EnsureWorldPositionFromTransform(scene, selectedEntity);
        }

        if (const AK::BoundsComponent* bounds = scene.GetWorld().GetBounds(sourceEntity))
        {
            AK::BoundsComponent copiedBounds = *bounds;
            copiedBounds.dirtyFlags = AK::BoundsDirty_All;
            scene.GetWorld().AddBounds(selectedEntity) = copiedBounds;
        }

        if (const AK::CameraComponent* camera = scene.GetWorld().GetCamera(sourceEntity))
        {
            scene.GetWorld().AddCamera(selectedEntity) = *camera;
        }
        if (const AK::LightComponent* light = scene.GetWorld().GetLight(sourceEntity))
        {
            scene.GetWorld().AddLight(selectedEntity) = *light;
        }
        if (const AK::MeshComponent* mesh = scene.GetWorld().GetMesh(sourceEntity))
        {
            scene.GetWorld().AddMesh(selectedEntity) = *mesh;
        }

        FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
        markDirty();
        statusLine = "Duplicated entity: " + newName;
        AddConsoleLine(consoleLines, statusLine);
    };

    auto requestDeleteSelected = [&]()
    {
        if (selectedEntity == AK::InvalidEntity)
        {
            return;
        }

        const AK::EntityRecord* record = FindEntityRecord(scene, selectedEntity);
        const std::string selectedName = record ? record->name : "entity";

        if (pendingDeleteEntity != selectedEntity)
        {
            pendingDeleteEntity = selectedEntity;
            statusLine = "Press Delete again to confirm removing " + selectedName;
            AddConsoleLine(consoleLines, statusLine);
            return;
        }

        pushUndoSnapshot("delete entity");
        const std::size_t previousIndex = EntityIndexOf(scene, selectedEntity);
        if (scene.GetWorld().DestroyEntity(selectedEntity))
        {
            selectedEntity = EntityAtIndex(scene, previousIndex);
            pendingDeleteEntity = AK::InvalidEntity;
            markDirty();
            statusLine = "Deleted " + selectedName;
            AddConsoleLine(consoleLines, statusLine);
        }
    };

    auto saveScene = [&]()
    {
        std::filesystem::create_directories(sandboxScenePath.parent_path());
        if (scene.SaveToFile(sandboxScenePath))
        {
            dirty = false;
            lastAutosaveRevision = dirtyRevision;
            AK::NotifyEditorAutosaveSceneSaved(editorAutosaveState, dirtyRevision);
            pendingDeleteEntity = AK::InvalidEntity;
            statusLine = "Saved scene.akscene";
            AddConsoleLine(consoleLines, statusLine);
        }
        else
        {
            statusLine = "Save failed";
            AddConsoleLine(consoleLines, statusLine);
        }
    };

    auto loadScene = [&]()
    {
        if (std::filesystem::exists(sandboxScenePath))
        {
            pushUndoSnapshot("load scene");
        }

        if (std::filesystem::exists(sandboxScenePath) && scene.LoadFromFile(sandboxScenePath))
        {
            EnsureDefaultScene(scene);
            const std::size_t fixedTransforms = SanitizeSceneTransforms(scene);
            EnsureWorldPositions(scene);
            const AK::BoundsUpdateStats loadedBoundsStats = EnsureSceneBounds(scene, true);
            AddConsoleLine(consoleLines, "Loaded bounds: " + AK::ToDebugString(loadedBoundsStats));
            if (fixedTransforms > 0)
            {
                AddConsoleLine(consoleLines, "Sanitized loaded transforms: " + std::to_string(fixedTransforms));
            }
            selectedEntity = EntityAtIndex(scene, 0);
            FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
            nextEntitySerial = BuildNextEntitySerial(scene);
            dirty = false;
            dirtyRevision = 0;
            lastAutosaveRevision = 0;
            renameActive = false;
            pendingDeleteEntity = AK::InvalidEntity;
            statusLine = "Loaded scene.akscene";
            AddConsoleLine(consoleLines, statusLine);
        }
        else
        {
            statusLine = "Load failed: scene.akscene not found or invalid";
            AddConsoleLine(consoleLines, statusLine);
        }
    };

    auto rescanAssets = [&]()
    {
        registry.Clear();
        registry.ScanDirectory(projectRoot / "assets");
        assetManifest = AK::BuildAssetManifest(projectLayout);
        const AK::Result<void> writeManifest = AK::WriteAssetManifestText(assetManifest.manifestPath, assetManifest);
        statusLine = "Rescanned assets: " + std::to_string(registry.Assets().size()) + " file(s), " + AK::ToDebugString(assetManifest.stats);
        AddConsoleLine(consoleLines, statusLine);
        if (!writeManifest)
        {
            AddConsoleLine(consoleLines, "Asset manifest write failed: " + writeManifest.GetError().message);
        }
    };

    auto dispatchEditorCommand = [&](const AK::CommandInvocation& invocation)
    {
        if (invocation.id == AK::CommandId::Count)
        {
            return false;
        }

        const AK::CommandDescriptor* descriptor = commandRegistry.Find(invocation.id);
        if (descriptor && descriptor->requiresSelection && selectedEntity == AK::InvalidEntity)
        {
            statusLine = std::string("Command requires selection: ") + descriptor->displayName;
            AddConsoleLine(consoleLines, statusLine);
            return false;
        }

        switch (invocation.id)
        {
            case AK::CommandId::CloseEditor:
                window.RequestClose();
                statusLine = "Close requested";
                return true;
            case AK::CommandId::NewEntity:
                createEntity();
                return true;
            case AK::CommandId::NewMesh:
                createMeshEntity();
                return true;
            case AK::CommandId::NewCamera:
                createCameraEntity();
                return true;
            case AK::CommandId::NewLight:
                createLightEntity();
                return true;
            case AK::CommandId::DeleteSelection:
                requestDeleteSelected();
                return true;
            case AK::CommandId::SaveScene:
                saveScene();
                return true;
            case AK::CommandId::LoadScene:
                loadScene();
                return true;
            case AK::CommandId::RescanAssets:
                rescanAssets();
                return true;
            case AK::CommandId::Undo:
                undoSceneChange();
                return true;
            case AK::CommandId::Redo:
                redoSceneChange();
                return true;
            case AK::CommandId::DuplicateSelection:
                duplicateSelected();
                return true;
            case AK::CommandId::FocusSelection:
                FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
                statusLine = "Focused selected entity";
                return true;
            case AK::CommandId::ResetViewport:
            {
                AK::EditorSceneViewCamera sceneView = BuildSceneViewCamera(viewportCamera);
                AK::ResetEditorSceneViewCamera(sceneView, viewportCamera.sceneView.mode);
                ApplySceneViewCamera(viewportCamera, sceneView);
                statusLine = "Viewport reset";
                return true;
            }
            case AK::CommandId::ViewFrameAll:
            {
                const AK::BoundsUpdateStats boundsStats = EnsureSceneBounds(scene);
                EditorLayout layout = BuildEditorLayout(window);
                ApplyEditorRuntimeLayoutToLegacyLayout(editorRuntimeBridge, layout);
                AK::EditorSceneViewCamera sceneView = BuildSceneViewCamera(viewportCamera);
                if (!AK::FrameEditorSceneViewCamera(sceneView, ToEditorSceneViewBounds(boundsStats.sceneBounds), ToEditorRect(layout.viewportGrid)))
                {
                    statusLine = "Frame All failed: scene bounds unavailable";
                    AddConsoleLine(consoleLines, statusLine);
                    return false;
                }
                ApplySceneViewCamera(viewportCamera, sceneView);
                statusLine = "Scene View frame all";
                AddConsoleLine(consoleLines, statusLine);
                return true;
            }
            case AK::CommandId::ViewScene2D:
            case AK::CommandId::ViewScene3D:
            {
                AK::EditorSceneViewCamera sceneView = BuildSceneViewCamera(viewportCamera);
                AK::SetEditorSceneViewMode(sceneView, SceneViewModeForCommand(invocation.id));
                ApplySceneViewCamera(viewportCamera, sceneView);
                AK::EndEditorTransformGizmoDrag(transformGizmoRuntime);
                statusLine = std::string("Scene View mode: ") + AK::ToString(viewportCamera.sceneView.mode) + " / " + AK::ToString(viewportCamera.sceneView.projection);
                AddConsoleLine(consoleLines, statusLine);
                return true;
            }
            case AK::CommandId::ViewAxisTop:
            case AK::CommandId::ViewAxisFront:
            case AK::CommandId::ViewAxisRight:
            {
                AK::EditorSceneViewCamera sceneView = BuildSceneViewCamera(viewportCamera);
                const AK::EditorSceneViewAxis axis = SceneViewAxisForCommand(invocation.id);
                AK::SnapEditorSceneViewCameraToAxis(sceneView, axis);
                ApplySceneViewCamera(viewportCamera, sceneView);
                AK::EndEditorTransformGizmoDrag(transformGizmoRuntime);
                statusLine = std::string("Scene View axis: ") + AK::ToString(axis);
                AddConsoleLine(consoleLines, statusLine);
                return true;
            }
            case AK::CommandId::ToggleGridSnap:
                snapEnabled = !snapEnabled;
                statusLine = std::string("Grid snap ") + (snapEnabled ? "enabled" : "disabled");
                AddConsoleLine(consoleLines, statusLine);
                return true;
            case AK::CommandId::ToolTranslate:
            case AK::CommandId::ToolRotate:
            case AK::CommandId::ToolScale:
                activeTransformGizmoMode = TransformGizmoModeForCommand(invocation.id);
                AK::EndEditorTransformGizmoDrag(transformGizmoRuntime);
                statusLine = std::string("Scene transform tool: ") + AK::ToString(activeTransformGizmoMode) + "/" + AK::ToString(activeTransformGizmoSpace);
                AddConsoleLine(consoleLines, statusLine);
                return true;
            case AK::CommandId::ToolSpaceWorld:
            case AK::CommandId::ToolSpaceLocal:
                activeTransformGizmoSpace = TransformGizmoSpaceForCommand(invocation.id);
                AK::EndEditorTransformGizmoDrag(transformGizmoRuntime);
                statusLine = std::string("Scene transform space: ") + AK::ToString(activeTransformGizmoSpace);
                AddConsoleLine(consoleLines, statusLine);
                return true;
            case AK::CommandId::SelectPrevious:
                selectedEntity = SelectRelativeEntity(scene, selectedEntity, -1);
                pendingDeleteEntity = AK::InvalidEntity;
                statusLine = "Selected previous entity";
                return true;
            case AK::CommandId::SelectNext:
                selectedEntity = SelectRelativeEntity(scene, selectedEntity, 1);
                pendingDeleteEntity = AK::InvalidEntity;
                statusLine = "Selected next entity";
                return true;
            case AK::CommandId::RenameSelection:
            {
                const AK::EntityRecord* record = FindEntityRecord(scene, selectedEntity);
                renameBuffer = record ? record->name : "Entity";
                renameActive = true;
                AK::BeginEditorTextEdit(editorFocusState, "Hierarchy.RenameSelection", renameBuffer);
                pendingDeleteEntity = AK::InvalidEntity;
                statusLine = "Rename mode";
                return true;
            }
            default:
                statusLine = std::string("Unknown command: ") + AK::ToString(invocation.id);
                AddConsoleLine(consoleLines, statusLine);
                return false;
        }
    };

    while (window.PollEvents())
    {
        gameClock.BeginFrame(frameTimer.RestartSeconds());
        while (gameClock.ConsumeFixedStep())
        {
        }
        const AK::FrameTiming frameTiming = gameClock.Snapshot();
        frameIndex = frameTiming.frameIndex;
        diagnosticsHub.BeginFrame(frameTiming.frameIndex);
        AK::Stopwatch diagnosticsFrameStopwatch;

        const AK::WindowInput& input = window.Input();
        const bool editorTextInputActive = renameActive || AK::EditorHasTextCapture(editorFocusState) || editorCommandPaletteRuntime.palette.open;
        const AK::InputActionSnapshot inputActions = editorInputMap.Evaluate(input, editorTextInputActive ? AK::InputContext::TextEntry : AK::InputContext::Editor);
        const std::string activeInputSummary = AK::FormatPressedActions(inputActions);
        if (window.Width() != editorRuntimeWidth || window.Height() != editorRuntimeHeight)
        {
            editorRuntimeWidth = window.Width();
            editorRuntimeHeight = window.Height();
            AK::ResizeEditorRuntimeBridge(editorRuntimeBridge, static_cast<AK::i32>(editorRuntimeWidth), static_cast<AK::i32>(editorRuntimeHeight));
        }
        const bool hasSelectionForCommands = selectedEntity != AK::InvalidEntity;
        AK::SetEditorCommandEnabled(editorRuntimeBridge, AK::CommandId::DeleteSelection, hasSelectionForCommands);
        AK::SetEditorCommandEnabled(editorRuntimeBridge, AK::CommandId::DuplicateSelection, hasSelectionForCommands);
        AK::SetEditorCommandEnabled(editorRuntimeBridge, AK::CommandId::RenameSelection, hasSelectionForCommands);
        AK::SetEditorCommandEnabled(editorRuntimeBridge, AK::CommandId::FocusSelection, hasSelectionForCommands);

        const std::vector<AK::EditorSelectionItem> commandPaletteEntities = BuildSceneSearchSelectionItems(scene);
        const std::vector<std::string> commandPaletteAssets = BuildAssetItems(registry);
        AK::EditorSearchIndex commandPaletteSearchIndex = AK::BuildDefaultEditorSearchIndex(commandRegistry, editorRuntimeBridge.panels, commandPaletteEntities, commandPaletteAssets);
        AK::EditorCommandContext commandPaletteContext = AK::BuildEditorCommandContext(editorFocusState,
                                                                                        selectedEntity != AK::InvalidEntity,
                                                                                        !undoStack.empty(),
                                                                                        !redoStack.empty(),
                                                                                        dirty,
                                                                                        snapEnabled,
                                                                                        true,
                                                                                        false,
                                                                                        false,
                                                                                        CommandForTransformGizmoMode(activeTransformGizmoMode),
                                                                                        CommandForTransformGizmoSpace(activeTransformGizmoSpace),
                                                                                        CommandForSceneViewMode(viewportCamera.sceneView.mode));
        AK::EditorCommandStateCache commandPaletteCommandCache = AK::BuildEditorCommandStateCache(commandRegistry, commandPaletteContext);
        AK::RefreshEditorCommandPaletteRuntimeSurface(editorCommandPaletteRuntime,
                                                       commandPaletteSearchIndex,
                                                       commandPaletteCommandCache,
                                                       editorWorkspaceLoad.shortcuts.profile,
                                                       static_cast<AK::i32>(window.Width()),
                                                       static_cast<AK::i32>(window.Height()));
        AK::RefreshEditorMenuToolbarRuntimeSurface(editorMenuToolbarRuntime,
                                                   editorRuntimeBridge.plan.menu,
                                                   editorRuntimeBridge.plan.toolbar,
                                                   commandPaletteCommandCache,
                                                   editorWorkspaceLoad.shortcuts.profile,
                                                   editorRuntimeBridge.plan.frame,
                                                   static_cast<AK::i32>(window.Width()),
                                                   static_cast<AK::i32>(window.Height()));

        EditorLayout layout = BuildEditorLayout(window);
        ApplyEditorRuntimeLayoutToLegacyLayout(editorRuntimeBridge, layout);
        const bool mouseInViewport = Contains(layout.viewportGrid, input.mouseX, input.mouseY);

        auto buildTransformGizmoSurface = [&]()
        {
            AK::EditorTransformGizmoBuildInput gizmoInput{};
            gizmoInput.viewport.rect = {layout.viewportGrid.left, layout.viewportGrid.top, layout.viewportGrid.right - layout.viewportGrid.left, layout.viewportGrid.bottom - layout.viewportGrid.top};
            gizmoInput.viewport.centerX = viewportCamera.centerX;
            gizmoInput.viewport.centerZ = viewportCamera.centerZ;
            gizmoInput.viewport.scale = viewportCamera.scale;
            gizmoInput.mode = activeTransformGizmoMode;
            gizmoInput.space = activeTransformGizmoSpace;
            gizmoInput.snapEnabled = snapEnabled;
            gizmoInput.snapStep = SnapStepForTransformGizmoMode(activeTransformGizmoMode);

            if (const AK::TransformComponent* selectedGizmoTransform = scene.GetWorld().GetTransform(selectedEntity))
            {
                gizmoInput.target.valid = selectedEntity != AK::InvalidEntity;
                gizmoInput.target.x = selectedGizmoTransform->position.x;
                gizmoInput.target.y = selectedGizmoTransform->position.y;
                gizmoInput.target.z = selectedGizmoTransform->position.z;
                gizmoInput.target.rotationX = selectedGizmoTransform->rotation.x;
                gizmoInput.target.rotationY = selectedGizmoTransform->rotation.y;
                gizmoInput.target.rotationZ = selectedGizmoTransform->rotation.z;
                gizmoInput.target.scaleX = selectedGizmoTransform->scale.x;
                gizmoInput.target.scaleY = selectedGizmoTransform->scale.y;
                gizmoInput.target.scaleZ = selectedGizmoTransform->scale.z;
                if (const AK::EntityRecord* selectedGizmoRecord = FindEntityRecord(scene, selectedEntity))
                {
                    gizmoInput.target.label = selectedGizmoRecord->name;
                }
            }

            return AK::BuildEditorTransformGizmoSurface(gizmoInput);
        };
        AK::EditorTransformGizmoSurface transformGizmoSurface = buildTransformGizmoSurface();

        AK::EditorInputRoutingInput focusRoutingInput{};
        focusRoutingInput.hit = AK::HitTestEditorRuntime(editorRuntimeBridge.plan, input.mouseX, input.mouseY);
        focusRoutingInput.popupOpen = editorUXRuntime.popupOpen || editorCommandPaletteRuntime.palette.open || editorMenuToolbarRuntime.menuOpen;
        focusRoutingInput.textEntryActive = renameActive;
        focusRoutingInput.mouseLeftPressed = input.mouseLeftPressed;
        focusRoutingInput.mouseRightPressed = input.mouseRightPressed;
        focusRoutingInput.mouseLeftDown = input.mouseLeftDown;
        focusRoutingInput.mouseRightDown = input.mouseRightDown;
        focusRoutingInput.mouseMiddleDown = input.mouseMiddleDown;
        focusRoutingInput.mouseWheel = input.mouseWheelDelta != 0;
        focusRoutingInput.cancelPressed = inputActions.Pressed(AK::InputAction::Cancel);
        focusRoutingInput.confirmPressed = inputActions.Pressed(AK::InputAction::Confirm);
        focusRoutingInput.ctrlDown = inputActions.ctrlDown;
        focusRoutingInput.shiftDown = inputActions.shiftDown;
        const AK::EditorInputRoutingDecision focusRouting = AK::RouteEditorInputFocus(editorFocusState, focusRoutingInput);

        const int hierarchyFirstEntityTop = layout.hierarchy.top + 32 + RowHeight;
        const int hierarchyViewportPixels = std::max(0, layout.hierarchy.bottom - hierarchyFirstEntityTop - 4);
        AK::SetEditorScrollMetrics(hierarchyScroll, static_cast<int>(scene.GetWorld().Entities().size()) * RowHeight, hierarchyViewportPixels, RowHeight);

        const int inspectorViewportPixels = std::max(0, layout.inspector.bottom - layout.inspector.top - 76);
        const int inspectorContentPixels = std::max(360, 240 + static_cast<int>(scene.GetWorld().Entities().size()) * 18);
        AK::SetEditorScrollMetrics(inspectorScroll, inspectorContentPixels, inspectorViewportPixels, RowHeight);

        const int projectTreeWidth = std::min(190, std::max(130, (layout.assets.right - layout.assets.left) / 4));
        const int projectGridWidth = std::max(0, layout.assets.right - (layout.assets.left + projectTreeWidth + 1));
        const int projectGridViewportPixels = std::max(0, layout.assets.bottom - layout.assets.top - 61);
        const int projectGridColumns = std::max(1, (projectGridWidth - 20) / 84);
        const int projectGridRows = static_cast<int>((registry.Assets().size() + static_cast<std::size_t>(projectGridColumns) - 1) / static_cast<std::size_t>(projectGridColumns));
        AK::SetEditorScrollMetrics(projectGridScroll, projectGridRows * 74, projectGridViewportPixels, 74);

        const int consoleViewportPixels = std::max(0, layout.console.bottom - layout.console.top - HeaderHeight - 8);
        AK::SetEditorScrollMetrics(consoleScroll, static_cast<int>(consoleLines.size()) * RowHeight, consoleViewportPixels, RowHeight);

        bool wheelConsumedByScrollablePanel = false;
        if (!renameActive && !editorUXRuntime.popupOpen && input.mouseWheelDelta != 0)
        {
            if (Contains(layout.hierarchy, input.mouseX, input.mouseY))
            {
                wheelConsumedByScrollablePanel = AK::ApplyEditorScrollWheel(hierarchyScroll, input.mouseWheelDelta);
            }
            else if (Contains(layout.inspector, input.mouseX, input.mouseY))
            {
                wheelConsumedByScrollablePanel = AK::ApplyEditorScrollWheel(inspectorScroll, input.mouseWheelDelta);
            }
            else if (Contains(layout.assets, input.mouseX, input.mouseY))
            {
                wheelConsumedByScrollablePanel = AK::ApplyEditorScrollWheel(projectGridScroll, input.mouseWheelDelta);
            }
            else if (Contains(layout.console, input.mouseX, input.mouseY))
            {
                wheelConsumedByScrollablePanel = AK::ApplyEditorScrollWheel(consoleScroll, input.mouseWheelDelta);
            }

            if (wheelConsumedByScrollablePanel)
            {
                statusLine = "Scrolled editor panel";
            }
        }

        if (!input.mouseLeftDown)
        {
            draggingEntity = false;
            draggedEntity = AK::InvalidEntity;
            dragUndoCaptured = false;
            AK::EndEditorTransformGizmoDrag(transformGizmoRuntime);
        }

        if (!input.mouseRightDown && !input.mouseMiddleDown)
        {
            panningViewport = false;
        }

        if (!renameActive && mouseInViewport && !wheelConsumedByScrollablePanel && input.mouseWheelDelta != 0)
        {
            AK::EditorSceneViewCamera sceneView = BuildSceneViewCamera(viewportCamera);
            AK::EditorSceneViewNavigationInput navigation{};
            navigation.viewport = ToEditorRect(layout.viewportGrid);
            navigation.mouseX = input.mouseX;
            navigation.mouseY = input.mouseY;
            navigation.mouseWheelDelta = input.mouseWheelDelta;
            navigation.deltaSeconds = frameTiming.frameDeltaSeconds > 0.0 ? frameTiming.frameDeltaSeconds : 1.0 / 60.0;
            const AK::EditorSceneViewNavigationResult navigationResult = AK::ApplyEditorSceneViewNavigation(sceneView, navigation);
            ApplySceneViewCamera(viewportCamera, sceneView);
            statusLine = navigationResult.status.empty()
                ? (IsViewport3D(viewportCamera) ? "Scene View 3D dolly" : "Viewport zoom: " + std::to_string(static_cast<int>(viewportCamera.scale)) + " px/unit")
                : navigationResult.status;
        }

        if (!renameActive && mouseInViewport && (input.mouseRightPressed || input.mouseMiddlePressed))
        {
            panningViewport = true;
            previousMouseX = input.mouseX;
            previousMouseY = input.mouseY;
            statusLine = "Viewport pan started";
        }

        if (!renameActive && panningViewport && (input.mouseRightDown || input.mouseMiddleDown))
        {
            const int dx = input.mouseX - previousMouseX;
            const int dy = input.mouseY - previousMouseY;
            if (dx != 0 || dy != 0)
            {
                AK::EditorSceneViewCamera sceneView = BuildSceneViewCamera(viewportCamera);
                AK::EditorSceneViewNavigationInput navigation{};
                navigation.viewport = ToEditorRect(layout.viewportGrid);
                navigation.mouseDeltaX = dx;
                navigation.mouseDeltaY = dy;
                navigation.pan2D = !IsViewport3D(viewportCamera) || input.mouseMiddleDown;
                navigation.orbit3D = IsViewport3D(viewportCamera) && input.mouseRightDown;
                navigation.deltaSeconds = frameTiming.frameDeltaSeconds > 0.0 ? frameTiming.frameDeltaSeconds : 1.0 / 60.0;
                const AK::EditorSceneViewNavigationResult navigationResult = AK::ApplyEditorSceneViewNavigation(sceneView, navigation);
                ApplySceneViewCamera(viewportCamera, sceneView);
                previousMouseX = input.mouseX;
                previousMouseY = input.mouseY;
                statusLine = navigationResult.status.empty() ? "Viewport panning" : navigationResult.status;
            }
        }

        if (inputActions.Pressed(AK::InputAction::Cancel))
        {
            if (editorCommandPaletteRuntime.palette.open)
            {
                AK::EditorCommandPaletteRuntimeInput paletteInput{};
                paletteInput.searchIndex = &commandPaletteSearchIndex;
                paletteInput.commandState = &commandPaletteCommandCache;
                paletteInput.shortcuts = &editorWorkspaceLoad.shortcuts.profile;
                paletteInput.viewportWidth = static_cast<AK::i32>(window.Width());
                paletteInput.viewportHeight = static_cast<AK::i32>(window.Height());
                paletteInput.keyEscapePressed = true;
                const AK::EditorCommandPaletteRuntimeResult result = AK::ApplyEditorCommandPaletteRuntimeInput(editorCommandPaletteRuntime, paletteInput);
                statusLine = result.statusText.empty() ? "Command palette closed" : result.statusText;
            }
            else if (editorMenuToolbarRuntime.menuOpen)
            {
                AK::CloseEditorMenuToolbarRuntime(editorMenuToolbarRuntime);
                statusLine = "Menu closed";
            }
            else if (editorUXRuntime.popupOpen)
            {
                AK::CloseEditorUXPopup(editorUXRuntime, AK::EditorUXPopupCloseReason::Escape);
                statusLine = "Popup closed";
            }
            else if (renameActive)
            {
                renameActive = false;
                AK::CancelEditorTextEdit(editorFocusState);
                statusLine = "Rename canceled";
            }
            else
            {
                dispatchEditorCommand({AK::CommandId::CloseEditor, AK::CommandSource::Shortcut});
            }
        }

        if (renameActive)
        {
            for (char c : input.textInput)
            {
                if (IsPrintableNameCharacter(c) && renameBuffer.size() < 64)
                {
                    renameBuffer.push_back(c);
                }
            }

            if (inputActions.Pressed(AK::InputAction::RenameBackspace) && !renameBuffer.empty())
            {
                renameBuffer.pop_back();
            }

            if (inputActions.Pressed(AK::InputAction::Confirm))
            {
                if (!renameBuffer.empty())
                {
                    pushUndoSnapshot("rename entity");
                }

                if (!renameBuffer.empty() && scene.GetWorld().RenameEntity(selectedEntity, renameBuffer))
                {
                    markDirty();
                    statusLine = "Renamed entity to " + renameBuffer;
                    AddConsoleLine(consoleLines, statusLine);
                }
                else
                {
                    statusLine = "Rename failed: empty or invalid entity";
                    AddConsoleLine(consoleLines, statusLine);
                }
                renameActive = false;
                AK::CommitEditorTextEdit(editorFocusState);
            }
        }
        else
        {
            if (focusRouting.allowGlobalShortcuts && !editorCommandPaletteRuntime.palette.open && !editorMenuToolbarRuntime.menuOpen)
            {
                const std::vector<AK::CommandInvocation> shortcutInvocations = AK::CollectCommandInvocations(commandRegistry, inputActions);
                for (const AK::CommandInvocation& invocation : shortcutInvocations)
                {
                    dispatchEditorCommand(invocation);
                }
            }

            bool runtimeUiConsumedClick = false;
            auto handleCommandPaletteResult = [&](const AK::EditorCommandPaletteRuntimeResult& result)
            {
                if (!result.handled && !result.opened && !result.closed && !result.accepted)
                {
                    return;
                }
                if (!result.statusText.empty())
                {
                    statusLine = result.statusText;
                }
                if (result.commandQueued)
                {
                    dispatchEditorCommand(result.command);
                    return;
                }
                if (result.accepted)
                {
                    if (result.acceptedKind == AK::EditorSearchResultKind::Panel && result.acceptedPanel.value != 0)
                    {
                        if (AK::ActivateEditorPanel(editorRuntimeBridge, result.acceptedPanel))
                        {
                            statusLine = "Command palette activated panel: " + result.acceptedStableId;
                        }
                    }
                    else if (result.acceptedKind == AK::EditorSearchResultKind::Entity && result.acceptedEntity != AK::InvalidEntity)
                    {
                        selectedEntity = result.acceptedEntity;
                        pendingDeleteEntity = AK::InvalidEntity;
                        FocusViewportOnEntity(scene, selectedEntity, viewportCamera);
                        const AK::EntityRecord* record = FindEntityRecord(scene, selectedEntity);
                        statusLine = record ? "Command palette selected entity: " + record->name : "Command palette selected entity";
                    }
                    else if (result.acceptedKind == AK::EditorSearchResultKind::Asset)
                    {
                        if (const AK::EditorPanelDesc* assetsPanel = editorRuntimeBridge.panels.FindByName("assets.browser"))
                        {
                            AK::ActivateEditorPanel(editorRuntimeBridge, assetsPanel->id);
                        }
                        statusLine = "Command palette selected asset: " + result.acceptedStableId;
                    }
                }
            };

            const Rect toolbarSearchRect = BuildToolbarSearchRect(editorRuntimeBridge);
            const bool commandPaletteOpenShortcut = input.keyCtrlDown && input.keyKPressed && !editorUXRuntime.popupOpen;
            const bool commandPaletteSearchClick = input.mouseLeftPressed && Contains(toolbarSearchRect, input.mouseX, input.mouseY);
            if (commandPaletteOpenShortcut || commandPaletteSearchClick || editorCommandPaletteRuntime.palette.open)
            {
                AK::EditorCommandPaletteRuntimeInput paletteInput{};
                paletteInput.searchIndex = &commandPaletteSearchIndex;
                paletteInput.commandState = &commandPaletteCommandCache;
                paletteInput.shortcuts = &editorWorkspaceLoad.shortcuts.profile;
                paletteInput.viewportWidth = static_cast<AK::i32>(window.Width());
                paletteInput.viewportHeight = static_cast<AK::i32>(window.Height());
                paletteInput.mouseX = input.mouseX;
                paletteInput.mouseY = input.mouseY;
                paletteInput.mouseWheelDelta = input.mouseWheelDelta;
                paletteInput.mouseLeftPressed = input.mouseLeftPressed;
                paletteInput.keyEscapePressed = false;
                paletteInput.keyEnterPressed = input.keyEnterPressed;
                paletteInput.keyBackspacePressed = input.keyBackspacePressed;
                paletteInput.keyUpPressed = input.keyUpPressed;
                paletteInput.keyDownPressed = input.keyDownPressed;
                paletteInput.openShortcutPressed = commandPaletteOpenShortcut || commandPaletteSearchClick;
                paletteInput.textInput = editorCommandPaletteRuntime.palette.open ? input.textInput : std::string{};
                const AK::EditorCommandPaletteRuntimeResult paletteResult = AK::ApplyEditorCommandPaletteRuntimeInput(editorCommandPaletteRuntime, paletteInput);
                handleCommandPaletteResult(paletteResult);
                runtimeUiConsumedClick = paletteResult.handled || editorCommandPaletteRuntime.palette.open || commandPaletteOpenShortcut || commandPaletteSearchClick;
            }

            if (!runtimeUiConsumedClick && !editorCommandPaletteRuntime.palette.open)
            {
                AK::EditorMenuToolbarRuntimeInput menuToolbarInput{};
                menuToolbarInput.menu = &editorRuntimeBridge.plan.menu;
                menuToolbarInput.toolbar = &editorRuntimeBridge.plan.toolbar;
                menuToolbarInput.commandState = &commandPaletteCommandCache;
                menuToolbarInput.shortcuts = &editorWorkspaceLoad.shortcuts.profile;
                menuToolbarInput.frame = editorRuntimeBridge.plan.frame;
                menuToolbarInput.viewportWidth = static_cast<AK::i32>(window.Width());
                menuToolbarInput.viewportHeight = static_cast<AK::i32>(window.Height());
                menuToolbarInput.mouseX = input.mouseX;
                menuToolbarInput.mouseY = input.mouseY;
                menuToolbarInput.mouseLeftPressed = input.mouseLeftPressed;
                menuToolbarInput.keyEscapePressed = false;
                const AK::EditorMenuToolbarRuntimeResult menuToolbarResult = AK::ApplyEditorMenuToolbarRuntimeInput(editorMenuToolbarRuntime, menuToolbarInput);
                if (!menuToolbarResult.statusText.empty() && (menuToolbarResult.handled || menuToolbarResult.commandQueued || menuToolbarResult.openCommandPalette))
                {
                    statusLine = menuToolbarResult.statusText;
                }
                if (menuToolbarResult.commandQueued)
                {
                    dispatchEditorCommand(menuToolbarResult.command);
                }
                if (menuToolbarResult.openCommandPalette)
                {
                    AK::EditorCommandPaletteRuntimeInput paletteInput{};
                    paletteInput.searchIndex = &commandPaletteSearchIndex;
                    paletteInput.commandState = &commandPaletteCommandCache;
                    paletteInput.shortcuts = &editorWorkspaceLoad.shortcuts.profile;
                    paletteInput.viewportWidth = static_cast<AK::i32>(window.Width());
                    paletteInput.viewportHeight = static_cast<AK::i32>(window.Height());
                    paletteInput.openShortcutPressed = true;
                    const AK::EditorCommandPaletteRuntimeResult paletteResult = AK::ApplyEditorCommandPaletteRuntimeInput(editorCommandPaletteRuntime, paletteInput);
                    handleCommandPaletteResult(paletteResult);
                }
                runtimeUiConsumedClick = menuToolbarResult.handled || menuToolbarResult.commandQueued || menuToolbarResult.openCommandPalette || editorMenuToolbarRuntime.menuOpen;
            }

            if (!runtimeUiConsumedClick && editorUXRuntime.popupOpen)
            {
                AK::HitTestEditorUXPopup(editorUXRuntime, input.mouseX, input.mouseY);
            }

            if (inputActions.Pressed(AK::InputAction::Cancel) && editorUXRuntime.popupOpen)
            {
                AK::CloseEditorUXPopup(editorUXRuntime, AK::EditorUXPopupCloseReason::Escape);
                editorFocusState.popupModalCapture = false;
                runtimeUiConsumedClick = true;
                statusLine = "Popup closed";
            }

            if (!runtimeUiConsumedClick && input.mouseLeftPressed && editorUXRuntime.popupOpen)
            {
                const AK::EditorUXActivationResult activation = AK::ActivateEditorUXPopupItem(editorUXRuntime, input.mouseX, input.mouseY);
                runtimeUiConsumedClick = activation.consumed;
                if (activation.consumed && !activation.message.empty())
                {
                    statusLine = activation.message;
                    AddConsoleLine(consoleLines, statusLine);
                }
                if (activation.invocation.id != AK::CommandId::CloseEditor && activation.invocation.id != AK::CommandId::Count)
                {
                    dispatchEditorCommand(activation.invocation);
                }
            }

            if (!runtimeUiConsumedClick && input.mouseRightPressed)
            {
                const AK::EditorRect anchor = AK::MakeEditorRect(input.mouseX, input.mouseY, 1, 1);
                if (Contains(layout.hierarchy, input.mouseX, input.mouseY))
                {
                    AK::OpenEditorUXPopup(editorUXRuntime, AK::BuildHierarchyContextWorkflow(anchor), static_cast<AK::i32>(window.Width()), static_cast<AK::i32>(window.Height()));
                    runtimeUiConsumedClick = true;
                    statusLine = "Hierarchy context menu";
                }
                else if (Contains(layout.assets, input.mouseX, input.mouseY))
                {
                    AK::OpenEditorUXPopup(editorUXRuntime, AK::BuildProjectContextWorkflow(anchor), static_cast<AK::i32>(window.Width()), static_cast<AK::i32>(window.Height()));
                    runtimeUiConsumedClick = true;
                    statusLine = "Project context menu";
                }
                else if (Contains(layout.inspector, input.mouseX, input.mouseY))
                {
                    AK::OpenEditorUXPopup(editorUXRuntime, AK::BuildAddComponentWorkflow(anchor), static_cast<AK::i32>(window.Width()), static_cast<AK::i32>(window.Height()));
                    runtimeUiConsumedClick = true;
                    statusLine = "Inspector Add Component menu";
                }
            }

            if (!runtimeUiConsumedClick)
            {
                const Rect diagnosticsBody = ActiveRuntimePanelBodyOrEmpty(editorRuntimeBridge, "diagnostics");
                if (Contains(diagnosticsBody, input.mouseX, input.mouseY))
                {
                    if (input.mouseWheelDelta != 0)
                    {
                        const AK::EditorDiagnosticsInteractionInput scrollInput = AK::MakeEditorDiagnosticsScroll(input.mouseWheelDelta,
                                                                                                                   RowHeight * 2,
                                                                                                                   std::max(0, diagnosticsBody.bottom - diagnosticsBody.top - 116),
                                                                                                                   std::max(0, static_cast<int>(diagnosticsPanelStateForInput.rows.size()) * 48));
                        const AK::EditorDiagnosticsInteractionResult result = AK::ApplyEditorDiagnosticsInteraction(diagnosticsInteractionState,
                                                                                                                     &editorActivityCenter,
                                                                                                                     diagnosticsPanelStateForInput,
                                                                                                                     scrollInput);
                        runtimeUiConsumedClick = result.handled;
                    }
                    if (input.mouseLeftPressed)
                    {
                        const int topY = diagnosticsBody.top + 6;
                        const int chipBaseX = std::max(diagnosticsBody.left + 238, diagnosticsBody.right - 528);
                        AK::EditorDiagnosticsInteractionInput interaction{};
                        if (input.mouseY >= topY && input.mouseY <= topY + 22)
                        {
                            if (input.mouseX >= chipBaseX && input.mouseX <= chipBaseX + 76)
                            {
                                interaction = AK::MakeEditorDiagnosticsBadgeToggle(AK::EditorDiagnosticsViewBadgeKind::Unread);
                            }
                            else if (input.mouseX >= chipBaseX + 82 && input.mouseX <= chipBaseX + 166)
                            {
                                interaction = AK::MakeEditorDiagnosticsBadgeToggle(AK::EditorDiagnosticsViewBadgeKind::Warnings);
                            }
                            else if (input.mouseX >= chipBaseX + 172 && input.mouseX <= chipBaseX + 254)
                            {
                                interaction = AK::MakeEditorDiagnosticsBadgeToggle(AK::EditorDiagnosticsViewBadgeKind::Errors);
                            }
                            else if (input.mouseX >= chipBaseX + 260 && input.mouseX <= chipBaseX + 338)
                            {
                                interaction = AK::MakeEditorDiagnosticsBadgeToggle(AK::EditorDiagnosticsViewBadgeKind::Tasks);
                            }
                        }

                        const int listTop = diagnosticsBody.top + 84;
                        const int listBottom = diagnosticsBody.bottom - 24;
                        if (interaction.action == AK::EditorDiagnosticsInteractionAction::None
                            && input.mouseY >= listTop
                            && input.mouseY < listBottom)
                        {
                            const int row = std::max(0, (input.mouseY - listTop + diagnosticsInteractionState.scrollOffsetPixels) / 48);
                            interaction = AK::MakeEditorDiagnosticsRowSelect(static_cast<AK::usize>(row));
                        }

                        if (interaction.action != AK::EditorDiagnosticsInteractionAction::None)
                        {
                            const AK::EditorDiagnosticsInteractionResult result = AK::ApplyEditorDiagnosticsInteraction(diagnosticsInteractionState,
                                                                                                                         &editorActivityCenter,
                                                                                                                         diagnosticsPanelStateForInput,
                                                                                                                         interaction);
                            runtimeUiConsumedClick = result.handled;
                            if (result.handled)
                            {
                                statusLine = result.statusText;
                            }
                        }
                    }
                }
            }

            if (!runtimeUiConsumedClick && input.mouseLeftPressed)
            {
                const std::vector<AK::CommandInvocation> routedInvocations = AK::RouteEditorPointerEvent(
                    editorRuntimeBridge,
                    {input.mouseX, input.mouseY, AK::EditorPointerButton::Left, true, true, false});
                runtimeUiConsumedClick = editorRuntimeBridge.plan.hover.region == AK::EditorHitRegion::Toolbar
                    || editorRuntimeBridge.plan.hover.region == AK::EditorHitRegion::DockTab
                    || editorRuntimeBridge.plan.hover.region == AK::EditorHitRegion::MenuBar
                    || editorRuntimeBridge.plan.hover.region == AK::EditorHitRegion::StatusBar
                    || editorRuntimeBridge.plan.hover.region == AK::EditorHitRegion::DockSplitter
                    || editorRuntimeBridge.plan.hover.region == AK::EditorHitRegion::Overlay;
                for (const AK::CommandInvocation& invocation : routedInvocations)
                {
                    runtimeUiConsumedClick = true;
                    dispatchEditorCommand(invocation);
                }
            }
            else if (!editorUXRuntime.popupOpen)
            {
                editorRuntimeBridge.plan.hover = AK::HitTestEditorRuntime(editorRuntimeBridge.plan, input.mouseX, input.mouseY);
            }

            if (!runtimeUiConsumedClick)
            {
                const ToolbarAction toolbarAction = HitTestToolbar(window);
                const AK::CommandId toolbarCommand = CommandFromToolbarAction(toolbarAction);
                if (toolbarCommand != AK::CommandId::Count)
                {
                    dispatchEditorCommand({toolbarCommand, AK::CommandSource::Toolbar});
                }
            }

            if (!runtimeUiConsumedClick && !editorUXRuntime.popupOpen && !editorCommandPaletteRuntime.palette.open && !editorMenuToolbarRuntime.menuOpen && mouseInViewport && input.mouseLeftPressed)
            {
                const AK::EditorTransformGizmoHandle gizmoHandle = AK::HitTestEditorTransformGizmo(transformGizmoSurface, input.mouseX, input.mouseY);
                if (gizmoHandle != AK::EditorTransformGizmoHandle::None)
                {
                    if (AK::BeginEditorTransformGizmoDrag(transformGizmoRuntime, transformGizmoSurface, gizmoHandle, input.mouseX, input.mouseY))
                    {
                        pushUndoSnapshot("transform gizmo drag");
                        draggingEntity = false;
                        draggedEntity = AK::InvalidEntity;
                        dragUndoCaptured = false;
                        runtimeUiConsumedClick = true;
                        statusLine = std::string("Transform gizmo drag started: ") + AK::ToString(gizmoHandle);
                    }
                }
            }

            if (!runtimeUiConsumedClick)
            {
                if (AK::EntityId clicked = HitTestHierarchy(window, scene, layout, hierarchyScroll.offsetPixels); clicked != AK::InvalidEntity)
                {
                    selectedEntity = clicked;
                    pendingDeleteEntity = AK::InvalidEntity;
                    const AK::EntityRecord* record = FindEntityRecord(scene, selectedEntity);
                    statusLine = record ? "Selected " + record->name : "Selected entity";
                }
                else if (AK::EntityId clickedViewport = HitTestViewport(window, scene, viewportCamera, layout); clickedViewport != AK::InvalidEntity)
                {
                    selectedEntity = clickedViewport;
                    pendingDeleteEntity = AK::InvalidEntity;
                    const AK::EntityRecord* record = FindEntityRecord(scene, selectedEntity);
                    statusLine = record ? "Selected in viewport: " + record->name : "Selected viewport entity";

                    if (AK::TransformComponent* transform = scene.GetWorld().GetTransform(selectedEntity))
                    {
                        const ViewportWorldPoint mouseWorld = ScreenToViewportWorld(layout.viewportGrid, viewportCamera, input.mouseX, input.mouseY);
                        dragOffsetX = transform->position.x - mouseWorld.x;
                        dragOffsetZ = transform->position.z - mouseWorld.z;
                        draggingEntity = true;
                        draggedEntity = selectedEntity;
                    }
                }
            }

            if (AK::TransformComponent* transform = scene.GetWorld().GetTransform(selectedEntity))
            {
                const float moveStep = inputActions.shiftDown ? 1.0f : 0.25f;
                const float rotateStep = inputActions.shiftDown ? 15.0f : 5.0f;
                const float scaleStep = inputActions.shiftDown ? 0.25f : 0.10f;
                bool transformChanged = false;

                if (transformGizmoRuntime.active)
                {
                    runtimeUiConsumedClick = true;
                    if (input.mouseLeftDown)
                    {
                        const AK::EditorTransformGizmoDragResult gizmoDrag = AK::UpdateEditorTransformGizmoDrag(transformGizmoRuntime, input.mouseX, input.mouseY);
                        if (gizmoDrag.changed)
                        {
                            if (gizmoDrag.mode == AK::EditorTransformGizmoMode::Rotate)
                            {
                                transform->rotation.x = gizmoDrag.rotationX;
                                transform->rotation.y = gizmoDrag.rotationY;
                                transform->rotation.z = gizmoDrag.rotationZ;
                            }
                            else if (gizmoDrag.mode == AK::EditorTransformGizmoMode::Scale)
                            {
                                transform->scale.x = gizmoDrag.scaleX;
                                transform->scale.y = gizmoDrag.scaleY;
                                transform->scale.z = gizmoDrag.scaleZ;
                            }
                            else
                            {
                                transform->position.x = gizmoDrag.x;
                                transform->position.y = gizmoDrag.y;
                                transform->position.z = gizmoDrag.z;
                            }
                            transformChanged = true;
                            statusLine = gizmoDrag.status;
                        }
                    }
                    else
                    {
                        AK::EndEditorTransformGizmoDrag(transformGizmoRuntime);
                        statusLine = "Transform gizmo drag ended";
                    }
                }

                if (!transformGizmoRuntime.active && draggingEntity && draggedEntity == selectedEntity && input.mouseLeftDown)
                {
                    if (!dragUndoCaptured)
                    {
                        pushUndoSnapshot("move entity in viewport");
                        dragUndoCaptured = true;
                    }

                    const ViewportWorldPoint mouseWorld = ScreenToViewportWorld(layout.viewportGrid, viewportCamera, input.mouseX, input.mouseY);
                    transform->position.x = mouseWorld.x + dragOffsetX;
                    transform->position.z = mouseWorld.z + dragOffsetZ;
                    if (snapEnabled)
                    {
                        SnapTransformXZ(*transform, DefaultSnapStep);
                    }
                    transformChanged = true;
                    statusLine = std::string("Dragged entity") + (snapEnabled ? " with snap: " : ": ") + TransformToText(transform);
                }

                if (!inputActions.ctrlDown)
                {
                    const bool keyboardTransformInput = inputActions.Active(AK::InputAction::MoveForward) || inputActions.Active(AK::InputAction::MoveBackward) || inputActions.Active(AK::InputAction::MoveLeft) || inputActions.Active(AK::InputAction::MoveRight)
                        || inputActions.Active(AK::InputAction::MoveDown) || inputActions.Active(AK::InputAction::MoveUp) || inputActions.Active(AK::InputAction::RotateYawLeft) || inputActions.Active(AK::InputAction::RotateYawRight)
                        || inputActions.Active(AK::InputAction::RotatePitchUp) || inputActions.Active(AK::InputAction::RotatePitchDown) || inputActions.Active(AK::InputAction::RotateRollLeft) || inputActions.Active(AK::InputAction::ScaleUp) || inputActions.Active(AK::InputAction::ScaleDown);
                    if (keyboardTransformInput)
                    {
                        pushUndoSnapshot("edit transform");
                    }

                    if (inputActions.Active(AK::InputAction::MoveForward))
                    {
                        transform->position.z += moveStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::MoveBackward))
                    {
                        transform->position.z -= moveStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::MoveLeft))
                    {
                        transform->position.x -= moveStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::MoveRight))
                    {
                        transform->position.x += moveStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::MoveDown))
                    {
                        transform->position.y -= moveStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::MoveUp))
                    {
                        transform->position.y += moveStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::RotateYawLeft))
                    {
                        transform->rotation.y -= rotateStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::RotateYawRight))
                    {
                        transform->rotation.y += rotateStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::RotatePitchUp))
                    {
                        transform->rotation.x += rotateStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::RotatePitchDown))
                    {
                        transform->rotation.x -= rotateStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::RotateRollLeft))
                    {
                        transform->rotation.z -= rotateStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::ScaleUp))
                    {
                        transform->scale.x += scaleStep;
                        transform->scale.y += scaleStep;
                        transform->scale.z += scaleStep;
                        transformChanged = true;
                    }
                    if (inputActions.Active(AK::InputAction::ScaleDown))
                    {
                        transform->scale.x = std::max(0.10f, transform->scale.x - scaleStep);
                        transform->scale.y = std::max(0.10f, transform->scale.y - scaleStep);
                        transform->scale.z = std::max(0.10f, transform->scale.z - scaleStep);
                        transformChanged = true;
                    }
                }

                if (transformChanged)
                {
                    if (snapEnabled)
                    {
                        SnapTransformXZ(*transform, DefaultSnapStep);
                    }
                    MarkTransformDirty(*transform);
                    const AK::TransformSanitizeResult sanitizeResult = SanitizeTransformComponent(*transform);
                    SyncWorldPositionFromTransform(scene, selectedEntity);
                    markDirty();
                    statusLine = std::string("Edited transform") + (snapEnabled ? " with snap: " : ": ") + TransformToText(transform);
                    if (sanitizeResult.changed)
                    {
                        statusLine += " | sanitized " + sanitizeResult.reason;
                    }
                }
            }
        }

        AK::Stopwatch diagnosticsSceneStopwatch;
        const AK::BoundsUpdateStats frameBoundsStats = EnsureSceneBounds(scene);
        const AK::VisibilityStats frameVisibilityStats = UpdateEditorVisibility(scene, viewportCamera);
        resourceManager.ProcessDeferredReleases(frameTiming.frameIndex);
        const AK::ResourceStats frameResourceStats = resourceManager.Stats();
        const AK::StreamingStats frameStreamingStats = startupStreaming.Stats();
        const AK::WorldPosition partitionCameraPosition = AK::MakeWorldPosition(static_cast<double>(viewportCamera.centerX), 0.0, static_cast<double>(viewportCamera.centerZ), EditorWorldCellSize);
        const AK::WorldPartitionUpdateResult framePartitionUpdate = editorWorldPartition.Update(partitionCameraPosition, frameTiming.frameIndex);
        const AK::WorldPartitionStats framePartitionStats = editorWorldPartition.Stats(partitionCameraPosition);
        const AK::JobSystemStats frameJobStats = editorJobSystem.Stats();
        diagnosticsHub.RecordZone("Editor.SceneUpdate", diagnosticsSceneStopwatch.ElapsedSeconds() * 1000.0);
        diagnosticsHub.SetCounter("entities", static_cast<AK::i64>(scene.GetWorld().Entities().size()));
        diagnosticsHub.SetCounter("assets", static_cast<AK::i64>(registry.Assets().size()));
        diagnosticsHub.SetCounter("package.entries", static_cast<AK::i64>(startupPackage.stats.entryCount));
        diagnosticsHub.SetCounter("vfs.entries", static_cast<AK::i64>(startupVfsStats.entryCount));
        diagnosticsHub.SetCounter("streaming.resident", static_cast<AK::i64>(frameStreamingStats.resident));
        diagnosticsHub.SetCounter("streaming.queued", static_cast<AK::i64>(frameStreamingStats.queued));
        diagnosticsHub.SetCounter("partition.cells", static_cast<AK::i64>(framePartitionStats.cells));
        diagnosticsHub.SetCounter("partition.active", static_cast<AK::i64>(framePartitionStats.active));
        diagnosticsHub.SetCounter("partition.queued", static_cast<AK::i64>(framePartitionStats.queued));
        diagnosticsHub.SetCounter("visible", static_cast<AK::i64>(frameVisibilityStats.visible));
        diagnosticsHub.SetCounter("culled", static_cast<AK::i64>(frameVisibilityStats.culled));
        diagnosticsHub.SetCounter("resources.resident", static_cast<AK::i64>(frameResourceStats.resident));
        diagnosticsHub.SetCounter("jobs.pending", static_cast<AK::i64>(frameJobStats.pendingJobs));

        AK::Stopwatch diagnosticsRenderStopwatch;
        renderer.BeginFrame();

        const AK::EntityRecord* selectedRecord = FindEntityRecord(scene, selectedEntity);
        const AK::TransformComponent* selectedTransform = scene.GetWorld().GetTransform(selectedEntity);
        const AK::WorldPositionComponent* selectedWorldPosition = scene.GetWorld().GetWorldPosition(selectedEntity);
        const AK::CameraComponent* selectedCamera = scene.GetWorld().GetCamera(selectedEntity);
        const AK::LightComponent* selectedLight = scene.GetWorld().GetLight(selectedEntity);
        const AK::MeshComponent* selectedMesh = scene.GetWorld().GetMesh(selectedEntity);
        const AK::BoundsComponent* selectedBounds = scene.GetWorld().GetBounds(selectedEntity);

        const AK::DiagnosticsSnapshot frameDiagnosticsSnapshot = diagnosticsHub.Snapshot();

        AK::EditorFrameDesc frame{};
        frame.title = "AK Engine Editor v10.6";
        frame.sceneName = scene.Name();
        frame.selectedEntityName = selectedRecord ? selectedRecord->name : "None";
        frame.selectedTransform = TransformToText(selectedTransform);
        frame.statusLine = statusLine;
        frame.hierarchyItems = BuildHierarchyItems(scene, selectedEntity);
        frame.inspectorLines = BuildInspectorLines(selectedTransform, selectedWorldPosition, selectedCamera, selectedLight, selectedMesh, selectedBounds, frameBoundsStats, frameVisibilityStats, frameResourceStats, resourceProbeSummary, memoryProbeSummary, diagnosticsProbeSummary, frameDiagnosticsSnapshot, filesystemProbeSummary, projectLayout, settingsProbeSummary, projectSettings, projectSettingsValidation, serializationProbeSummary, assetDatabaseProbeSummary, assetManifest.stats, packageProbeSummary, startupPackage.stats, vfsProbeSummary, startupVfsStats, streamingProbeSummary, frameStreamingStats, worldPartitionProbeSummary, framePartitionStats, framePartitionUpdate, renameActive, renameBuffer, viewportCamera, snapEnabled, undoStack.size(), redoStack.size(), mathProbe, geometryProbeSummary, transformProbeSummary, largeWorldProbeSummary, worldTopologyProbeSummary, gravityProbeSummary, terrainProbeSummary, splineProbeSummary, csgProbeSummary, textureSetProbeSummary, ntcProbeSummary, ntcProbe.futureVulkanPlan, inputProbeSummary, commandProbeSummary, activeInputSummary, jobProbeSummary, frameJobStats, renderProbe, frameTiming);
        frame.assetItems = BuildAssetItems(registry);
        frame.consoleLines = consoleLines;
        frame.viewportItems = BuildViewportItems(scene, selectedEntity);
        transformGizmoSurface = buildTransformGizmoSurface();
        frame.transformGizmo = BuildRenderTransformGizmo(transformGizmoSurface, transformGizmoRuntime, input.mouseX, input.mouseY);
        frame.viewportCenterX = viewportCamera.centerX;
        frame.viewportCenterZ = viewportCamera.centerZ;
        frame.viewportScale = viewportCamera.scale;
        frame.viewportMode = AK::ToString(viewportCamera.sceneView.mode);
        frame.viewportProjection = AK::ToString(viewportCamera.sceneView.projection);
        frame.viewportPerspective = viewportCamera.sceneView.mode == AK::EditorSceneViewMode::Mode3D;
        frame.viewportCameraX = viewportCamera.sceneView.positionX;
        frame.viewportCameraY = viewportCamera.sceneView.positionY;
        frame.viewportCameraZ = viewportCamera.sceneView.positionZ;
        frame.viewportYawDegrees = viewportCamera.sceneView.yawDegrees;
        frame.viewportPitchDegrees = viewportCamera.sceneView.pitchDegrees;
        frame.viewportFovYDegrees = viewportCamera.sceneView.fovYDegrees;
        frame.viewportDragging = draggingEntity;
        frame.viewportPanning = panningViewport;
        frame.viewportSnapEnabled = snapEnabled;
        frame.viewportSnapStep = DefaultSnapStep;
        frame.frameIndex = frameIndex;
        frame.entityCount = static_cast<std::uint32_t>(scene.GetWorld().Entities().size());
        frame.assetCount = static_cast<std::uint32_t>(registry.Assets().size());
        frame.meshCount = static_cast<std::uint32_t>(scene.GetWorld().MeshCount());
        frame.cameraCount = static_cast<std::uint32_t>(scene.GetWorld().CameraCount());
        frame.lightCount = static_cast<std::uint32_t>(scene.GetWorld().LightCount());
        frame.boundsCount = static_cast<std::uint32_t>(scene.GetWorld().BoundsCount());
        frame.visibleCount = static_cast<std::uint32_t>(frameVisibilityStats.visible);
        frame.culledCount = static_cast<std::uint32_t>(frameVisibilityStats.culled);
        frame.undoDepth = static_cast<std::uint32_t>(undoStack.size());
        frame.redoDepth = static_cast<std::uint32_t>(redoStack.size());
        frame.dirty = dirty;
        frame.hierarchyScrollPixels = hierarchyScroll.offsetPixels;
        frame.inspectorScrollPixels = inspectorScroll.offsetPixels;
        frame.projectGridScrollPixels = projectGridScroll.offsetPixels;
        frame.consoleScrollPixels = consoleScroll.offsetPixels;

        AK::EditorPanelModelFrame diagnosticsSourceFrame = BuildLiveDiagnosticsPanelFrame(consoleLines,
                                                                                           frameDiagnosticsSnapshot,
                                                                                           frameJobStats,
                                                                                           frameStreamingStats,
                                                                                           framePartitionStats,
                                                                                           frameResourceStats,
                                                                                           frame.frameIndex);
        AK::EditorCommandContext diagnosticsCommandContext = AK::BuildEditorCommandContext(editorFocusState,
                                                                                           selectedRecord != nullptr,
                                                                                           !undoStack.empty(),
                                                                                           !redoStack.empty(),
                                                                                           dirty,
                                                                                           snapEnabled,
                                                                                           true,
                                                                                           false,
                                                                                           false,
                                                                                           CommandForTransformGizmoMode(activeTransformGizmoMode),
                                                                                           CommandForTransformGizmoSpace(activeTransformGizmoSpace),
                                                                                           CommandForSceneViewMode(viewportCamera.sceneView.mode));
        AK::EditorCommandStateCache diagnosticsCommandCache = AK::BuildEditorCommandStateCache(commandRegistry, diagnosticsCommandContext);
        AK::EditorDiagnosticsPanelBuildInput diagnosticsBuildInput{};
        diagnosticsBuildInput.activity = &editorActivityCenter;
        diagnosticsBuildInput.panelFrame = &diagnosticsSourceFrame;
        diagnosticsBuildInput.autosaveState = &editorAutosaveState;
        diagnosticsBuildInput.autosavePolicy = &editorAutosavePolicy;
        diagnosticsBuildInput.workspaceLoad = &editorWorkspaceLoad;
        diagnosticsBuildInput.workspaceReport = &editorWorkspaceLoad.report;
        diagnosticsBuildInput.commandStates = &diagnosticsCommandCache;
        diagnosticsBuildInput.nowSeconds = static_cast<double>(frame.frameIndex);
        AK::EditorDiagnosticsPanelState diagnosticsPanelState = AK::BuildEditorDiagnosticsPanelState(diagnosticsBuildInput, AK::BuildEditorDiagnosticsPanelFilterFromInteraction(diagnosticsInteractionState));
        AK::SyncEditorDiagnosticsInteractionFromPanel(diagnosticsInteractionState, diagnosticsPanelState);
        diagnosticsPanelStateForInput = diagnosticsPanelState;
        AK::EditorDiagnosticsViewBuildInput diagnosticsViewInput{};
        diagnosticsViewInput.panelState = &diagnosticsPanelState;
        diagnosticsViewInput.viewportWidth = static_cast<AK::i32>(window.Width());
        diagnosticsViewInput.viewportHeight = 220;
        diagnosticsViewInput.scrollOffsetPixels = diagnosticsInteractionState.scrollOffsetPixels;
        diagnosticsViewInput.maxMetricTiles = 6;
        AK::EditorDiagnosticsViewState diagnosticsView = AK::BuildEditorDiagnosticsViewState(diagnosticsViewInput);
        FillRenderDiagnosticsFromView(frame, diagnosticsView);

        editorRuntimeBridge.plan.status.leftText = statusLine;
        editorRuntimeBridge.plan.status.rightText = "entities=" + std::to_string(frame.entityCount)
            + " assets=" + std::to_string(frame.assetCount)
            + " visible=" + std::to_string(frame.visibleCount)
            + " frame=" + std::to_string(frame.frameIndex);
        editorRuntimeDiagnostics = AK::ValidateEditorRuntimeBridge(editorRuntimeBridge);
        frame.useLayoutDrivenShell = editorRuntimeDiagnostics.ok;
        frame.shellLayout = BuildRenderShellLayoutFromEditorRuntime(editorRuntimeBridge);
        if (!frame.shellLayout.valid)
        {
            frame.useLayoutDrivenShell = false;
        }
        editorUXDiagnostics = AK::ValidateEditorUXRuntime(editorUXRuntime);
        editorFocusDiagnostics = AK::ValidateEditorFocusState(editorFocusState);
        frame.popupSurfaces = BuildRenderPopupsFromEditorUXRuntime(editorUXRuntime);
        AK::EditorShellPopupDesc menuToolbarPopup{};
        if (BuildRenderPopupFromMenuToolbarRuntime(editorMenuToolbarRuntime, menuToolbarPopup))
        {
            frame.popupSurfaces.push_back(std::move(menuToolbarPopup));
        }
        frame.commandPalette = BuildRenderCommandPaletteSurface(editorCommandPaletteRuntime.surface);

        renderer.DrawEditorShell(window, frame);
        diagnosticsHub.RecordZone("Editor.RenderShell", diagnosticsRenderStopwatch.ElapsedSeconds() * 1000.0);

        if (titleTimer.ElapsedSeconds() >= 1.0)
        {
            window.SetTitle("AK Engine Editor v10.6 - " + scene.Name() + (dirty ? " *" : ""));
            titleTimer.Reset();
        }

        AK::EditorAutosaveTickInput editorAutosaveInput{};
        editorAutosaveInput.deltaSeconds = frameTiming.frameDeltaSeconds;
        editorAutosaveInput.editorActive = window.IsOpen();
        editorAutosaveInput.sceneDirty = dirty;
        editorAutosaveInput.sceneRevision = dirtyRevision;
        editorAutosaveInput.workspaceDirty = editorRuntimeBridge.revision != editorAutosaveState.lastWorkspaceRevision;
        editorAutosaveInput.workspaceRevision = editorRuntimeBridge.revision;
        const AK::EditorAutosaveTickResult editorAutosaveTick = AK::TickEditorAutosaveRuntime(editorAutosavePolicy, editorAutosavePaths, editorAutosaveState, editorAutosaveInput, scene.Name());
        if (editorAutosaveTick.triggered)
        {
            for (const AK::EditorAutosaveAction& action : editorAutosaveTick.actions)
            {
                if (action.kind == AK::EditorAutosaveActionKind::SceneBackup)
                {
                    std::filesystem::create_directories(std::filesystem::path(action.path).parent_path());
                    if (scene.SaveToFile(action.path))
                    {
                        AddConsoleLine(consoleLines, "Scene backup: " + action.path);
                    }
                }
                else if (action.kind == AK::EditorAutosaveActionKind::SceneAutosave)
                {
                    std::filesystem::create_directories(std::filesystem::path(action.path).parent_path());
                    if (scene.SaveToFile(action.path))
                    {
                        lastAutosaveRevision = dirtyRevision;
                        AK::NotifyEditorAutosaveSceneSaved(editorAutosaveState, dirtyRevision);
                        AddConsoleLine(consoleLines, "Autosaved scene: " + action.path);
                    }
                }
                else if (action.kind == AK::EditorAutosaveActionKind::WorkspaceSave)
                {
                    AK::EditorSelectionModel autosaveSelection{};
                    if (const AK::EntityRecord* record = FindEntityRecord(scene, selectedEntity))
                    {
                        AK::SetSelection(autosaveSelection, {AK::EditorSelectionDomain::SceneEntity, selectedEntity, "entity:" + record->name, record->name});
                    }

                    std::vector<AK::EditorScrollState> autosaveScrollStates{hierarchyScroll, inspectorScroll, projectGridScroll, consoleScroll};
                    AK::EditorSessionCaptureInput sessionInput{};
                    sessionInput.frame = &editorRuntimeBridge.plan.frame;
                    sessionInput.selection = &autosaveSelection;
                    sessionInput.scrollStates = &autosaveScrollStates;
                    sessionInput.focusedPanel = editorRuntimeBridge.plan.focusedPanel;
                    sessionInput.projectName = "Sandbox";
                    sessionInput.scenePath = sandboxScenePath.generic_string();
                    AK::EditorSessionState session = AK::CaptureEditorSessionState(sessionInput, editorRuntimeBridge.panels);
                    AK::EditorWindowPlacement placement = AK::MakeDefaultEditorWindowPlacement(static_cast<AK::i32>(window.Width()), static_cast<AK::i32>(window.Height()));

                    AK::EditorWorkspaceSaveInput saveInput{};
                    saveInput.paths = editorWorkspacePaths;
                    saveInput.preferences = &editorWorkspaceLoad.preferences.profile;
                    saveInput.shortcuts = &editorWorkspaceLoad.shortcuts.profile;
                    saveInput.layout = &editorRuntimeBridge.plan.frame;
                    saveInput.placement = &placement;
                    saveInput.session = &session;
                    const AK::EditorWorkspaceSaveResult saveResult = AK::SaveEditorWorkspace(saveInput);
                    if (saveResult.ok)
                    {
                        AK::NotifyEditorAutosaveWorkspaceSaved(editorAutosaveState, editorRuntimeBridge.revision);
                        AddConsoleLine(consoleLines, saveResult.summary);
                    }
                    else
                    {
                        AddConsoleLine(consoleLines, "Workspace autosave failed: " + saveResult.summary);
                    }
                }
            }
        }

        diagnosticsHub.RecordZone("Editor.Frame", diagnosticsFrameStopwatch.ElapsedSeconds() * 1000.0);
        diagnosticsHub.EndFrame();
        renderer.EndFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    if (dirty)
    {
        std::filesystem::create_directories(autosavePath.parent_path());
        scene.SaveToFile(autosavePath);
    }

    editorJobSystem.Shutdown();
    renderer.Shutdown();
    AK::LogInfo("AK Editor stopped");
    return 0;
}
