#pragma once

#include <AK/EditorUI/EditorCommandPalette.hpp>

#include <string>

namespace AK
{
    enum class EditorCommandPaletteHitKind
    {
        None,
        Outside,
        Surface,
        SearchBox,
        ResultRow,
        Footer
    };

    struct EditorCommandPaletteHitTestResult
    {
        EditorCommandPaletteHitKind kind = EditorCommandPaletteHitKind::None;
        usize rowIndex = 0;
        bool actionable = false;
        bool enabled = false;
        std::string stableId;
        std::string label;
    };

    struct EditorCommandPaletteRuntimeState
    {
        EditorCommandPaletteState palette{};
        EditorCommandPaletteSurface surface{};
        EditorCommandPaletteHitTestResult hover{};
        EditorCommandPaletteResult lastResult{};
        bool keyboardCapture = false;
        bool mouseCapture = false;
        u64 revision = 1;
    };

    struct EditorCommandPaletteRuntimeInput
    {
        const EditorSearchIndex* searchIndex = nullptr;
        const EditorCommandStateCache* commandState = nullptr;
        const EditorShortcutProfile* shortcuts = nullptr;
        i32 viewportWidth = 1366;
        i32 viewportHeight = 768;
        i32 mouseX = 0;
        i32 mouseY = 0;
        i32 mouseWheelDelta = 0;
        bool mouseLeftPressed = false;
        bool keyEscapePressed = false;
        bool keyEnterPressed = false;
        bool keyBackspacePressed = false;
        bool keyUpPressed = false;
        bool keyDownPressed = false;
        bool openShortcutPressed = false;
        std::string textInput;
    };

    struct EditorCommandPaletteRuntimeResult
    {
        bool handled = false;
        bool opened = false;
        bool closed = false;
        bool surfaceChanged = false;
        bool hoverChanged = false;
        bool accepted = false;
        bool commandQueued = false;
        CommandInvocation command{};
        EditorSearchResultKind acceptedKind = EditorSearchResultKind::Command;
        EntityId acceptedEntity{};
        EditorPanelId acceptedPanel{};
        std::string acceptedStableId;
        std::string statusText;
    };

    struct EditorCommandPaletteRuntimeDiagnostics
    {
        EditorCommandPaletteRuntimeState runtime{};
        EditorCommandPaletteRuntimeResult openResult{};
        EditorCommandPaletteRuntimeResult textResult{};
        EditorCommandPaletteRuntimeResult moveResult{};
        EditorCommandPaletteRuntimeResult acceptResult{};
        EditorCommandPaletteHitTestResult rowHit{};
        usize rowCount = 0;
        bool openOk = false;
        bool textOk = false;
        bool hitOk = false;
        bool acceptOk = false;
        bool captureOk = false;
        bool validSurface = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorCommandPaletteHitKind kind);

    EditorCommandPaletteRuntimeState MakeDefaultEditorCommandPaletteRuntimeState();

    EditorCommandPaletteHitTestResult HitTestEditorCommandPaletteSurface(const EditorCommandPaletteSurface& surface, i32 x, i32 y);

    void RefreshEditorCommandPaletteRuntimeSurface(EditorCommandPaletteRuntimeState& runtime,
                                                   const EditorSearchIndex& searchIndex,
                                                   const EditorCommandStateCache& commandState,
                                                   const EditorShortcutProfile& shortcuts,
                                                   i32 viewportWidth,
                                                   i32 viewportHeight);

    EditorCommandPaletteRuntimeResult ApplyEditorCommandPaletteRuntimeInput(EditorCommandPaletteRuntimeState& runtime,
                                                                            const EditorCommandPaletteRuntimeInput& input);

    bool ValidateEditorCommandPaletteRuntimeState(const EditorCommandPaletteRuntimeState& runtime, i32 viewportWidth, i32 viewportHeight);

    std::string FormatEditorCommandPaletteHitTestResult(const EditorCommandPaletteHitTestResult& hit);
    std::string FormatEditorCommandPaletteRuntimeState(const EditorCommandPaletteRuntimeState& runtime);
    std::string FormatEditorCommandPaletteRuntimeResult(const EditorCommandPaletteRuntimeResult& result);

    EditorCommandPaletteRuntimeDiagnostics RunEditorCommandPaletteRuntimeDiagnostics();
    std::string BuildEditorCommandPaletteRuntimeProbeSummary();
}
