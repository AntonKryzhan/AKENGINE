#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorSearch.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorPointerButton
    {
        None,
        Left,
        Middle,
        Right
    };

    enum class EditorHitRegion
    {
        None,
        MenuBar,
        Toolbar,
        StatusBar,
        DockTab,
        DockPanelBody,
        DockSplitter,
        Overlay,
        EmptyDockSpace
    };

    struct EditorPointerEvent
    {
        i32 x = 0;
        i32 y = 0;
        EditorPointerButton button = EditorPointerButton::None;
        bool pressed = false;
        bool released = false;
        bool doubleClick = false;
    };

    struct EditorHitTestResult
    {
        EditorHitRegion region = EditorHitRegion::None;
        EditorPanelId panel{};
        EditorDockNodeId node{};
        u32 overlayId = 0;
        CommandId command = CommandId::CloseEditor;
        std::size_t itemIndex = 0;
        bool actionable = false;
        bool active = false;
        std::string label;
    };

    struct EditorCommandRoute
    {
        CommandId command = CommandId::CloseEditor;
        CommandSource source = CommandSource::Programmatic;
        std::string label;
        bool enabled = false;
        bool destructive = false;
        bool requiresSelection = false;
    };

    struct EditorPanelRuntimeState
    {
        EditorPanelId panel{};
        EditorDockNodeId node{};
        EditorRect bodyRect{};
        EditorRect tabRect{};
        bool visible = false;
        bool active = false;
        bool focused = false;
        bool hovered = false;
        u64 lastInteractionRevision = 0;
    };

    struct EditorDockSplitterHandle
    {
        EditorDockNodeId node{};
        EditorDockSplitAxis axis = EditorDockSplitAxis::X;
        EditorRect rect{};
        float weight = 0.5f;
        bool draggable = true;
    };

    struct EditorRuntimeFramePlan
    {
        EditorFrameLayout frame{};
        EditorMenuModel menu{};
        EditorToolbarModel toolbar{};
        EditorStatusBarModel status{};
        std::vector<EditorOverlayDesc> overlays;
        std::vector<EditorPanelRuntimeState> panels;
        std::vector<EditorDockSplitterHandle> splitters;
        std::vector<EditorCommandRoute> commandRoutes;
        EditorPanelId focusedPanel{};
        EditorHitTestResult hover{};
        u64 revision = 1;
    };

    struct EditorRuntimeDiagnostics
    {
        std::size_t panelStateCount = 0;
        std::size_t activePanelCount = 0;
        std::size_t focusedPanelCount = 0;
        std::size_t splitterCount = 0;
        std::size_t commandRouteCount = 0;
        std::size_t disabledCommandCount = 0;
        std::size_t invalidPanelStateCount = 0;
        std::size_t invalidRectCount = 0;
        bool ok = false;
        std::string summary;
    };

    struct EditorRuntimeBridge
    {
        EditorPanelRegistry panels{};
        CommandRegistry commands{};
        EditorRuntimeFramePlan plan{};
        EditorSelectionModel selection{};
        EditorSearchIndex searchIndex{};
        u64 revision = 1;
    };

    const char* ToString(EditorPointerButton button);
    const char* ToString(EditorHitRegion region);

    EditorRuntimeBridge BuildDefaultEditorRuntimeBridge(i32 width, i32 height);
    void ResizeEditorRuntimeBridge(EditorRuntimeBridge& bridge, i32 width, i32 height);
    void FocusEditorPanel(EditorRuntimeBridge& bridge, EditorPanelId panel);
    bool ActivateEditorPanel(EditorRuntimeBridge& bridge, EditorPanelId panel);
    bool ToggleEditorOverlay(EditorRuntimeBridge& bridge, u32 overlayId, bool visible);
    bool SetEditorCommandEnabled(EditorRuntimeBridge& bridge, CommandId command, bool enabled);

    EditorHitTestResult HitTestEditorRuntime(const EditorRuntimeFramePlan& plan, i32 x, i32 y);
    std::vector<CommandInvocation> RouteEditorPointerEvent(EditorRuntimeBridge& bridge, const EditorPointerEvent& event);
    std::vector<EditorSearchResult> QueryEditorRuntime(const EditorRuntimeBridge& bridge, std::string text, std::size_t maxResults = 16);

    EditorRuntimeDiagnostics ValidateEditorRuntimeBridge(const EditorRuntimeBridge& bridge);
    std::string SerializeEditorRuntimeLayout(const EditorRuntimeBridge& bridge);
    Result<EditorRuntimeBridge> DeserializeEditorRuntimeLayout(std::string_view text, i32 width, i32 height);

    std::string FormatEditorHitTestResult(const EditorHitTestResult& hit);
    std::string FormatEditorCommandRoute(const EditorCommandRoute& route);
    std::string FormatEditorPanelRuntimeState(const EditorPanelRuntimeState& state);
    std::string FormatEditorRuntimeDiagnostics(const EditorRuntimeDiagnostics& diagnostics);
}
