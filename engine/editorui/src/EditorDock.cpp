#include <AK/EditorUI/EditorDock.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace AK
{
    namespace
    {
        constexpr i32 MenuHeight = 22;
        constexpr i32 ToolbarHeight = 38;
        constexpr i32 StatusHeight = 22;
        constexpr i32 DockGap = 4;
        constexpr i32 TabHeaderHeight = 24;

        u32 AllocateNodeId(const EditorDockLayout& layout)
        {
            u32 maxId = 0;
            for (const EditorDockNode& node : layout.nodes)
            {
                maxId = std::max(maxId, node.id.value);
            }
            return maxId + 1u;
        }

        i32 ClampSize(i32 value)
        {
            return std::max(0, value);
        }

        bool IsStack(const EditorDockNode* node)
        {
            return node && node->kind == EditorDockNodeKind::Stack;
        }

        void ComputeNodeRect(EditorDockLayout& layout, EditorDockNodeId id, EditorRect rect)
        {
            EditorDockNode* node = FindDockNode(layout, id);
            if (!node)
            {
                return;
            }

            node->rect = rect;
            if (node->kind != EditorDockNodeKind::Split)
            {
                return;
            }

            const float weight = std::clamp(node->splitWeight, 0.05f, 0.95f);
            if (node->axis == EditorDockSplitAxis::X)
            {
                const i32 firstWidth = static_cast<i32>(std::round(static_cast<float>(rect.width - DockGap) * weight));
                const EditorRect firstRect{rect.x, rect.y, ClampSize(firstWidth), rect.height};
                const EditorRect secondRect{rect.x + firstWidth + DockGap, rect.y, ClampSize(rect.width - firstWidth - DockGap), rect.height};
                ComputeNodeRect(layout, node->first, firstRect);
                ComputeNodeRect(layout, node->second, secondRect);
            }
            else
            {
                const i32 firstHeight = static_cast<i32>(std::round(static_cast<float>(rect.height - DockGap) * weight));
                const EditorRect firstRect{rect.x, rect.y, rect.width, ClampSize(firstHeight)};
                const EditorRect secondRect{rect.x, rect.y + firstHeight + DockGap, rect.width, ClampSize(rect.height - firstHeight - DockGap)};
                ComputeNodeRect(layout, node->first, firstRect);
                ComputeNodeRect(layout, node->second, secondRect);
            }
        }

        void CollectPlacements(const EditorDockLayout& layout, EditorDockNodeId id, std::vector<EditorPanelPlacement>& placements)
        {
            const EditorDockNode* node = FindDockNode(layout, id);
            if (!node)
            {
                return;
            }

            if (node->kind == EditorDockNodeKind::Split)
            {
                CollectPlacements(layout, node->first, placements);
                CollectPlacements(layout, node->second, placements);
                return;
            }

            const EditorRect panelRect{
                node->rect.x,
                node->rect.y + TabHeaderHeight,
                node->rect.width,
                ClampSize(node->rect.height - TabHeaderHeight)
            };

            for (std::size_t i = 0; i < node->tabs.size(); ++i)
            {
                EditorPanelPlacement placement{};
                placement.panel = node->tabs[i].panel;
                placement.node = node->id;
                placement.rect = panelRect;
                placement.visible = true;
                placement.active = i == node->activeTab;
                placements.push_back(placement);
            }
        }

        EditorPanelId PanelId(const EditorPanelRegistry& panels, const char* name)
        {
            const EditorPanelDesc* panel = panels.FindByName(name);
            return panel ? panel->id : EditorPanelId{};
        }

    }

    const char* ToString(EditorDockNodeKind kind)
    {
        switch (kind)
        {
            case EditorDockNodeKind::Split: return "split";
            case EditorDockNodeKind::Stack: return "stack";
            default: return "unknown";
        }
    }

    const char* ToString(EditorDockSplitAxis axis)
    {
        switch (axis)
        {
            case EditorDockSplitAxis::X: return "x";
            case EditorDockSplitAxis::Y: return "y";
            default: return "unknown";
        }
    }

    const char* ToString(EditorDockZone zone)
    {
        switch (zone)
        {
            case EditorDockZone::Left: return "left";
            case EditorDockZone::Center: return "center";
            case EditorDockZone::Right: return "right";
            case EditorDockZone::Bottom: return "bottom";
            case EditorDockZone::Floating: return "floating";
            default: return "unknown";
        }
    }

    EditorDockNode* FindDockNode(EditorDockLayout& layout, EditorDockNodeId id)
    {
        for (EditorDockNode& node : layout.nodes)
        {
            if (node.id == id)
            {
                return &node;
            }
        }
        return nullptr;
    }

    const EditorDockNode* FindDockNode(const EditorDockLayout& layout, EditorDockNodeId id)
    {
        for (const EditorDockNode& node : layout.nodes)
        {
            if (node.id == id)
            {
                return &node;
            }
        }
        return nullptr;
    }

    EditorDockNodeId AddDockStack(EditorDockLayout& layout, EditorDockZone zone)
    {
        EditorDockNode node{};
        node.id.value = AllocateNodeId(layout);
        node.kind = EditorDockNodeKind::Stack;
        node.zone = zone;
        layout.nodes.push_back(node);
        ++layout.revision;
        return node.id;
    }

    EditorDockNodeId AddDockSplit(EditorDockLayout& layout, EditorDockSplitAxis axis, float splitWeight, EditorDockNodeId first, EditorDockNodeId second)
    {
        EditorDockNode node{};
        node.id.value = AllocateNodeId(layout);
        node.kind = EditorDockNodeKind::Split;
        node.axis = axis;
        node.splitWeight = std::clamp(splitWeight, 0.05f, 0.95f);
        node.first = first;
        node.second = second;
        layout.nodes.push_back(node);
        ++layout.revision;
        return node.id;
    }

    bool AddDockTab(EditorDockLayout& layout, EditorDockNodeId stack, EditorPanelId panel, std::string title, bool pinned)
    {
        EditorDockNode* node = FindDockNode(layout, stack);
        if (!IsStack(node) || !panel.IsValid())
        {
            return false;
        }

        node->tabs.push_back({panel, std::move(title), pinned, false});
        if (node->tabs.size() == 1)
        {
            node->activeTab = 0;
        }
        ++layout.revision;
        return true;
    }

    bool ActivateDockTab(EditorDockLayout& layout, EditorPanelId panel)
    {
        for (EditorDockNode& node : layout.nodes)
        {
            if (node.kind != EditorDockNodeKind::Stack)
            {
                continue;
            }

            for (std::size_t i = 0; i < node.tabs.size(); ++i)
            {
                if (node.tabs[i].panel == panel)
                {
                    node.activeTab = i;
                    ++layout.revision;
                    return true;
                }
            }
        }
        return false;
    }

    EditorFrameLayout BuildDefaultEditorFrameLayout(const EditorPanelRegistry& panels, i32 width, i32 height)
    {
        EditorFrameLayout frame{};
        frame.width = std::max<i32>(900, width);
        frame.height = std::max<i32>(540, height);
        frame.menuBar = {0, 0, frame.width, MenuHeight};
        frame.toolbar = {0, MenuHeight, frame.width, ToolbarHeight};
        frame.statusBar = {0, frame.height - StatusHeight, frame.width, StatusHeight};
        frame.dockSpace = {DockGap, MenuHeight + ToolbarHeight + DockGap, frame.width - 2 * DockGap, frame.height - MenuHeight - ToolbarHeight - StatusHeight - 2 * DockGap};

        EditorDockLayout& dock = frame.dock;
        const EditorDockNodeId hierarchy = AddDockStack(dock, EditorDockZone::Left);
        const EditorDockNodeId viewport = AddDockStack(dock, EditorDockZone::Center);
        const EditorDockNodeId inspector = AddDockStack(dock, EditorDockZone::Right);
        const EditorDockNodeId bottom = AddDockStack(dock, EditorDockZone::Bottom);

        AddDockTab(dock, hierarchy, PanelId(panels, "scene.hierarchy"), "Hierarchy", true);
        AddDockTab(dock, hierarchy, PanelId(panels, "world.partition"), "World Partition", false);
        AddDockTab(dock, viewport, PanelId(panels, "scene.viewport"), "Scene", true);
        AddDockTab(dock, viewport, PanelId(panels, "game.viewport"), "Game", false);
        AddDockTab(dock, inspector, PanelId(panels, "inspector"), "Inspector", true);
        AddDockTab(dock, inspector, PanelId(panels, "diagnostics"), "Diagnostics", false);
        AddDockTab(dock, bottom, PanelId(panels, "assets.browser"), "Project", true);
        AddDockTab(dock, bottom, PanelId(panels, "console"), "Console", false);
        AddDockTab(dock, bottom, PanelId(panels, "render.graph"), "RenderGraph", false);

        const EditorDockNodeId centerWithBottom = AddDockSplit(dock, EditorDockSplitAxis::Y, 0.76f, viewport, bottom);
        const EditorDockNodeId centerRight = AddDockSplit(dock, EditorDockSplitAxis::X, 0.79f, centerWithBottom, inspector);
        dock.root = AddDockSplit(dock, EditorDockSplitAxis::X, 0.16f, hierarchy, centerRight);
        ComputeEditorDockRects(frame);
        return frame;
    }

    void ComputeEditorDockRects(EditorFrameLayout& frame)
    {
        ComputeNodeRect(frame.dock, frame.dock.root, frame.dockSpace);
        frame.placements.clear();
        CollectPlacements(frame.dock, frame.dock.root, frame.placements);
    }

    EditorDockDiagnostics ValidateEditorDockLayout(const EditorFrameLayout& frame, const EditorPanelRegistry& panels)
    {
        EditorDockDiagnostics diagnostics{};
        diagnostics.nodeCount = frame.dock.nodes.size();

        std::unordered_set<u32> nodeIds;
        std::unordered_set<u32> panelIds;
        for (const EditorDockNode& node : frame.dock.nodes)
        {
            if (!node.id.IsValid() || !nodeIds.insert(node.id.value).second)
            {
                ++diagnostics.invalidReferenceCount;
            }

            if (node.kind == EditorDockNodeKind::Split)
            {
                ++diagnostics.splitCount;
                if (!FindDockNode(frame.dock, node.first) || !FindDockNode(frame.dock, node.second))
                {
                    ++diagnostics.invalidReferenceCount;
                }
            }
            else
            {
                ++diagnostics.stackCount;
                if (node.tabs.empty())
                {
                    ++diagnostics.emptyStackCount;
                }
                if (!node.tabs.empty() && node.activeTab >= node.tabs.size())
                {
                    ++diagnostics.invalidReferenceCount;
                }
                for (const EditorDockTab& tab : node.tabs)
                {
                    ++diagnostics.tabCount;
                    if (!panels.Has(tab.panel))
                    {
                        ++diagnostics.invalidReferenceCount;
                    }
                    if (!panelIds.insert(tab.panel.value).second)
                    {
                        ++diagnostics.duplicatePanelCount;
                    }
                }
            }
        }

        diagnostics.visiblePanelCount = frame.placements.size();
        if (!frame.dock.root.IsValid() || !FindDockNode(frame.dock, frame.dock.root))
        {
            ++diagnostics.invalidReferenceCount;
        }

        diagnostics.ok = diagnostics.nodeCount >= 7
            && diagnostics.splitCount >= 3
            && diagnostics.stackCount >= 4
            && diagnostics.tabCount >= 8
            && diagnostics.invalidReferenceCount == 0
            && diagnostics.duplicatePanelCount == 0
            && diagnostics.emptyStackCount == 0;

        std::ostringstream out;
        out << "dock nodes=" << diagnostics.nodeCount
            << " splits=" << diagnostics.splitCount
            << " stacks=" << diagnostics.stackCount
            << " tabs=" << diagnostics.tabCount
            << " visible=" << diagnostics.visiblePanelCount
            << " invalid=" << diagnostics.invalidReferenceCount
            << " duplicates=" << diagnostics.duplicatePanelCount
            << " empty=" << diagnostics.emptyStackCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string SerializeEditorDockLayout(const EditorFrameLayout& frame)
    {
        std::ostringstream out;
        out << "AKLAYOUT 1\n";
        out << "frame " << frame.width << ' ' << frame.height << ' ' << frame.dock.root.value << '\n';
        for (const EditorDockNode& node : frame.dock.nodes)
        {
            out << "node " << node.id.value << ' ' << ToString(node.kind) << ' ' << ToString(node.axis) << ' ' << node.splitWeight
                << ' ' << node.first.value << ' ' << node.second.value << ' ' << ToString(node.zone) << ' ' << node.activeTab << '\n';
            for (const EditorDockTab& tab : node.tabs)
            {
                out << "tab " << node.id.value << ' ' << tab.panel.value << ' ' << (tab.pinned ? 1 : 0) << ' ' << (tab.dirty ? 1 : 0) << ' ' << tab.title << '\n';
            }
        }
        return out.str();
    }

    Result<EditorFrameLayout> DeserializeEditorDockLayout(std::string_view text, const EditorPanelRegistry& panels)
    {
        std::istringstream input{std::string(text)};
        std::string magic;
        u32 version = 0;
        input >> magic >> version;
        if (magic != "AKLAYOUT" || version != 1)
        {
            return MakeError(ErrorCode::ParseError, "Invalid AKLAYOUT header");
        }

        EditorFrameLayout frame{};
        std::string tag;
        input >> tag >> frame.width >> frame.height >> frame.dock.root.value;
        if (tag != "frame")
        {
            return MakeError(ErrorCode::ParseError, "Missing frame record");
        }

        std::string line;
        std::getline(input, line);
        while (std::getline(input, line))
        {
            if (line.empty())
            {
                continue;
            }

            std::istringstream lineInput(line);
            std::string record;
            lineInput >> record;
            if (record == "node")
            {
                EditorDockNode node{};
                std::string kind;
                std::string axis;
                std::string zone;
                lineInput >> node.id.value >> kind >> axis >> node.splitWeight >> node.first.value >> node.second.value >> zone >> node.activeTab;
                node.kind = kind == "split" ? EditorDockNodeKind::Split : EditorDockNodeKind::Stack;
                node.axis = axis == "y" ? EditorDockSplitAxis::Y : EditorDockSplitAxis::X;
                if (zone == "left") { node.zone = EditorDockZone::Left; }
                else if (zone == "right") { node.zone = EditorDockZone::Right; }
                else if (zone == "bottom") { node.zone = EditorDockZone::Bottom; }
                else if (zone == "floating") { node.zone = EditorDockZone::Floating; }
                else { node.zone = EditorDockZone::Center; }
                frame.dock.nodes.push_back(node);
            }
            else if (record == "tab")
            {
                u32 nodeIdValue = 0;
                u32 panelIdValue = 0;
                u32 pinned = 0;
                u32 dirty = 0;
                lineInput >> nodeIdValue >> panelIdValue >> pinned >> dirty;
                std::string title;
                std::getline(lineInput, title);
                if (!title.empty() && title.front() == ' ')
                {
                    title.erase(title.begin());
                }
                EditorDockNode* node = FindDockNode(frame.dock, EditorDockNodeId{nodeIdValue});
                if (!node)
                {
                    return MakeError(ErrorCode::ParseError, "Tab references unknown dock node");
                }
                EditorDockTab tab{};
                tab.panel = EditorPanelId{panelIdValue};
                tab.title = title;
                tab.pinned = pinned != 0;
                tab.dirty = dirty != 0;
                node->tabs.push_back(tab);
            }
        }

        frame.menuBar = {0, 0, frame.width, MenuHeight};
        frame.toolbar = {0, MenuHeight, frame.width, ToolbarHeight};
        frame.statusBar = {0, frame.height - StatusHeight, frame.width, StatusHeight};
        frame.dockSpace = {DockGap, MenuHeight + ToolbarHeight + DockGap, frame.width - 2 * DockGap, frame.height - MenuHeight - ToolbarHeight - StatusHeight - 2 * DockGap};
        ComputeEditorDockRects(frame);
        const EditorDockDiagnostics diagnostics = ValidateEditorDockLayout(frame, panels);
        if (!diagnostics.ok)
        {
            return MakeError(ErrorCode::InvalidState, diagnostics.summary);
        }
        return frame;
    }

    std::string FormatEditorRect(EditorRect rect)
    {
        std::ostringstream out;
        out << rect.x << ',' << rect.y << ' ' << rect.width << 'x' << rect.height;
        return out.str();
    }

    std::string FormatEditorDockNode(const EditorDockNode& node)
    {
        std::ostringstream out;
        out << "node=" << node.id.value << " kind=" << ToString(node.kind)
            << " zone=" << ToString(node.zone) << " rect=" << FormatEditorRect(node.rect)
            << " tabs=" << node.tabs.size();
        return out.str();
    }
}
