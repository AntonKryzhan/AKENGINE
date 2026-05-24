#pragma once

#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorPanel.hpp>

#include <string>
#include <vector>

namespace AK
{
    struct EditorRect
    {
        i32 x = 0;
        i32 y = 0;
        i32 width = 0;
        i32 height = 0;
    };

    struct EditorDockNodeId final
    {
        u32 value = 0;

        constexpr bool IsValid() const
        {
            return value != 0;
        }
    };

    constexpr bool operator==(EditorDockNodeId a, EditorDockNodeId b)
    {
        return a.value == b.value;
    }

    constexpr bool operator!=(EditorDockNodeId a, EditorDockNodeId b)
    {
        return !(a == b);
    }

    enum class EditorDockNodeKind
    {
        Split,
        Stack
    };

    enum class EditorDockSplitAxis
    {
        X,
        Y
    };

    enum class EditorDockZone
    {
        Left,
        Center,
        Right,
        Bottom,
        Floating
    };

    struct EditorDockTab
    {
        EditorPanelId panel{};
        std::string title;
        bool pinned = false;
        bool dirty = false;
    };

    struct EditorDockNode
    {
        EditorDockNodeId id{};
        EditorDockNodeKind kind = EditorDockNodeKind::Stack;
        EditorDockSplitAxis axis = EditorDockSplitAxis::X;
        float splitWeight = 0.5f;
        EditorDockNodeId first{};
        EditorDockNodeId second{};
        EditorDockZone zone = EditorDockZone::Center;
        std::vector<EditorDockTab> tabs;
        std::size_t activeTab = 0;
        EditorRect rect{};
        bool locked = false;
    };

    struct EditorPanelPlacement
    {
        EditorPanelId panel{};
        EditorDockNodeId node{};
        EditorRect rect{};
        bool visible = false;
        bool active = false;
    };

    struct EditorDockLayout
    {
        u32 schemaVersion = 1;
        EditorDockNodeId root{};
        std::vector<EditorDockNode> nodes;
        u64 revision = 1;
    };

    struct EditorFrameLayout
    {
        i32 width = 1600;
        i32 height = 900;
        EditorRect menuBar{};
        EditorRect toolbar{};
        EditorRect dockSpace{};
        EditorRect statusBar{};
        EditorDockLayout dock{};
        std::vector<EditorPanelPlacement> placements;
    };

    struct EditorDockDiagnostics
    {
        std::size_t nodeCount = 0;
        std::size_t splitCount = 0;
        std::size_t stackCount = 0;
        std::size_t tabCount = 0;
        std::size_t visiblePanelCount = 0;
        std::size_t duplicatePanelCount = 0;
        std::size_t invalidReferenceCount = 0;
        std::size_t emptyStackCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorDockNodeKind kind);
    const char* ToString(EditorDockSplitAxis axis);
    const char* ToString(EditorDockZone zone);

    EditorDockNode* FindDockNode(EditorDockLayout& layout, EditorDockNodeId id);
    const EditorDockNode* FindDockNode(const EditorDockLayout& layout, EditorDockNodeId id);
    EditorDockNodeId AddDockStack(EditorDockLayout& layout, EditorDockZone zone);
    EditorDockNodeId AddDockSplit(EditorDockLayout& layout, EditorDockSplitAxis axis, float splitWeight, EditorDockNodeId first, EditorDockNodeId second);
    bool AddDockTab(EditorDockLayout& layout, EditorDockNodeId stack, EditorPanelId panel, std::string title, bool pinned = false);
    bool ActivateDockTab(EditorDockLayout& layout, EditorPanelId panel);

    EditorFrameLayout BuildDefaultEditorFrameLayout(const EditorPanelRegistry& panels, i32 width, i32 height);
    void ComputeEditorDockRects(EditorFrameLayout& frame);
    EditorDockDiagnostics ValidateEditorDockLayout(const EditorFrameLayout& frame, const EditorPanelRegistry& panels);
    std::string SerializeEditorDockLayout(const EditorFrameLayout& frame);
    Result<EditorFrameLayout> DeserializeEditorDockLayout(std::string_view text, const EditorPanelRegistry& panels);
    std::string FormatEditorRect(EditorRect rect);
    std::string FormatEditorDockNode(const EditorDockNode& node);
}
