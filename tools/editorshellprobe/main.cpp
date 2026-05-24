#include <AK/EditorUI/EditorRuntimeBridge.hpp>
#include <AK/Render/Renderer.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    AK::EditorShellRect ToShellRect(AK::EditorRect rect)
    {
        return {rect.x, rect.y, rect.width, rect.height};
    }
}

int main()
{
    AK::EditorRuntimeBridge bridge = AK::BuildDefaultEditorRuntimeBridge(1366, 768);
    AK::SetEditorCommandEnabled(bridge, AK::CommandId::DeleteSelection, true);
    AK::SetEditorCommandEnabled(bridge, AK::CommandId::DuplicateSelection, true);
    AK::SetEditorCommandEnabled(bridge, AK::CommandId::FocusSelection, true);

    const AK::EditorRuntimeDiagnostics diagnostics = AK::ValidateEditorRuntimeBridge(bridge);
    if (!diagnostics.ok)
    {
        std::cerr << "editor shell runtime diagnostics failed: " << diagnostics.summary << '\n';
        return EXIT_FAILURE;
    }

    AK::EditorFrameDesc frame{};
    frame.title = "AK Engine Editor v7.7";
    frame.useLayoutDrivenShell = true;
    frame.shellLayout.menuBar = ToShellRect(bridge.plan.frame.menuBar);
    frame.shellLayout.toolbar = ToShellRect(bridge.plan.frame.toolbar);
    frame.shellLayout.dockSpace = ToShellRect(bridge.plan.frame.dockSpace);
    frame.shellLayout.statusBar = ToShellRect(bridge.plan.frame.statusBar);
    frame.shellLayout.statusLeft = "Ready";
    frame.shellLayout.statusRight = diagnostics.summary;

    for (const AK::EditorPanelRuntimeState& state : bridge.plan.panels)
    {
        const AK::EditorPanelDesc* panel = bridge.panels.Find(state.panel);
        AK::EditorShellPanelDesc out{};
        out.id = state.panel.value;
        out.name = panel ? panel->name : "unknown";
        out.title = panel ? panel->title : out.name;
        out.kind = panel ? AK::ToString(panel->kind) : "custom";
        out.tabRect = ToShellRect(state.tabRect);
        out.bodyRect = ToShellRect(state.bodyRect);
        out.visible = state.visible;
        out.active = state.active;
        out.focused = state.focused;
        frame.shellLayout.panels.push_back(out);
    }

    for (const AK::EditorDockSplitterHandle& splitter : bridge.plan.splitters)
    {
        AK::EditorShellSplitterDesc out{};
        out.rect = ToShellRect(splitter.rect);
        out.vertical = splitter.axis == AK::EditorDockSplitAxis::X;
        out.draggable = splitter.draggable;
        frame.shellLayout.splitters.push_back(out);
    }

    std::size_t toolbarCommands = 0;
    for (const AK::EditorToolbarItem& item : bridge.plan.toolbar.items)
    {
        if (item.kind == AK::EditorToolbarItemKind::Command || item.kind == AK::EditorToolbarItemKind::Toggle)
        {
            AK::EditorShellToolbarButtonDesc button{};
            button.rect = {8 + static_cast<AK::i32>(toolbarCommands) * 86, 31, 82, 28};
            button.label = item.label;
            button.enabled = item.enabled;
            button.active = item.active;
            frame.shellLayout.toolbarButtons.push_back(button);
            ++toolbarCommands;
        }
    }

    frame.shellLayout.valid = frame.useLayoutDrivenShell
        && frame.shellLayout.menuBar.width > 0
        && frame.shellLayout.toolbar.width > 0
        && frame.shellLayout.statusBar.width > 0
        && frame.shellLayout.panels.size() >= 8
        && frame.shellLayout.splitters.size() >= 3
        && frame.shellLayout.toolbarButtons.size() >= 8;

    if (!frame.shellLayout.valid)
    {
        std::cerr << "layout-driven render shell desc is invalid\n";
        return EXIT_FAILURE;
    }

    std::cout << "[ ok ] editor shell integration / layout-driven gdi bridge"
              << " panels=" << frame.shellLayout.panels.size()
              << " active=" << diagnostics.activePanelCount
              << " splitters=" << frame.shellLayout.splitters.size()
              << " toolbar=" << frame.shellLayout.toolbarButtons.size()
              << " routes=" << diagnostics.commandRouteCount
              << " focused=" << diagnostics.focusedPanelCount
              << " valid=" << (frame.shellLayout.valid ? "true" : "false")
              << '\n';
    return EXIT_SUCCESS;
}
