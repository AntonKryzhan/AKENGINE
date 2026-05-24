#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/EditorUI/EditorPanelModels.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorInteractionKind
    {
        None,
        PointerSelect,
        CommandShortcut,
        CommandPaletteOpen,
        CommandPaletteQuery,
        CommandPaletteMove,
        CommandPaletteAccept,
        HierarchySelect,
        AssetSelect,
        PropertyBeginEdit,
        PropertyCommitEdit,
        PropertyCancelEdit,
        PanelFocus,
        OverlayToggle
    };

    enum class EditorPropertyChangeStatus
    {
        Pending,
        Accepted,
        Rejected
    };

    enum class EditorSelectionChangeReason
    {
        UserClick,
        KeyboardNavigation,
        SearchResult,
        Command,
        Programmatic
    };

    struct EditorInteractionPaletteState
    {
        bool open = false;
        std::string query;
        std::vector<EditorSearchResult> results;
        std::size_t selectedIndex = 0;
        u64 revision = 1;
    };

    struct EditorPropertyEditState
    {
        bool editing = false;
        std::string propertyPath;
        EditorPropertyValue originalValue{};
        EditorPropertyValue workingValue{};
        u64 revision = 1;
    };

    struct EditorPropertyChangeRequest
    {
        std::string propertyPath;
        EditorPropertyValue oldValue{};
        EditorPropertyValue newValue{};
        EditorPropertyChangeStatus status = EditorPropertyChangeStatus::Pending;
        bool requiresSceneDirty = false;
        bool requiresProxyRebuild = false;
        bool undoable = true;
        u64 revision = 1;
    };

    struct EditorSelectionChangeRequest
    {
        EditorSelectionSnapshot before{};
        EditorSelectionSnapshot after{};
        EditorSelectionChangeReason reason = EditorSelectionChangeReason::Programmatic;
        bool undoable = false;
        u64 revision = 1;
    };

    struct EditorInteractionEvent
    {
        EditorInteractionKind kind = EditorInteractionKind::None;
        CommandId command = CommandId::CloseEditor;
        CommandSource commandSource = CommandSource::Programmatic;
        std::string text;
        std::string stableId;
        std::string propertyPath;
        EditorPropertyValue propertyValue{};
        i32 direction = 0;
        i32 x = 0;
        i32 y = 0;
        u32 overlayId = 0;
    };

    struct EditorInteractionFrameResult
    {
        std::vector<CommandInvocation> commands;
        std::vector<EditorSelectionChangeRequest> selectionChanges;
        std::vector<EditorPropertyChangeRequest> propertyChanges;
        std::vector<std::string> messages;
        bool consumed = false;
        bool sceneDirty = false;
        bool layoutDirty = false;
        bool modelDirty = false;
    };

    struct EditorInteractionContext
    {
        EditorRuntimeBridge bridge{};
        EditorPanelModelFrame panels{};
        EditorInteractionPaletteState palette{};
        EditorPropertyEditState propertyEdit{};
        std::vector<EditorPropertyChangeRequest> pendingPropertyChanges;
        std::vector<EditorSelectionChangeRequest> pendingSelectionChanges;
        std::vector<CommandInvocation> pendingCommands;
        u64 revision = 1;
    };

    struct EditorInteractionDiagnostics
    {
        std::size_t pendingCommandCount = 0;
        std::size_t pendingSelectionChangeCount = 0;
        std::size_t pendingPropertyChangeCount = 0;
        std::size_t paletteResultCount = 0;
        std::size_t rejectedPropertyChangeCount = 0;
        bool paletteOpen = false;
        bool propertyEditing = false;
        bool selectionValid = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorInteractionKind kind);
    const char* ToString(EditorPropertyChangeStatus status);
    const char* ToString(EditorSelectionChangeReason reason);

    EditorInteractionContext BuildDefaultEditorInteractionContext(i32 width = 1600, i32 height = 900);
    EditorInteractionFrameResult ProcessEditorInteraction(EditorInteractionContext& context, const EditorInteractionEvent& event);

    bool OpenEditorCommandPalette(EditorInteractionContext& context, std::string query = {});
    bool UpdateEditorCommandPalette(EditorInteractionContext& context, std::string query);
    bool MoveEditorCommandPaletteSelection(EditorInteractionContext& context, i32 delta);
    EditorInteractionFrameResult AcceptEditorCommandPaletteSelection(EditorInteractionContext& context);

    bool BeginEditorPropertyEdit(EditorInteractionContext& context, const std::string& propertyPath);
    EditorInteractionFrameResult CommitEditorPropertyEdit(EditorInteractionContext& context, const std::string& propertyPath, EditorPropertyValue value);
    bool CancelEditorPropertyEdit(EditorInteractionContext& context);

    EditorInteractionFrameResult SelectEditorHierarchyItem(EditorInteractionContext& context, const std::string& stableId, EditorSelectionChangeReason reason);
    EditorInteractionFrameResult SelectEditorAssetItem(EditorInteractionContext& context, const std::string& stableId, EditorSelectionChangeReason reason);
    EditorInteractionFrameResult InvokeEditorCommand(EditorInteractionContext& context, CommandId command, CommandSource source);

    EditorInteractionDiagnostics ValidateEditorInteractionContext(const EditorInteractionContext& context);
    std::string FormatEditorInteractionEvent(const EditorInteractionEvent& event);
    std::string FormatEditorPropertyChangeRequest(const EditorPropertyChangeRequest& request);
    std::string FormatEditorSelectionChangeRequest(const EditorSelectionChangeRequest& request);
    std::string FormatEditorInteractionPaletteState(const EditorInteractionPaletteState& palette);
    std::string FormatEditorInteractionDiagnostics(const EditorInteractionDiagnostics& diagnostics);
}
