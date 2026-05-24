#include <AK/EditorUI/EditorFocus.hpp>
#include <AK/EditorUI/EditorText.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        bool SameFocusOwner(const EditorFocusOwner& a, const EditorFocusOwner& b)
        {
            return a.scope == b.scope && a.panel == b.panel && a.widgetId == b.widgetId && a.acceptsText == b.acceptsText && a.acceptsHotkeys == b.acceptsHotkeys && a.acceptsViewportInput == b.acceptsViewportInput;
        }

        bool SameCaptureOwner(const EditorCaptureOwner& a, const EditorCaptureOwner& b)
        {
            return a.kind == b.kind && a.scope == b.scope && a.panel == b.panel && a.widgetId == b.widgetId && a.active == b.active;
        }

        EditorCaptureOwner MakeCapture(EditorFocusOwner owner, EditorCaptureKind kind)
        {
            EditorCaptureOwner capture{};
            capture.kind = kind;
            capture.scope = owner.scope;
            capture.panel = owner.panel;
            capture.widgetId = owner.widgetId;
            capture.active = kind != EditorCaptureKind::None && owner.scope != EditorFocusScope::None;
            return capture;
        }

        void Bump(EditorFocusState& state)
        {
            ++state.revision;
        }

        bool IsPrintableEditorCodepoint(u32 cp)
        {
            return cp >= 0x20u && cp != 0x7Fu;
        }
    }

    const char* ToString(EditorFocusScope scope)
    {
        switch (scope)
        {
            case EditorFocusScope::None: return "None";
            case EditorFocusScope::MenuBar: return "MenuBar";
            case EditorFocusScope::Toolbar: return "Toolbar";
            case EditorFocusScope::DockTab: return "DockTab";
            case EditorFocusScope::Hierarchy: return "Hierarchy";
            case EditorFocusScope::SceneView: return "SceneView";
            case EditorFocusScope::GameView: return "GameView";
            case EditorFocusScope::Inspector: return "Inspector";
            case EditorFocusScope::ProjectBrowser: return "ProjectBrowser";
            case EditorFocusScope::Console: return "Console";
            case EditorFocusScope::Diagnostics: return "Diagnostics";
            case EditorFocusScope::Popup: return "Popup";
            case EditorFocusScope::ModalDialog: return "ModalDialog";
            case EditorFocusScope::TextField: return "TextField";
            case EditorFocusScope::ViewportOverlay: return "ViewportOverlay";
        }
        return "None";
    }

    const char* ToString(EditorCaptureKind kind)
    {
        switch (kind)
        {
            case EditorCaptureKind::None: return "None";
            case EditorCaptureKind::Mouse: return "Mouse";
            case EditorCaptureKind::Keyboard: return "Keyboard";
            case EditorCaptureKind::Text: return "Text";
            case EditorCaptureKind::Scroll: return "Scroll";
            case EditorCaptureKind::Drag: return "Drag";
        }
        return "None";
    }

    const char* ToString(EditorInputRoute route)
    {
        switch (route)
        {
            case EditorInputRoute::None: return "None";
            case EditorInputRoute::GlobalShortcut: return "GlobalShortcut";
            case EditorInputRoute::TextEdit: return "TextEdit";
            case EditorInputRoute::Popup: return "Popup";
            case EditorInputRoute::Viewport: return "Viewport";
            case EditorInputRoute::Panel: return "Panel";
            case EditorInputRoute::Blocked: return "Blocked";
        }
        return "None";
    }

    const char* ToString(EditorFocusChangeReason reason)
    {
        switch (reason)
        {
            case EditorFocusChangeReason::None: return "None";
            case EditorFocusChangeReason::MousePress: return "MousePress";
            case EditorFocusChangeReason::KeyboardNavigation: return "KeyboardNavigation";
            case EditorFocusChangeReason::BeginTextEdit: return "BeginTextEdit";
            case EditorFocusChangeReason::EndTextEdit: return "EndTextEdit";
            case EditorFocusChangeReason::PopupOpened: return "PopupOpened";
            case EditorFocusChangeReason::PopupClosed: return "PopupClosed";
            case EditorFocusChangeReason::Command: return "Command";
            case EditorFocusChangeReason::Programmatic: return "Programmatic";
        }
        return "None";
    }

    EditorFocusState MakeDefaultEditorFocusState()
    {
        EditorFocusState state{};
        state.focused = MakeEditorFocusOwner(EditorFocusScope::SceneView);
        state.hovered = {};
        state.revision = 1;
        return state;
    }

    EditorFocusOwner MakeEditorFocusOwner(EditorFocusScope scope, EditorPanelId panel, std::string widgetId)
    {
        EditorFocusOwner owner{};
        owner.scope = scope;
        owner.panel = panel;
        owner.widgetId = std::move(widgetId);
        owner.acceptsText = scope == EditorFocusScope::TextField;
        owner.acceptsHotkeys = scope != EditorFocusScope::TextField && scope != EditorFocusScope::Popup && scope != EditorFocusScope::ModalDialog;
        owner.acceptsViewportInput = scope == EditorFocusScope::SceneView || scope == EditorFocusScope::GameView;
        return owner;
    }

    EditorFocusScope FocusScopeFromHitRegion(EditorHitRegion region, EditorPanelId panel)
    {
        switch (region)
        {
            case EditorHitRegion::MenuBar: return EditorFocusScope::MenuBar;
            case EditorHitRegion::Toolbar: return EditorFocusScope::Toolbar;
            case EditorHitRegion::StatusBar: return EditorFocusScope::None;
            case EditorHitRegion::DockTab: return EditorFocusScope::DockTab;
            case EditorHitRegion::DockSplitter: return EditorFocusScope::None;
            case EditorHitRegion::Overlay: return EditorFocusScope::ViewportOverlay;
            case EditorHitRegion::DockPanelBody:
            {
                if (panel.value == 1u)
                {
                    return EditorFocusScope::Hierarchy;
                }
                if (panel.value == 3u)
                {
                    return EditorFocusScope::SceneView;
                }
                if (panel.value == 4u)
                {
                    return EditorFocusScope::GameView;
                }
                if (panel.value == 5u)
                {
                    return EditorFocusScope::Inspector;
                }
                if (panel.value == 8u)
                {
                    return EditorFocusScope::ProjectBrowser;
                }
                if (panel.value == 9u)
                {
                    return EditorFocusScope::Console;
                }
                if (panel.value == 7u)
                {
                    return EditorFocusScope::Diagnostics;
                }
                return EditorFocusScope::None;
            }
            case EditorHitRegion::EmptyDockSpace:
            case EditorHitRegion::None:
                return EditorFocusScope::None;
        }
        return EditorFocusScope::None;
    }

    EditorInputRoute DefaultRouteForScope(EditorFocusScope scope)
    {
        switch (scope)
        {
            case EditorFocusScope::TextField: return EditorInputRoute::TextEdit;
            case EditorFocusScope::Popup:
            case EditorFocusScope::ModalDialog: return EditorInputRoute::Popup;
            case EditorFocusScope::SceneView:
            case EditorFocusScope::GameView:
            case EditorFocusScope::ViewportOverlay: return EditorInputRoute::Viewport;
            case EditorFocusScope::Hierarchy:
            case EditorFocusScope::Inspector:
            case EditorFocusScope::ProjectBrowser:
            case EditorFocusScope::Console:
            case EditorFocusScope::Diagnostics:
            case EditorFocusScope::DockTab:
            case EditorFocusScope::Toolbar:
            case EditorFocusScope::MenuBar: return EditorInputRoute::Panel;
            case EditorFocusScope::None: return EditorInputRoute::GlobalShortcut;
        }
        return EditorInputRoute::None;
    }

    bool SetEditorHoverOwner(EditorFocusState& state, EditorFocusOwner owner)
    {
        if (SameFocusOwner(state.hovered, owner))
        {
            return false;
        }
        state.hovered = std::move(owner);
        ++state.hovered.revision;
        Bump(state);
        return true;
    }

    bool SetEditorFocusOwner(EditorFocusState& state, EditorFocusOwner owner, EditorFocusChangeReason)
    {
        if (SameFocusOwner(state.focused, owner))
        {
            return false;
        }
        state.focused = std::move(owner);
        ++state.focused.revision;
        Bump(state);
        return true;
    }

    bool ClearEditorFocusOwner(EditorFocusState& state, EditorFocusChangeReason reason)
    {
        return SetEditorFocusOwner(state, {}, reason);
    }

    bool CaptureEditorMouse(EditorFocusState& state, EditorFocusOwner owner, EditorCaptureKind kind)
    {
        EditorCaptureOwner next = MakeCapture(std::move(owner), kind == EditorCaptureKind::None ? EditorCaptureKind::Mouse : kind);
        if (SameCaptureOwner(state.mouseCapture, next))
        {
            return false;
        }
        state.mouseCapture = std::move(next);
        ++state.mouseCapture.revision;
        Bump(state);
        return true;
    }

    bool ReleaseEditorMouseCapture(EditorFocusState& state)
    {
        if (!state.mouseCapture.active)
        {
            return false;
        }
        state.mouseCapture = {};
        Bump(state);
        return true;
    }

    bool CaptureEditorKeyboard(EditorFocusState& state, EditorFocusOwner owner)
    {
        EditorCaptureOwner next = MakeCapture(std::move(owner), EditorCaptureKind::Keyboard);
        if (SameCaptureOwner(state.keyboardCapture, next))
        {
            return false;
        }
        state.keyboardCapture = std::move(next);
        ++state.keyboardCapture.revision;
        Bump(state);
        return true;
    }

    bool ReleaseEditorKeyboardCapture(EditorFocusState& state)
    {
        if (!state.keyboardCapture.active)
        {
            return false;
        }
        state.keyboardCapture = {};
        Bump(state);
        return true;
    }

    bool CaptureEditorScroll(EditorFocusState& state, EditorFocusOwner owner)
    {
        EditorCaptureOwner next = MakeCapture(std::move(owner), EditorCaptureKind::Scroll);
        if (SameCaptureOwner(state.scrollCapture, next))
        {
            return false;
        }
        state.scrollCapture = std::move(next);
        ++state.scrollCapture.revision;
        Bump(state);
        return true;
    }

    bool ReleaseEditorScrollCapture(EditorFocusState& state)
    {
        if (!state.scrollCapture.active)
        {
            return false;
        }
        state.scrollCapture = {};
        Bump(state);
        return true;
    }

    bool BeginEditorDragCapture(EditorFocusState& state, EditorFocusOwner owner, std::string widgetId)
    {
        if (!widgetId.empty())
        {
            owner.widgetId = std::move(widgetId);
        }
        EditorCaptureOwner next = MakeCapture(std::move(owner), EditorCaptureKind::Drag);
        if (SameCaptureOwner(state.dragCapture, next))
        {
            return false;
        }
        state.dragCapture = std::move(next);
        ++state.dragCapture.revision;
        Bump(state);
        return true;
    }

    bool EndEditorDragCapture(EditorFocusState& state)
    {
        if (!state.dragCapture.active)
        {
            return false;
        }
        state.dragCapture = {};
        Bump(state);
        return true;
    }

    bool BeginEditorTextEdit(EditorFocusState& state, std::string fieldId, std::string initialText, EditorFocusOwner owner)
    {
        if (owner.scope == EditorFocusScope::None)
        {
            owner = MakeEditorFocusOwner(EditorFocusScope::TextField, {}, fieldId);
        }
        owner.scope = EditorFocusScope::TextField;
        owner.acceptsText = true;
        owner.acceptsHotkeys = false;
        owner.acceptsViewportInput = false;
        SetEditorFocusOwner(state, owner, EditorFocusChangeReason::BeginTextEdit);
        CaptureEditorKeyboard(state, owner);

        state.textEdit.active = true;
        state.textEdit.fieldId = std::move(fieldId);
        state.textEdit.originalText = std::move(initialText);
        state.textEdit.workingText = state.textEdit.originalText;
        state.textEdit.dirty = false;
        ++state.textEdit.revision;
        Bump(state);
        return true;
    }

    bool AppendEditorTextInput(EditorFocusState& state, std::string_view utf8Text, std::size_t maxBytes)
    {
        if (!state.textEdit.active || utf8Text.empty())
        {
            return false;
        }
        bool valid = false;
        const std::vector<u32> codepoints = DecodeEditorUtf8(utf8Text, &valid);
        std::string append;
        for (const u32 cp : codepoints)
        {
            if (IsPrintableEditorCodepoint(cp))
            {
                append += EncodeEditorUtf8({cp});
            }
        }
        if (append.empty())
        {
            return false;
        }
        if (state.textEdit.workingText.size() + append.size() > maxBytes)
        {
            append.resize(maxBytes > state.textEdit.workingText.size() ? maxBytes - state.textEdit.workingText.size() : 0);
            while (!append.empty() && (static_cast<unsigned char>(append.back()) & 0xC0u) == 0x80u)
            {
                append.pop_back();
            }
        }
        if (append.empty())
        {
            return false;
        }
        state.textEdit.workingText += append;
        state.textEdit.dirty = state.textEdit.workingText != state.textEdit.originalText;
        ++state.textEdit.revision;
        Bump(state);
        return valid;
    }

    bool BackspaceEditorTextInput(EditorFocusState& state)
    {
        if (!state.textEdit.active || state.textEdit.workingText.empty())
        {
            return false;
        }
        std::string& text = state.textEdit.workingText;
        do
        {
            text.pop_back();
        } while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0u) == 0x80u);
        state.textEdit.dirty = text != state.textEdit.originalText;
        ++state.textEdit.revision;
        Bump(state);
        return true;
    }

    bool CommitEditorTextEdit(EditorFocusState& state)
    {
        if (!state.textEdit.active)
        {
            return false;
        }
        state.textEdit.active = false;
        state.textEdit.dirty = false;
        ++state.textEdit.revision;
        ReleaseEditorKeyboardCapture(state);
        ClearEditorFocusOwner(state, EditorFocusChangeReason::EndTextEdit);
        Bump(state);
        return true;
    }

    bool CancelEditorTextEdit(EditorFocusState& state)
    {
        if (!state.textEdit.active)
        {
            return false;
        }
        state.textEdit.workingText = state.textEdit.originalText;
        state.textEdit.active = false;
        state.textEdit.dirty = false;
        ++state.textEdit.revision;
        ReleaseEditorKeyboardCapture(state);
        ClearEditorFocusOwner(state, EditorFocusChangeReason::EndTextEdit);
        Bump(state);
        return true;
    }

    bool EditorHasTextCapture(const EditorFocusState& state)
    {
        return state.textEdit.active;
    }

    bool EditorHasMouseCapture(const EditorFocusState& state)
    {
        return state.mouseCapture.active || state.dragCapture.active;
    }

    bool EditorHasKeyboardCapture(const EditorFocusState& state)
    {
        return state.keyboardCapture.active || state.textEdit.active;
    }

    bool EditorAllowsGlobalShortcuts(const EditorFocusState& state)
    {
        return !state.popupModalCapture && !state.textEdit.active && (!state.keyboardCapture.active || state.keyboardCapture.scope != EditorFocusScope::TextField);
    }

    bool EditorAllowsViewportInput(const EditorFocusState& state)
    {
        return !state.popupModalCapture && !state.textEdit.active && state.focused.acceptsViewportInput;
    }

    EditorInputRoutingDecision RouteEditorInputFocus(EditorFocusState& state, const EditorInputRoutingInput& input)
    {
        EditorInputRoutingDecision decision{};

        if (input.popupOpen)
        {
            state.popupModalCapture = true;
            decision.route = EditorInputRoute::Popup;
            decision.scope = EditorFocusScope::Popup;
            decision.consumed = input.mouseLeftPressed || input.mouseRightPressed || input.cancelPressed;
            decision.allowGlobalShortcuts = false;
            decision.allowTextInput = false;
            decision.allowViewportInput = false;
            decision.allowPanelScroll = false;
            decision.closePopup = input.cancelPressed;
            decision.reason = input.cancelPressed ? "popup escape" : "popup modal capture";
            SetEditorFocusOwner(state, MakeEditorFocusOwner(EditorFocusScope::Popup), EditorFocusChangeReason::PopupOpened);
            return decision;
        }

        if (state.popupModalCapture)
        {
            state.popupModalCapture = false;
            ClearEditorFocusOwner(state, EditorFocusChangeReason::PopupClosed);
        }

        if (state.textEdit.active || input.textEntryActive)
        {
            decision.route = EditorInputRoute::TextEdit;
            decision.scope = EditorFocusScope::TextField;
            decision.consumed = true;
            decision.allowGlobalShortcuts = false;
            decision.allowTextInput = true;
            decision.allowViewportInput = false;
            decision.allowPanelScroll = false;
            decision.commitText = input.confirmPressed;
            decision.cancelText = input.cancelPressed;
            decision.reason = "text edit capture";
            return decision;
        }

        EditorFocusScope scope = FocusScopeFromHitRegion(input.hit.region, input.hit.panel);
        if (input.hit.region == EditorHitRegion::None && state.focused.scope != EditorFocusScope::None)
        {
            scope = state.focused.scope;
        }

        EditorFocusOwner hover = MakeEditorFocusOwner(scope, input.hit.panel, input.hit.label);
        SetEditorHoverOwner(state, hover);
        if (input.mouseLeftPressed || input.mouseRightPressed)
        {
            SetEditorFocusOwner(state, hover, EditorFocusChangeReason::MousePress);
            if (input.mouseLeftDown || input.mouseRightDown)
            {
                CaptureEditorMouse(state, hover, EditorCaptureKind::Mouse);
            }
        }
        else if (!input.mouseLeftDown && !input.mouseRightDown && !input.mouseMiddleDown)
        {
            ReleaseEditorMouseCapture(state);
        }

        if (input.mouseWheel)
        {
            CaptureEditorScroll(state, hover);
        }
        else if (!input.mouseWheel)
        {
            ReleaseEditorScrollCapture(state);
        }

        decision.scope = scope;
        decision.panel = input.hit.panel;
        decision.route = DefaultRouteForScope(scope);
        decision.allowGlobalShortcuts = state.focused.acceptsHotkeys && !EditorHasKeyboardCapture(state);
        decision.allowTextInput = false;
        decision.allowViewportInput = scope == EditorFocusScope::SceneView || scope == EditorFocusScope::GameView || scope == EditorFocusScope::ViewportOverlay;
        decision.allowPanelScroll = scope != EditorFocusScope::None;
        decision.consumed = decision.route == EditorInputRoute::Panel && (input.mouseLeftPressed || input.mouseRightPressed);
        decision.reason = FormatEditorFocusOwner(hover);
        return decision;
    }

    EditorFocusDiagnostics ValidateEditorFocusState(const EditorFocusState& state)
    {
        EditorFocusDiagnostics diagnostics{};
        diagnostics.hasFocus = state.focused.scope != EditorFocusScope::None;
        diagnostics.mouseCaptured = state.mouseCapture.active;
        diagnostics.keyboardCaptured = state.keyboardCapture.active;
        diagnostics.textCaptured = state.textEdit.active;
        diagnostics.scrollCaptured = state.scrollCapture.active;
        diagnostics.dragCaptured = state.dragCapture.active;
        diagnostics.popupModal = state.popupModalCapture;
        diagnostics.invalidCapture = state.textEdit.active && state.focused.scope != EditorFocusScope::TextField && state.focused.scope != EditorFocusScope::Popup;
        diagnostics.ok = !diagnostics.invalidCapture;
        diagnostics.summary = FormatEditorFocusDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorFocusOwner(const EditorFocusOwner& owner)
    {
        std::ostringstream stream;
        stream << ToString(owner.scope) << " panel=" << owner.panel.value;
        if (!owner.widgetId.empty())
        {
            stream << " widget=" << owner.widgetId;
        }
        stream << " text=" << (owner.acceptsText ? "yes" : "no") << " hotkeys=" << (owner.acceptsHotkeys ? "yes" : "no") << " viewport=" << (owner.acceptsViewportInput ? "yes" : "no");
        return stream.str();
    }

    std::string FormatEditorInputRoutingDecision(const EditorInputRoutingDecision& decision)
    {
        std::ostringstream stream;
        stream << "route=" << ToString(decision.route)
               << " scope=" << ToString(decision.scope)
               << " panel=" << decision.panel.value
               << " consumed=" << (decision.consumed ? "yes" : "no")
               << " shortcuts=" << (decision.allowGlobalShortcuts ? "yes" : "no")
               << " text=" << (decision.allowTextInput ? "yes" : "no")
               << " viewport=" << (decision.allowViewportInput ? "yes" : "no")
               << " scroll=" << (decision.allowPanelScroll ? "yes" : "no");
        if (!decision.reason.empty())
        {
            stream << " reason=" << decision.reason;
        }
        return stream.str();
    }

    std::string FormatEditorFocusDiagnostics(const EditorFocusDiagnostics& diagnostics)
    {
        std::ostringstream stream;
        stream << "editor-focus focus=" << (diagnostics.hasFocus ? "yes" : "no")
               << " mouse=" << (diagnostics.mouseCaptured ? "yes" : "no")
               << " keyboard=" << (diagnostics.keyboardCaptured ? "yes" : "no")
               << " text=" << (diagnostics.textCaptured ? "yes" : "no")
               << " scroll=" << (diagnostics.scrollCaptured ? "yes" : "no")
               << " drag=" << (diagnostics.dragCaptured ? "yes" : "no")
               << " popup=" << (diagnostics.popupModal ? "yes" : "no")
               << " invalid=" << diagnostics.invalidCapture
               << " ok=" << (diagnostics.ok ? "true" : "false");
        return stream.str();
    }
}
