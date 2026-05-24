#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/EditorUI/EditorChrome.hpp>
#include <AK/EditorUI/EditorCommandState.hpp>
#include <AK/EditorUI/EditorRuntimeBridge.hpp>
#include <AK/EditorUI/EditorShortcuts.hpp>
#include <AK/EditorUI/EditorUXRuntime.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorMenuToolbarHitKind
    {
        None,
        Outside,
        MenuRoot,
        MenuPopupItem,
        ToolbarButton,
        ToolbarSearchBox,
        ToolbarEmpty
    };

    struct EditorMenuToolbarRootRuntime
    {
        std::string id;
        std::string label;
        EditorRect rect{};
        bool hovered = false;
        bool open = false;
        usize itemCount = 0;
    };

    struct EditorMenuToolbarCommandItemRuntime
    {
        std::string id;
        std::string label;
        std::string shortcutText;
        EditorIconKind icon = EditorIconKind::None;
        EditorRect rect{};
        CommandId command = CommandId::Count;
        bool enabled = true;
        bool checked = false;
        bool destructive = false;
        bool separatorBefore = false;
        bool hasSubmenu = false;
        i32 depth = 0;
    };

    struct EditorMenuToolbarButtonRuntime
    {
        std::string id;
        std::string label;
        std::string tooltip;
        EditorRect rect{};
        CommandId command = CommandId::Count;
        bool enabled = true;
        bool checked = false;
        bool destructive = false;
        bool hovered = false;
    };

    struct EditorMenuToolbarPopupSurface
    {
        std::string id;
        std::string title;
        EditorRect rect{};
        EditorRect anchor{};
        bool open = false;
        std::vector<EditorMenuToolbarCommandItemRuntime> items;
    };

    struct EditorMenuToolbarSurface
    {
        EditorRect menuBar{};
        EditorRect toolbar{};
        EditorRect searchBox{};
        std::vector<EditorMenuToolbarRootRuntime> roots;
        std::vector<EditorMenuToolbarButtonRuntime> toolbarButtons;
        EditorMenuToolbarPopupSurface popup{};
        u64 revision = 1;
    };

    struct EditorMenuToolbarHitTestResult
    {
        EditorMenuToolbarHitKind kind = EditorMenuToolbarHitKind::None;
        usize index = 0;
        std::string id;
        std::string label;
        CommandId command = CommandId::Count;
        bool enabled = false;
        bool destructive = false;
    };

    struct EditorMenuToolbarRuntimeState
    {
        std::string activeMenuId;
        EditorMenuToolbarSurface surface{};
        EditorMenuToolbarHitTestResult hover{};
        CommandInvocation lastInvocation{};
        bool menuOpen = false;
        bool mouseCapture = false;
        bool keyboardCapture = false;
        u64 revision = 1;
    };

    struct EditorMenuToolbarRuntimeInput
    {
        const EditorMenuModel* menu = nullptr;
        const EditorToolbarModel* toolbar = nullptr;
        const EditorCommandStateCache* commandState = nullptr;
        const EditorShortcutProfile* shortcuts = nullptr;
        EditorFrameLayout frame{};
        i32 viewportWidth = 1366;
        i32 viewportHeight = 768;
        i32 mouseX = 0;
        i32 mouseY = 0;
        bool mouseLeftPressed = false;
        bool keyEscapePressed = false;
    };

    struct EditorMenuToolbarRuntimeResult
    {
        bool handled = false;
        bool openedMenu = false;
        bool closedMenu = false;
        bool hoverChanged = false;
        bool commandQueued = false;
        bool openCommandPalette = false;
        CommandInvocation command{};
        std::string statusText;
    };

    struct EditorMenuToolbarRuntimeDiagnostics
    {
        EditorMenuToolbarRuntimeState runtime{};
        EditorMenuToolbarHitTestResult menuHit{};
        EditorMenuToolbarHitTestResult toolbarHit{};
        EditorMenuToolbarHitTestResult popupHit{};
        EditorMenuToolbarRuntimeResult openResult{};
        EditorMenuToolbarRuntimeResult commandResult{};
        usize rootCount = 0;
        usize toolbarButtonCount = 0;
        usize popupItemCount = 0;
        usize shortcutTextCount = 0;
        bool rootHitOk = false;
        bool popupCommandOk = false;
        bool toolbarCommandOk = false;
        bool searchBoxOk = false;
        bool commandStateOk = false;
        bool surfaceOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorMenuToolbarHitKind kind);

    EditorMenuToolbarRuntimeState MakeDefaultEditorMenuToolbarRuntimeState();

    EditorMenuToolbarSurface BuildEditorMenuToolbarSurface(const EditorMenuModel& menu,
                                                           const EditorToolbarModel& toolbar,
                                                           const EditorCommandStateCache& commandState,
                                                           const EditorShortcutProfile& shortcuts,
                                                           const EditorFrameLayout& frame,
                                                           std::string activeMenuId,
                                                           i32 viewportWidth,
                                                           i32 viewportHeight);

    EditorMenuToolbarHitTestResult HitTestEditorMenuToolbarSurface(const EditorMenuToolbarSurface& surface, i32 x, i32 y);

    void RefreshEditorMenuToolbarRuntimeSurface(EditorMenuToolbarRuntimeState& runtime,
                                                const EditorMenuModel& menu,
                                                const EditorToolbarModel& toolbar,
                                                const EditorCommandStateCache& commandState,
                                                const EditorShortcutProfile& shortcuts,
                                                const EditorFrameLayout& frame,
                                                i32 viewportWidth,
                                                i32 viewportHeight);

    EditorMenuToolbarRuntimeResult ApplyEditorMenuToolbarRuntimeInput(EditorMenuToolbarRuntimeState& runtime,
                                                                       const EditorMenuToolbarRuntimeInput& input);

    bool CloseEditorMenuToolbarRuntime(EditorMenuToolbarRuntimeState& runtime);
    bool ValidateEditorMenuToolbarSurface(const EditorMenuToolbarSurface& surface, i32 viewportWidth, i32 viewportHeight);

    std::string FormatEditorMenuToolbarHitTestResult(const EditorMenuToolbarHitTestResult& hit);
    std::string FormatEditorMenuToolbarSurface(const EditorMenuToolbarSurface& surface);
    std::string FormatEditorMenuToolbarRuntimeResult(const EditorMenuToolbarRuntimeResult& result);

    EditorMenuToolbarRuntimeDiagnostics RunEditorMenuToolbarRuntimeDiagnostics();
    std::string BuildEditorMenuToolbarRuntimeProbeSummary();
}
