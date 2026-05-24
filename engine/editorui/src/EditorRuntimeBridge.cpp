#include <AK/EditorUI/EditorRuntimeBridge.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace AK
{
    namespace
    {
        constexpr i32 TabHeaderHeight = 24;
        constexpr i32 SplitterHitSize = 8;

        bool Contains(EditorRect rect, i32 x, i32 y)
        {
            return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
        }

        bool ValidRect(EditorRect rect)
        {
            return rect.width >= 0 && rect.height >= 0;
        }

        EditorRect Inflate(EditorRect rect, i32 amount)
        {
            return {rect.x - amount, rect.y - amount, rect.width + amount * 2, rect.height + amount * 2};
        }

        EditorRect BuildTabRect(const EditorDockNode& node, std::size_t tabIndex)
        {
            constexpr i32 MinTabWidth = 88;
            constexpr i32 MaxTabWidth = 172;
            const i32 tabCount = std::max<i32>(1, static_cast<i32>(node.tabs.size()));
            const i32 tabWidth = std::clamp(node.rect.width / tabCount, MinTabWidth, MaxTabWidth);
            return {node.rect.x + static_cast<i32>(tabIndex) * tabWidth, node.rect.y, tabWidth, TabHeaderHeight};
        }

        EditorRect BuildSplitterRect(const EditorDockLayout& layout, const EditorDockNode& node)
        {
            const EditorDockNode* first = FindDockNode(layout, node.first);
            const EditorDockNode* second = FindDockNode(layout, node.second);
            if (!first || !second)
            {
                return {};
            }

            if (node.axis == EditorDockSplitAxis::X)
            {
                const i32 x = first->rect.x + first->rect.width;
                return {x - SplitterHitSize / 2, node.rect.y, SplitterHitSize, node.rect.height};
            }

            const i32 y = first->rect.y + first->rect.height;
            return {node.rect.x, y - SplitterHitSize / 2, node.rect.width, SplitterHitSize};
        }

        void CollectSplitters(const EditorDockLayout& layout, EditorDockNodeId id, std::vector<EditorDockSplitterHandle>& splitters)
        {
            const EditorDockNode* node = FindDockNode(layout, id);
            if (!node || node->kind != EditorDockNodeKind::Split)
            {
                return;
            }

            EditorDockSplitterHandle handle{};
            handle.node = node->id;
            handle.axis = node->axis;
            handle.rect = BuildSplitterRect(layout, *node);
            handle.weight = node->splitWeight;
            handle.draggable = !node->locked;
            splitters.push_back(handle);

            CollectSplitters(layout, node->first, splitters);
            CollectSplitters(layout, node->second, splitters);
        }

        bool CommandEnabledByDefault(CommandId command)
        {
            switch (command)
            {
                case CommandId::DeleteSelection:
                case CommandId::DuplicateSelection:
                case CommandId::RenameSelection:
                case CommandId::FocusSelection:
                    return false;
                default:
                    return true;
            }
        }

        std::vector<EditorCommandRoute> BuildCommandRoutes(const CommandRegistry& commands, const EditorMenuModel& menu, const EditorToolbarModel& toolbar)
        {
            std::vector<EditorCommandRoute> routes;
            for (const EditorMenuItem& item : menu.items)
            {
                if (item.kind != EditorMenuItemKind::Command)
                {
                    continue;
                }

                const CommandDescriptor* descriptor = commands.Find(item.command);
                EditorCommandRoute route{};
                route.command = item.command;
                route.source = CommandSource::Menu;
                route.label = item.label.empty() ? ToString(item.command) : item.label;
                route.enabled = item.enabled && CommandEnabledByDefault(item.command);
                route.destructive = item.destructive || (descriptor && descriptor->destructive);
                route.requiresSelection = descriptor && descriptor->requiresSelection;
                routes.push_back(std::move(route));
            }

            for (const EditorToolbarItem& item : toolbar.items)
            {
                if (item.kind != EditorToolbarItemKind::Command && item.kind != EditorToolbarItemKind::Toggle)
                {
                    continue;
                }

                const CommandDescriptor* descriptor = commands.Find(item.command);
                EditorCommandRoute route{};
                route.command = item.command;
                route.source = CommandSource::Toolbar;
                route.label = item.label.empty() ? ToString(item.command) : item.label;
                route.enabled = item.enabled && CommandEnabledByDefault(item.command);
                route.destructive = descriptor && descriptor->destructive;
                route.requiresSelection = descriptor && descriptor->requiresSelection;
                routes.push_back(std::move(route));
            }
            return routes;
        }

        EditorRuntimeFramePlan BuildRuntimeFramePlan(const EditorPanelRegistry& panels, const CommandRegistry& commands, i32 width, i32 height)
        {
            EditorRuntimeFramePlan plan{};
            plan.frame = BuildDefaultEditorFrameLayout(panels, width, height);
            ComputeEditorDockRects(plan.frame);
            const EditorDockDiagnostics dockDiagnostics = ValidateEditorDockLayout(plan.frame, panels);
            plan.menu = BuildDefaultEditorMenuModel(commands);
            plan.toolbar = BuildDefaultEditorToolbarModel(commands);
            plan.status = BuildDefaultEditorStatusBarModel(dockDiagnostics);
            plan.overlays = BuildDefaultSceneViewportOverlays(panels);
            plan.commandRoutes = BuildCommandRoutes(commands, plan.menu, plan.toolbar);
            plan.focusedPanel = panels.FindByName("scene.viewport") ? panels.FindByName("scene.viewport")->id : EditorPanelId{};

            for (const EditorPanelPlacement& placement : plan.frame.placements)
            {
                EditorPanelRuntimeState state{};
                state.panel = placement.panel;
                state.node = placement.node;
                state.bodyRect = placement.rect;
                if (const EditorDockNode* node = FindDockNode(plan.frame.dock, placement.node))
                {
                    for (std::size_t i = 0; i < node->tabs.size(); ++i)
                    {
                        if (node->tabs[i].panel == placement.panel)
                        {
                            state.tabRect = BuildTabRect(*node, i);
                            break;
                        }
                    }
                }
                state.visible = placement.visible;
                state.active = placement.active;
                state.focused = placement.panel == plan.focusedPanel;
                plan.panels.push_back(state);
            }

            CollectSplitters(plan.frame.dock, plan.frame.dock.root, plan.splitters);
            plan.revision = plan.frame.dock.revision;
            return plan;
        }

        void RebuildRuntimePlan(EditorRuntimeBridge& bridge, i32 width, i32 height)
        {
            const EditorPanelId focused = bridge.plan.focusedPanel;
            bridge.plan = BuildRuntimeFramePlan(bridge.panels, bridge.commands, width, height);
            if (focused.IsValid())
            {
                FocusEditorPanel(bridge, focused);
            }
            ++bridge.revision;
            bridge.plan.revision = bridge.revision;
        }
    }

    const char* ToString(EditorPointerButton button)
    {
        switch (button)
        {
            case EditorPointerButton::None: return "none";
            case EditorPointerButton::Left: return "left";
            case EditorPointerButton::Middle: return "middle";
            case EditorPointerButton::Right: return "right";
            default: return "unknown";
        }
    }

    const char* ToString(EditorHitRegion region)
    {
        switch (region)
        {
            case EditorHitRegion::None: return "none";
            case EditorHitRegion::MenuBar: return "menu-bar";
            case EditorHitRegion::Toolbar: return "toolbar";
            case EditorHitRegion::StatusBar: return "status-bar";
            case EditorHitRegion::DockTab: return "dock-tab";
            case EditorHitRegion::DockPanelBody: return "dock-panel-body";
            case EditorHitRegion::DockSplitter: return "dock-splitter";
            case EditorHitRegion::Overlay: return "overlay";
            case EditorHitRegion::EmptyDockSpace: return "empty-dock-space";
            default: return "unknown";
        }
    }

    EditorRuntimeBridge BuildDefaultEditorRuntimeBridge(i32 width, i32 height)
    {
        EditorRuntimeBridge bridge{};
        bridge.panels = BuildDefaultEditorPanelRegistry();
        bridge.commands = BuildDefaultEditorCommandRegistry();
        bridge.plan = BuildRuntimeFramePlan(bridge.panels, bridge.commands, width, height);
        bridge.selection = {};

        std::vector<EditorSelectionItem> mockEntities;
        mockEntities.push_back({EditorSelectionDomain::SceneEntity, EntityId{1, 1}, "entity:EditorCamera", "EditorCamera"});
        mockEntities.push_back({EditorSelectionDomain::SceneEntity, EntityId{2, 1}, "entity:SampleCube", "SampleCube"});
        mockEntities.push_back({EditorSelectionDomain::SceneEntity, EntityId{3, 1}, "entity:DirectionalLight", "DirectionalLight"});
        bridge.searchIndex = BuildDefaultEditorSearchIndex(bridge.commands, bridge.panels, mockEntities, {"builtin:cube", "builtin:default", "project.aksettings"});
        bridge.revision = bridge.plan.revision;
        return bridge;
    }

    void ResizeEditorRuntimeBridge(EditorRuntimeBridge& bridge, i32 width, i32 height)
    {
        RebuildRuntimePlan(bridge, width, height);
    }

    void FocusEditorPanel(EditorRuntimeBridge& bridge, EditorPanelId panel)
    {
        bridge.plan.focusedPanel = panel;
        for (EditorPanelRuntimeState& state : bridge.plan.panels)
        {
            state.focused = state.panel == panel;
            if (state.focused)
            {
                state.lastInteractionRevision = bridge.revision + 1;
            }
        }
        ++bridge.revision;
    }

    bool ActivateEditorPanel(EditorRuntimeBridge& bridge, EditorPanelId panel)
    {
        if (!ActivateDockTab(bridge.plan.frame.dock, panel))
        {
            return false;
        }

        const i32 width = bridge.plan.frame.width;
        const i32 height = bridge.plan.frame.height;
        RebuildRuntimePlan(bridge, width, height);
        FocusEditorPanel(bridge, panel);
        return true;
    }

    bool ToggleEditorOverlay(EditorRuntimeBridge& bridge, u32 overlayId, bool visible)
    {
        for (EditorOverlayDesc& overlay : bridge.plan.overlays)
        {
            if (overlay.id == overlayId)
            {
                overlay.visible = visible;
                ++bridge.revision;
                return true;
            }
        }
        return false;
    }

    bool SetEditorCommandEnabled(EditorRuntimeBridge& bridge, CommandId command, bool enabled)
    {
        bool changed = false;
        for (EditorCommandRoute& route : bridge.plan.commandRoutes)
        {
            if (route.command == command && route.enabled != enabled)
            {
                route.enabled = enabled;
                changed = true;
            }
        }
        if (changed)
        {
            ++bridge.revision;
        }
        return changed;
    }

    EditorHitTestResult HitTestEditorRuntime(const EditorRuntimeFramePlan& plan, i32 x, i32 y)
    {
        EditorHitTestResult result{};
        if (Contains(plan.frame.menuBar, x, y))
        {
            result.region = EditorHitRegion::MenuBar;
            result.label = "Main Menu";
            return result;
        }

        if (Contains(plan.frame.toolbar, x, y))
        {
            result.region = EditorHitRegion::Toolbar;
            result.label = "Toolbar";
            const i32 itemWidth = 86;
            const std::size_t index = static_cast<std::size_t>(std::max(0, x - plan.frame.toolbar.x) / itemWidth);
            std::size_t commandIndex = 0;
            for (const EditorToolbarItem& item : plan.toolbar.items)
            {
                if (item.kind != EditorToolbarItemKind::Command && item.kind != EditorToolbarItemKind::Toggle)
                {
                    continue;
                }
                if (commandIndex == index)
                {
                    result.command = item.command;
                    result.itemIndex = index;
                    result.actionable = item.enabled;
                    result.label = item.label;
                    return result;
                }
                ++commandIndex;
            }
            return result;
        }

        if (Contains(plan.frame.statusBar, x, y))
        {
            result.region = EditorHitRegion::StatusBar;
            result.label = "Status Bar";
            return result;
        }

        for (const EditorOverlayDesc& overlay : plan.overlays)
        {
            if (!overlay.visible)
            {
                continue;
            }
            if (Contains(overlay.anchorRect, x, y))
            {
                result.region = EditorHitRegion::Overlay;
                result.panel = overlay.targetPanel;
                result.overlayId = overlay.id;
                result.actionable = overlay.interactive;
                result.label = overlay.name;
                return result;
            }
        }

        for (const EditorDockSplitterHandle& splitter : plan.splitters)
        {
            if (Contains(Inflate(splitter.rect, 2), x, y))
            {
                result.region = EditorHitRegion::DockSplitter;
                result.node = splitter.node;
                result.actionable = splitter.draggable;
                result.label = splitter.axis == EditorDockSplitAxis::X ? "Vertical Splitter" : "Horizontal Splitter";
                return result;
            }
        }

        for (const EditorPanelRuntimeState& state : plan.panels)
        {
            if (state.visible && Contains(state.tabRect, x, y))
            {
                result.region = EditorHitRegion::DockTab;
                result.panel = state.panel;
                result.node = state.node;
                result.actionable = true;
                result.active = state.active;
                result.label = "Panel Tab";
                return result;
            }
        }

        for (const EditorPanelRuntimeState& state : plan.panels)
        {
            if (state.active && state.visible && Contains(state.bodyRect, x, y))
            {
                result.region = EditorHitRegion::DockPanelBody;
                result.panel = state.panel;
                result.node = state.node;
                result.actionable = true;
                result.active = true;
                result.label = "Panel Body";
                return result;
            }
        }

        if (Contains(plan.frame.dockSpace, x, y))
        {
            result.region = EditorHitRegion::EmptyDockSpace;
            result.label = "Dock Space";
        }
        return result;
    }

    std::vector<CommandInvocation> RouteEditorPointerEvent(EditorRuntimeBridge& bridge, const EditorPointerEvent& event)
    {
        std::vector<CommandInvocation> invocations;
        bridge.plan.hover = HitTestEditorRuntime(bridge.plan, event.x, event.y);
        if (event.button != EditorPointerButton::Left || !event.released)
        {
            return invocations;
        }

        const EditorHitTestResult hit = bridge.plan.hover;
        if (hit.region == EditorHitRegion::Toolbar && hit.actionable)
        {
            invocations.push_back({hit.command, CommandSource::Toolbar});
        }
        else if (hit.region == EditorHitRegion::DockTab && hit.panel.IsValid())
        {
            ActivateEditorPanel(bridge, hit.panel);
        }
        else if (hit.region == EditorHitRegion::DockPanelBody && hit.panel.IsValid())
        {
            FocusEditorPanel(bridge, hit.panel);
        }
        else if (hit.region == EditorHitRegion::Overlay && hit.overlayId != 0)
        {
            FocusEditorPanel(bridge, hit.panel);
        }
        return invocations;
    }

    std::vector<EditorSearchResult> QueryEditorRuntime(const EditorRuntimeBridge& bridge, std::string text, std::size_t maxResults)
    {
        return SearchEditorIndex(bridge.searchIndex, {std::move(text), maxResults, false});
    }

    EditorRuntimeDiagnostics ValidateEditorRuntimeBridge(const EditorRuntimeBridge& bridge)
    {
        EditorRuntimeDiagnostics diagnostics{};
        diagnostics.panelStateCount = bridge.plan.panels.size();
        diagnostics.splitterCount = bridge.plan.splitters.size();
        diagnostics.commandRouteCount = bridge.plan.commandRoutes.size();

        std::unordered_set<u32> seenPanels;
        for (const EditorPanelRuntimeState& state : bridge.plan.panels)
        {
            if (!bridge.panels.Has(state.panel) || !seenPanels.insert(state.panel.value).second)
            {
                ++diagnostics.invalidPanelStateCount;
            }
            if (state.active)
            {
                ++diagnostics.activePanelCount;
            }
            if (state.focused)
            {
                ++diagnostics.focusedPanelCount;
            }
            if (!ValidRect(state.bodyRect) || !ValidRect(state.tabRect))
            {
                ++diagnostics.invalidRectCount;
            }
        }

        for (const EditorDockSplitterHandle& splitter : bridge.plan.splitters)
        {
            if (!ValidRect(splitter.rect))
            {
                ++diagnostics.invalidRectCount;
            }
        }

        for (const EditorCommandRoute& route : bridge.plan.commandRoutes)
        {
            if (!route.enabled)
            {
                ++diagnostics.disabledCommandCount;
            }
        }

        diagnostics.ok = diagnostics.panelStateCount >= 8
            && diagnostics.activePanelCount >= 4
            && diagnostics.focusedPanelCount == 1
            && diagnostics.splitterCount >= 3
            && diagnostics.commandRouteCount >= 16
            && diagnostics.invalidPanelStateCount == 0
            && diagnostics.invalidRectCount == 0;

        diagnostics.summary = FormatEditorRuntimeDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string SerializeEditorRuntimeLayout(const EditorRuntimeBridge& bridge)
    {
        std::ostringstream out;
        out << "AKEDITORRUNTIME 1\n";
        out << "size " << bridge.plan.frame.width << ' ' << bridge.plan.frame.height << "\n";
        out << "focused " << bridge.plan.focusedPanel.value << "\n";
        out << SerializeEditorDockLayout(bridge.plan.frame);
        return out.str();
    }

    Result<EditorRuntimeBridge> DeserializeEditorRuntimeLayout(std::string_view text, i32 width, i32 height)
    {
        const std::string source(text);
        const std::size_t marker = source.find("AKLAYOUT 1");
        if (marker == std::string::npos)
        {
            return Result<EditorRuntimeBridge>(MakeError(ErrorCode::ParseError, "AKEDITORRUNTIME layout is missing embedded AKLAYOUT 1 payload"));
        }

        EditorRuntimeBridge bridge = BuildDefaultEditorRuntimeBridge(width, height);
        Result<EditorFrameLayout> frameResult = DeserializeEditorDockLayout(std::string_view(source).substr(marker), bridge.panels);
        if (!frameResult)
        {
            return Result<EditorRuntimeBridge>(frameResult.GetError());
        }

        bridge.plan.frame = frameResult.Value();
        bridge.plan.frame.width = width;
        bridge.plan.frame.height = height;
        ComputeEditorDockRects(bridge.plan.frame);
        RebuildRuntimePlan(bridge, width, height);
        return Ok(std::move(bridge));
    }

    std::string FormatEditorHitTestResult(const EditorHitTestResult& hit)
    {
        std::ostringstream out;
        out << "hit region=" << ToString(hit.region)
            << " panel=" << hit.panel.value
            << " node=" << hit.node.value
            << " overlay=" << hit.overlayId
            << " command=" << ToString(hit.command)
            << " actionable=" << (hit.actionable ? "true" : "false")
            << " label='" << hit.label << "'";
        return out.str();
    }

    std::string FormatEditorCommandRoute(const EditorCommandRoute& route)
    {
        std::ostringstream out;
        out << "route " << ToString(route.command)
            << " source=" << ToString(route.source)
            << " enabled=" << (route.enabled ? "true" : "false")
            << " destructive=" << (route.destructive ? "true" : "false")
            << " selection=" << (route.requiresSelection ? "true" : "false")
            << " label='" << route.label << "'";
        return out.str();
    }

    std::string FormatEditorPanelRuntimeState(const EditorPanelRuntimeState& state)
    {
        std::ostringstream out;
        out << "panel-state panel=" << state.panel.value
            << " node=" << state.node.value
            << " body=" << FormatEditorRect(state.bodyRect)
            << " tab=" << FormatEditorRect(state.tabRect)
            << " active=" << (state.active ? "true" : "false")
            << " focused=" << (state.focused ? "true" : "false");
        return out.str();
    }

    std::string FormatEditorRuntimeDiagnostics(const EditorRuntimeDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-runtime panels=" << diagnostics.panelStateCount
            << " active=" << diagnostics.activePanelCount
            << " focused=" << diagnostics.focusedPanelCount
            << " splitters=" << diagnostics.splitterCount
            << " routes=" << diagnostics.commandRouteCount
            << " disabled=" << diagnostics.disabledCommandCount
            << " invalidPanels=" << diagnostics.invalidPanelStateCount
            << " invalidRects=" << diagnostics.invalidRectCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        return out.str();
    }
}
