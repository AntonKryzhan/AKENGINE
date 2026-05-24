#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>
#include <AK/EditorUI/EditorFocus.hpp>
#include <AK/EditorUI/EditorWidgets.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorCommandDisableReason
    {
        None,
        RequiresSelection,
        UndoUnavailable,
        RedoUnavailable,
        TextEditing,
        PopupOpen,
        ModalOpen,
        PlayModeBlocked,
        ReadOnlyScene,
        InvalidContext
    };

    struct EditorCommandContext
    {
        bool hasSelection = false;
        bool canUndo = false;
        bool canRedo = false;
        bool sceneDirty = false;
        bool readOnlyScene = false;
        bool playMode = false;
        bool gridSnapEnabled = false;
        bool textEditing = false;
        bool popupOpen = false;
        bool modalOpen = false;
        bool viewportFocused = false;
        bool projectFocused = false;
        bool hierarchyFocused = false;
        u64 selectionRevision = 1;
        u64 undoRevision = 1;
        u64 sceneRevision = 1;
        u64 focusRevision = 1;
        u64 uiRevision = 1;
        CommandId activeTransformToolCommand = CommandId::ToolTranslate;
        CommandId activeTransformSpaceCommand = CommandId::ToolSpaceWorld;
        CommandId activeSceneViewModeCommand = CommandId::ViewScene2D;
    };

    struct EditorCommandStateEntry
    {
        CommandId command = CommandId::Count;
        std::string name;
        std::string label;
        std::string tooltip;
        std::string disabledReasonText;
        bool visible = true;
        bool enabled = true;
        bool checked = false;
        bool destructive = false;
        bool requiresSelection = false;
        EditorCommandDisableReason disabledReason = EditorCommandDisableReason::None;
        u64 revision = 1;
    };

    struct EditorCommandStateCache
    {
        std::vector<EditorCommandStateEntry> entries;
        u64 contextHash = 0;
        u64 revision = 1;
    };

    enum class EditorTooltipPhase
    {
        Hidden,
        Waiting,
        Visible
    };

    struct EditorTooltipState
    {
        EditorTooltipPhase phase = EditorTooltipPhase::Hidden;
        std::string widgetId;
        std::string text;
        EditorRect anchor{};
        EditorRect surface{};
        double hoverSeconds = 0.0;
        double delaySeconds = 0.45;
        bool pinned = false;
        u64 revision = 1;
    };

    enum class EditorModalKind
    {
        None,
        ConfirmDelete,
        SaveChanges,
        BuildProgress,
        ProjectSettings,
        Error
    };

    enum class EditorModalButtonRole
    {
        None,
        Accept,
        Cancel,
        Destructive,
        Alternate
    };

    struct EditorModalButton
    {
        std::string id;
        std::string label;
        EditorModalButtonRole role = EditorModalButtonRole::None;
        CommandId command = CommandId::Count;
        bool enabled = true;
        bool defaultButton = false;
    };

    struct EditorModalDialog
    {
        std::string id;
        EditorModalKind kind = EditorModalKind::None;
        std::string title;
        std::string message;
        std::vector<EditorModalButton> buttons;
        bool blocksGlobalShortcuts = true;
        bool closeOnEscape = true;
        u64 revision = 1;
    };

    struct EditorModalStack
    {
        std::vector<EditorModalDialog> dialogs;
        std::string lastActivatedButtonId;
        CommandInvocation lastInvocation{};
        u64 revision = 1;
    };

    struct EditorCommandStateDiagnostics
    {
        std::size_t commandCount = 0;
        std::size_t enabledCount = 0;
        std::size_t disabledCount = 0;
        std::size_t checkedCount = 0;
        std::size_t missingCommandCount = 0;
        std::size_t invalidTooltipCount = 0;
        std::size_t modalCount = 0;
        bool selectionCommandsDisabled = false;
        bool undoRedoStateOk = false;
        bool textCaptureBlocksUnsafeCommands = false;
        bool tooltipTimingOk = false;
        bool modalCaptureOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorCommandDisableReason reason);
    const char* ToString(EditorTooltipPhase phase);
    const char* ToString(EditorModalKind kind);
    const char* ToString(EditorModalButtonRole role);

    u64 HashEditorCommandContext(const EditorCommandContext& context);
    EditorCommandContext BuildEditorCommandContext(const EditorFocusState& focus, bool hasSelection, bool canUndo, bool canRedo, bool sceneDirty, bool gridSnapEnabled, bool viewportFocused, bool readOnlyScene = false, bool playMode = false, CommandId activeTransformToolCommand = CommandId::ToolTranslate, CommandId activeTransformSpaceCommand = CommandId::ToolSpaceWorld, CommandId activeSceneViewModeCommand = CommandId::ViewScene2D);

    EditorCommandStateCache BuildEditorCommandStateCache(const CommandRegistry& registry, const EditorCommandContext& context);
    const EditorCommandStateEntry* FindEditorCommandState(const EditorCommandStateCache& cache, CommandId command);
    bool IsEditorCommandEnabled(const EditorCommandStateCache& cache, CommandId command);
    bool IsEditorCommandChecked(const EditorCommandStateCache& cache, CommandId command);
    bool ApplyEditorCommandStateToToolbar(EditorToolbarModel& toolbar, const EditorCommandStateCache& cache);

    EditorTooltipState BeginEditorTooltipHover(EditorTooltipState state, std::string widgetId, std::string text, EditorRect anchor, double delaySeconds = 0.45);
    EditorTooltipState UpdateEditorTooltip(EditorTooltipState state, double deltaSeconds, i32 viewportWidth, i32 viewportHeight);
    EditorTooltipState EndEditorTooltipHover(EditorTooltipState state, std::string_view widgetId = {});

    EditorModalDialog BuildConfirmDeleteModal(std::string targetLabel);
    EditorModalDialog BuildSaveChangesModal(std::string sceneLabel);
    bool PushEditorModal(EditorModalStack& stack, EditorModalDialog dialog);
    bool CloseTopEditorModal(EditorModalStack& stack);
    const EditorModalDialog* TopEditorModal(const EditorModalStack& stack);
    CommandInvocation ActivateEditorModalButton(EditorModalStack& stack, std::string_view buttonId);
    bool EditorModalStackBlocksGlobalShortcuts(const EditorModalStack& stack);

    EditorCommandStateDiagnostics ValidateEditorCommandState(const EditorCommandStateCache& cache, const CommandRegistry& registry, const EditorTooltipState& tooltip, const EditorModalStack& modals);
    EditorCommandStateDiagnostics RunEditorCommandStateDiagnostics();

    std::string FormatEditorCommandContext(const EditorCommandContext& context);
    std::string FormatEditorCommandStateEntry(const EditorCommandStateEntry& entry);
    std::string FormatEditorCommandStateDiagnostics(const EditorCommandStateDiagnostics& diagnostics);
    std::string FormatEditorTooltipState(const EditorTooltipState& tooltip);
    std::string FormatEditorModalDialog(const EditorModalDialog& dialog);
}
