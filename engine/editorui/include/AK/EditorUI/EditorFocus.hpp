#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/EditorUI/EditorRuntimeBridge.hpp>
#include <AK/EditorUI/EditorUXRuntime.hpp>

#include <string>

namespace AK
{
    enum class EditorFocusScope
    {
        None,
        MenuBar,
        Toolbar,
        DockTab,
        Hierarchy,
        SceneView,
        GameView,
        Inspector,
        ProjectBrowser,
        Console,
        Diagnostics,
        Popup,
        ModalDialog,
        TextField,
        ViewportOverlay
    };

    enum class EditorCaptureKind
    {
        None,
        Mouse,
        Keyboard,
        Text,
        Scroll,
        Drag
    };

    enum class EditorInputRoute
    {
        None,
        GlobalShortcut,
        TextEdit,
        Popup,
        Viewport,
        Panel,
        Blocked
    };

    enum class EditorFocusChangeReason
    {
        None,
        MousePress,
        KeyboardNavigation,
        BeginTextEdit,
        EndTextEdit,
        PopupOpened,
        PopupClosed,
        Command,
        Programmatic
    };

    struct EditorFocusOwner
    {
        EditorFocusScope scope = EditorFocusScope::None;
        EditorPanelId panel{};
        std::string widgetId;
        bool acceptsText = false;
        bool acceptsHotkeys = true;
        bool acceptsViewportInput = false;
        u64 revision = 1;
    };

    struct EditorCaptureOwner
    {
        EditorCaptureKind kind = EditorCaptureKind::None;
        EditorFocusScope scope = EditorFocusScope::None;
        EditorPanelId panel{};
        std::string widgetId;
        bool active = false;
        u64 revision = 1;
    };

    struct EditorTextEditCapture
    {
        bool active = false;
        std::string fieldId;
        std::string originalText;
        std::string workingText;
        bool commitOnEnter = true;
        bool cancelOnEscape = true;
        bool dirty = false;
        u64 revision = 1;
    };

    struct EditorFocusState
    {
        EditorFocusOwner focused{};
        EditorFocusOwner hovered{};
        EditorCaptureOwner mouseCapture{};
        EditorCaptureOwner keyboardCapture{};
        EditorCaptureOwner scrollCapture{};
        EditorCaptureOwner dragCapture{};
        EditorTextEditCapture textEdit{};
        bool popupModalCapture = false;
        u64 revision = 1;
    };

    struct EditorInputRoutingInput
    {
        EditorHitTestResult hit{};
        bool popupOpen = false;
        bool textEntryActive = false;
        bool mouseLeftPressed = false;
        bool mouseRightPressed = false;
        bool mouseLeftDown = false;
        bool mouseRightDown = false;
        bool mouseMiddleDown = false;
        bool mouseWheel = false;
        bool cancelPressed = false;
        bool confirmPressed = false;
        bool ctrlDown = false;
        bool shiftDown = false;
    };

    struct EditorInputRoutingDecision
    {
        EditorInputRoute route = EditorInputRoute::None;
        EditorFocusScope scope = EditorFocusScope::None;
        EditorPanelId panel{};
        bool consumed = false;
        bool allowGlobalShortcuts = true;
        bool allowTextInput = false;
        bool allowViewportInput = false;
        bool allowPanelScroll = true;
        bool closePopup = false;
        bool commitText = false;
        bool cancelText = false;
        std::string reason;
    };

    struct EditorFocusDiagnostics
    {
        bool hasFocus = false;
        bool mouseCaptured = false;
        bool keyboardCaptured = false;
        bool textCaptured = false;
        bool scrollCaptured = false;
        bool dragCaptured = false;
        bool popupModal = false;
        bool invalidCapture = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorFocusScope scope);
    const char* ToString(EditorCaptureKind kind);
    const char* ToString(EditorInputRoute route);
    const char* ToString(EditorFocusChangeReason reason);

    EditorFocusState MakeDefaultEditorFocusState();
    EditorFocusOwner MakeEditorFocusOwner(EditorFocusScope scope, EditorPanelId panel = {}, std::string widgetId = {});
    EditorFocusScope FocusScopeFromHitRegion(EditorHitRegion region, EditorPanelId panel);
    EditorInputRoute DefaultRouteForScope(EditorFocusScope scope);

    bool SetEditorHoverOwner(EditorFocusState& state, EditorFocusOwner owner);
    bool SetEditorFocusOwner(EditorFocusState& state, EditorFocusOwner owner, EditorFocusChangeReason reason = EditorFocusChangeReason::Programmatic);
    bool ClearEditorFocusOwner(EditorFocusState& state, EditorFocusChangeReason reason = EditorFocusChangeReason::Programmatic);

    bool CaptureEditorMouse(EditorFocusState& state, EditorFocusOwner owner, EditorCaptureKind kind = EditorCaptureKind::Mouse);
    bool ReleaseEditorMouseCapture(EditorFocusState& state);
    bool CaptureEditorKeyboard(EditorFocusState& state, EditorFocusOwner owner);
    bool ReleaseEditorKeyboardCapture(EditorFocusState& state);
    bool CaptureEditorScroll(EditorFocusState& state, EditorFocusOwner owner);
    bool ReleaseEditorScrollCapture(EditorFocusState& state);
    bool BeginEditorDragCapture(EditorFocusState& state, EditorFocusOwner owner, std::string widgetId = {});
    bool EndEditorDragCapture(EditorFocusState& state);

    bool BeginEditorTextEdit(EditorFocusState& state, std::string fieldId, std::string initialText, EditorFocusOwner owner = {});
    bool AppendEditorTextInput(EditorFocusState& state, std::string_view utf8Text, std::size_t maxBytes = 256);
    bool BackspaceEditorTextInput(EditorFocusState& state);
    bool CommitEditorTextEdit(EditorFocusState& state);
    bool CancelEditorTextEdit(EditorFocusState& state);

    bool EditorHasTextCapture(const EditorFocusState& state);
    bool EditorHasMouseCapture(const EditorFocusState& state);
    bool EditorHasKeyboardCapture(const EditorFocusState& state);
    bool EditorAllowsGlobalShortcuts(const EditorFocusState& state);
    bool EditorAllowsViewportInput(const EditorFocusState& state);

    EditorInputRoutingDecision RouteEditorInputFocus(EditorFocusState& state, const EditorInputRoutingInput& input);
    EditorFocusDiagnostics ValidateEditorFocusState(const EditorFocusState& state);
    std::string FormatEditorFocusOwner(const EditorFocusOwner& owner);
    std::string FormatEditorInputRoutingDecision(const EditorInputRoutingDecision& decision);
    std::string FormatEditorFocusDiagnostics(const EditorFocusDiagnostics& diagnostics);
}
