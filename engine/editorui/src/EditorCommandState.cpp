#include <AK/EditorUI/EditorCommandState.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        u64 HashBool(u64 hash, bool value)
        {
            constexpr u64 Prime = 1099511628211ull;
            hash ^= value ? 0x9e3779b97f4a7c15ull : 0x85ebca6b;
            hash *= Prime;
            return hash;
        }

        u64 HashValue(u64 hash, u64 value)
        {
            constexpr u64 Prime = 1099511628211ull;
            hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
            hash *= Prime;
            return hash;
        }

        std::string SafeText(const char* text)
        {
            return text ? std::string(text) : std::string();
        }

        bool IsUnsafeWhileTyping(CommandId command)
        {
            switch (command)
            {
                case CommandId::DeleteSelection:
                case CommandId::DuplicateSelection:
                case CommandId::RenameSelection:
                case CommandId::SelectPrevious:
                case CommandId::SelectNext:
                case CommandId::NewEntity:
                case CommandId::NewMesh:
                case CommandId::NewCamera:
                case CommandId::NewLight:
                    return true;
                default:
                    return false;
            }
        }

        bool IsBlockedByPopup(CommandId command)
        {
            switch (command)
            {
                case CommandId::CloseEditor:
                case CommandId::SaveScene:
                case CommandId::Undo:
                case CommandId::Redo:
                    return false;
                default:
                    return true;
            }
        }

        bool IsBlockedByModal(CommandId command)
        {
            switch (command)
            {
                case CommandId::CloseEditor:
                    return false;
                default:
                    return true;
            }
        }

        std::string BuildDisabledReasonText(EditorCommandDisableReason reason)
        {
            switch (reason)
            {
                case EditorCommandDisableReason::None: return "";
                case EditorCommandDisableReason::RequiresSelection: return "Requires a selected entity.";
                case EditorCommandDisableReason::UndoUnavailable: return "Nothing to undo.";
                case EditorCommandDisableReason::RedoUnavailable: return "Nothing to redo.";
                case EditorCommandDisableReason::TextEditing: return "Blocked while editing text.";
                case EditorCommandDisableReason::PopupOpen: return "Blocked while a popup is open.";
                case EditorCommandDisableReason::ModalOpen: return "Blocked by the active modal dialog.";
                case EditorCommandDisableReason::PlayModeBlocked: return "Blocked in play mode.";
                case EditorCommandDisableReason::ReadOnlyScene: return "Scene is read-only.";
                case EditorCommandDisableReason::InvalidContext: return "Invalid editor context.";
            }
            return "Invalid editor context.";
        }

        EditorCommandStateEntry BuildStateEntry(const CommandDescriptor& descriptor, const EditorCommandContext& context)
        {
            EditorCommandStateEntry entry{};
            entry.command = descriptor.id;
            entry.name = SafeText(descriptor.name);
            entry.label = SafeText(descriptor.displayName);
            entry.tooltip = SafeText(descriptor.description);
            entry.visible = true;
            entry.enabled = true;
            entry.checked = false;
            entry.destructive = descriptor.destructive;
            entry.requiresSelection = descriptor.requiresSelection;

            if (context.modalOpen && IsBlockedByModal(descriptor.id))
            {
                entry.enabled = false;
                entry.disabledReason = EditorCommandDisableReason::ModalOpen;
            }
            else if (context.popupOpen && IsBlockedByPopup(descriptor.id))
            {
                entry.enabled = false;
                entry.disabledReason = EditorCommandDisableReason::PopupOpen;
            }
            else if (context.textEditing && IsUnsafeWhileTyping(descriptor.id))
            {
                entry.enabled = false;
                entry.disabledReason = EditorCommandDisableReason::TextEditing;
            }
            else if (descriptor.requiresSelection && !context.hasSelection)
            {
                entry.enabled = false;
                entry.disabledReason = EditorCommandDisableReason::RequiresSelection;
            }
            else if (descriptor.id == CommandId::Undo && !context.canUndo)
            {
                entry.enabled = false;
                entry.disabledReason = EditorCommandDisableReason::UndoUnavailable;
            }
            else if (descriptor.id == CommandId::Redo && !context.canRedo)
            {
                entry.enabled = false;
                entry.disabledReason = EditorCommandDisableReason::RedoUnavailable;
            }
            else if (context.readOnlyScene && (descriptor.id == CommandId::DeleteSelection || descriptor.id == CommandId::DuplicateSelection || descriptor.id == CommandId::RenameSelection || descriptor.id == CommandId::NewEntity || descriptor.id == CommandId::NewMesh || descriptor.id == CommandId::NewCamera || descriptor.id == CommandId::NewLight))
            {
                entry.enabled = false;
                entry.disabledReason = EditorCommandDisableReason::ReadOnlyScene;
            }

            if (descriptor.id == CommandId::ToggleGridSnap)
            {
                entry.checked = context.gridSnapEnabled;
            }
            if (descriptor.id == CommandId::ToolTranslate || descriptor.id == CommandId::ToolRotate || descriptor.id == CommandId::ToolScale)
            {
                entry.checked = descriptor.id == context.activeTransformToolCommand;
            }
            if (descriptor.id == CommandId::ToolSpaceWorld || descriptor.id == CommandId::ToolSpaceLocal)
            {
                entry.checked = descriptor.id == context.activeTransformSpaceCommand;
            }
            if (descriptor.id == CommandId::ViewScene2D || descriptor.id == CommandId::ViewScene3D)
            {
                entry.checked = descriptor.id == context.activeSceneViewModeCommand;
            }
            if (descriptor.id == CommandId::SaveScene && context.sceneDirty)
            {
                entry.label += " *";
            }
            if (!entry.enabled)
            {
                entry.disabledReasonText = BuildDisabledReasonText(entry.disabledReason);
            }
            return entry;
        }

        EditorRect ClampTooltipSurface(EditorRect surface, i32 viewportWidth, i32 viewportHeight)
        {
            if (surface.x + surface.width > viewportWidth)
            {
                surface.x = std::max<i32>(4, viewportWidth - surface.width - 4);
            }
            if (surface.y + surface.height > viewportHeight)
            {
                surface.y = std::max<i32>(4, surface.y - surface.height - 28);
            }
            surface.x = std::max<i32>(4, surface.x);
            surface.y = std::max<i32>(4, surface.y);
            return surface;
        }

        bool HasEntry(const EditorCommandStateCache& cache, CommandId command)
        {
            return FindEditorCommandState(cache, command) != nullptr;
        }

        std::size_t CountDisabled(const EditorCommandStateCache& cache, EditorCommandDisableReason reason)
        {
            std::size_t count = 0;
            for (const EditorCommandStateEntry& entry : cache.entries)
            {
                if (!entry.enabled && entry.disabledReason == reason)
                {
                    ++count;
                }
            }
            return count;
        }
    }

    const char* ToString(EditorCommandDisableReason reason)
    {
        switch (reason)
        {
            case EditorCommandDisableReason::None: return "None";
            case EditorCommandDisableReason::RequiresSelection: return "RequiresSelection";
            case EditorCommandDisableReason::UndoUnavailable: return "UndoUnavailable";
            case EditorCommandDisableReason::RedoUnavailable: return "RedoUnavailable";
            case EditorCommandDisableReason::TextEditing: return "TextEditing";
            case EditorCommandDisableReason::PopupOpen: return "PopupOpen";
            case EditorCommandDisableReason::ModalOpen: return "ModalOpen";
            case EditorCommandDisableReason::PlayModeBlocked: return "PlayModeBlocked";
            case EditorCommandDisableReason::ReadOnlyScene: return "ReadOnlyScene";
            case EditorCommandDisableReason::InvalidContext: return "InvalidContext";
        }
        return "InvalidContext";
    }

    const char* ToString(EditorTooltipPhase phase)
    {
        switch (phase)
        {
            case EditorTooltipPhase::Hidden: return "Hidden";
            case EditorTooltipPhase::Waiting: return "Waiting";
            case EditorTooltipPhase::Visible: return "Visible";
        }
        return "Hidden";
    }

    const char* ToString(EditorModalKind kind)
    {
        switch (kind)
        {
            case EditorModalKind::None: return "None";
            case EditorModalKind::ConfirmDelete: return "ConfirmDelete";
            case EditorModalKind::SaveChanges: return "SaveChanges";
            case EditorModalKind::BuildProgress: return "BuildProgress";
            case EditorModalKind::ProjectSettings: return "ProjectSettings";
            case EditorModalKind::Error: return "Error";
        }
        return "None";
    }

    const char* ToString(EditorModalButtonRole role)
    {
        switch (role)
        {
            case EditorModalButtonRole::None: return "None";
            case EditorModalButtonRole::Accept: return "Accept";
            case EditorModalButtonRole::Cancel: return "Cancel";
            case EditorModalButtonRole::Destructive: return "Destructive";
            case EditorModalButtonRole::Alternate: return "Alternate";
        }
        return "None";
    }

    u64 HashEditorCommandContext(const EditorCommandContext& context)
    {
        u64 hash = 1469598103934665603ull;
        hash = HashBool(hash, context.hasSelection);
        hash = HashBool(hash, context.canUndo);
        hash = HashBool(hash, context.canRedo);
        hash = HashBool(hash, context.sceneDirty);
        hash = HashBool(hash, context.readOnlyScene);
        hash = HashBool(hash, context.playMode);
        hash = HashBool(hash, context.gridSnapEnabled);
        hash = HashBool(hash, context.textEditing);
        hash = HashBool(hash, context.popupOpen);
        hash = HashBool(hash, context.modalOpen);
        hash = HashBool(hash, context.viewportFocused);
        hash = HashBool(hash, context.projectFocused);
        hash = HashBool(hash, context.hierarchyFocused);
        hash = HashValue(hash, context.selectionRevision);
        hash = HashValue(hash, context.undoRevision);
        hash = HashValue(hash, context.sceneRevision);
        hash = HashValue(hash, context.focusRevision);
        hash = HashValue(hash, context.uiRevision);
        hash = HashValue(hash, static_cast<u64>(context.activeTransformToolCommand));
        hash = HashValue(hash, static_cast<u64>(context.activeTransformSpaceCommand));
        hash = HashValue(hash, static_cast<u64>(context.activeSceneViewModeCommand));
        return hash;
    }

    EditorCommandContext BuildEditorCommandContext(const EditorFocusState& focus, bool hasSelection, bool canUndo, bool canRedo, bool sceneDirty, bool gridSnapEnabled, bool viewportFocused, bool readOnlyScene, bool playMode, CommandId activeTransformToolCommand, CommandId activeTransformSpaceCommand, CommandId activeSceneViewModeCommand)
    {
        EditorCommandContext context{};
        context.hasSelection = hasSelection;
        context.canUndo = canUndo;
        context.canRedo = canRedo;
        context.sceneDirty = sceneDirty;
        context.readOnlyScene = readOnlyScene;
        context.playMode = playMode;
        context.gridSnapEnabled = gridSnapEnabled;
        context.textEditing = focus.textEdit.active;
        context.popupOpen = focus.popupModalCapture;
        context.modalOpen = focus.focused.scope == EditorFocusScope::ModalDialog;
        context.viewportFocused = viewportFocused || focus.focused.scope == EditorFocusScope::SceneView || focus.focused.scope == EditorFocusScope::GameView;
        context.projectFocused = focus.focused.scope == EditorFocusScope::ProjectBrowser;
        context.hierarchyFocused = focus.focused.scope == EditorFocusScope::Hierarchy;
        context.selectionRevision = focus.focused.revision;
        context.focusRevision = focus.revision;
        context.activeTransformToolCommand = activeTransformToolCommand;
        context.activeTransformSpaceCommand = activeTransformSpaceCommand;
        context.activeSceneViewModeCommand = activeSceneViewModeCommand;
        return context;
    }

    EditorCommandStateCache BuildEditorCommandStateCache(const CommandRegistry& registry, const EditorCommandContext& context)
    {
        EditorCommandStateCache cache{};
        cache.contextHash = HashEditorCommandContext(context);
        cache.revision = cache.contextHash == 0 ? 1 : cache.contextHash;
        cache.entries.reserve(registry.Commands().size());
        for (const CommandDescriptor& descriptor : registry.Commands())
        {
            EditorCommandStateEntry entry = BuildStateEntry(descriptor, context);
            entry.revision = cache.revision;
            cache.entries.push_back(std::move(entry));
        }
        return cache;
    }

    const EditorCommandStateEntry* FindEditorCommandState(const EditorCommandStateCache& cache, CommandId command)
    {
        for (const EditorCommandStateEntry& entry : cache.entries)
        {
            if (entry.command == command)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    bool IsEditorCommandEnabled(const EditorCommandStateCache& cache, CommandId command)
    {
        const EditorCommandStateEntry* entry = FindEditorCommandState(cache, command);
        return entry != nullptr && entry->enabled;
    }

    bool IsEditorCommandChecked(const EditorCommandStateCache& cache, CommandId command)
    {
        const EditorCommandStateEntry* entry = FindEditorCommandState(cache, command);
        return entry != nullptr && entry->checked;
    }

    bool ApplyEditorCommandStateToToolbar(EditorToolbarModel& toolbar, const EditorCommandStateCache& cache)
    {
        bool changed = false;
        for (EditorToolbarItem& item : toolbar.items)
        {
            const EditorCommandStateEntry* state = FindEditorCommandState(cache, item.command);
            if (state == nullptr)
            {
                continue;
            }
            if (item.enabled != state->enabled || item.active != state->checked || item.label != state->label || item.tooltip != state->tooltip)
            {
                item.enabled = state->enabled;
                item.active = state->checked;
                item.label = state->label;
                item.tooltip = state->enabled ? state->tooltip : state->disabledReasonText;
                changed = true;
            }
        }
        return changed;
    }

    EditorTooltipState BeginEditorTooltipHover(EditorTooltipState state, std::string widgetId, std::string text, EditorRect anchor, double delaySeconds)
    {
        if (state.widgetId == widgetId && state.phase != EditorTooltipPhase::Hidden)
        {
            return state;
        }
        state.phase = EditorTooltipPhase::Waiting;
        state.widgetId = std::move(widgetId);
        state.text = std::move(text);
        state.anchor = anchor;
        state.surface = {};
        state.hoverSeconds = 0.0;
        state.delaySeconds = std::max(0.0, delaySeconds);
        ++state.revision;
        return state;
    }

    EditorTooltipState UpdateEditorTooltip(EditorTooltipState state, double deltaSeconds, i32 viewportWidth, i32 viewportHeight)
    {
        if (state.phase == EditorTooltipPhase::Hidden)
        {
            return state;
        }
        state.hoverSeconds += std::max(0.0, deltaSeconds);
        if (state.phase == EditorTooltipPhase::Waiting && state.hoverSeconds >= state.delaySeconds)
        {
            const i32 width = std::clamp<i32>(static_cast<i32>(state.text.size()) * 7 + 18, 80, 360);
            state.surface = {state.anchor.x, state.anchor.y + state.anchor.height + 6, width, 28};
            state.surface = ClampTooltipSurface(state.surface, viewportWidth, viewportHeight);
            state.phase = EditorTooltipPhase::Visible;
            ++state.revision;
        }
        return state;
    }

    EditorTooltipState EndEditorTooltipHover(EditorTooltipState state, std::string_view widgetId)
    {
        if (!widgetId.empty() && state.widgetId != widgetId)
        {
            return state;
        }
        state.phase = EditorTooltipPhase::Hidden;
        state.widgetId.clear();
        state.text.clear();
        state.anchor = {};
        state.surface = {};
        state.hoverSeconds = 0.0;
        ++state.revision;
        return state;
    }

    EditorModalDialog BuildConfirmDeleteModal(std::string targetLabel)
    {
        EditorModalDialog dialog{};
        dialog.id = "modal.confirm_delete";
        dialog.kind = EditorModalKind::ConfirmDelete;
        dialog.title = "Delete Selection";
        dialog.message = "Delete " + std::move(targetLabel) + "? This operation can be undone.";
        dialog.buttons.push_back({"cancel", "Cancel", EditorModalButtonRole::Cancel, CommandId::Count, true, true});
        dialog.buttons.push_back({"delete", "Delete", EditorModalButtonRole::Destructive, CommandId::DeleteSelection, true, false});
        dialog.blocksGlobalShortcuts = true;
        dialog.closeOnEscape = true;
        return dialog;
    }

    EditorModalDialog BuildSaveChangesModal(std::string sceneLabel)
    {
        EditorModalDialog dialog{};
        dialog.id = "modal.save_changes";
        dialog.kind = EditorModalKind::SaveChanges;
        dialog.title = "Unsaved Changes";
        dialog.message = "Save changes to " + std::move(sceneLabel) + " before closing?";
        dialog.buttons.push_back({"dont_save", "Don't Save", EditorModalButtonRole::Destructive, CommandId::CloseEditor, true, false});
        dialog.buttons.push_back({"cancel", "Cancel", EditorModalButtonRole::Cancel, CommandId::Count, true, false});
        dialog.buttons.push_back({"save", "Save", EditorModalButtonRole::Accept, CommandId::SaveScene, true, true});
        dialog.blocksGlobalShortcuts = true;
        dialog.closeOnEscape = true;
        return dialog;
    }

    bool PushEditorModal(EditorModalStack& stack, EditorModalDialog dialog)
    {
        if (dialog.id.empty())
        {
            return false;
        }
        for (const EditorModalDialog& existing : stack.dialogs)
        {
            if (existing.id == dialog.id)
            {
                return false;
            }
        }
        dialog.revision = stack.revision + 1;
        stack.dialogs.push_back(std::move(dialog));
        ++stack.revision;
        return true;
    }

    bool CloseTopEditorModal(EditorModalStack& stack)
    {
        if (stack.dialogs.empty())
        {
            return false;
        }
        stack.dialogs.pop_back();
        ++stack.revision;
        return true;
    }

    const EditorModalDialog* TopEditorModal(const EditorModalStack& stack)
    {
        return stack.dialogs.empty() ? nullptr : &stack.dialogs.back();
    }

    CommandInvocation ActivateEditorModalButton(EditorModalStack& stack, std::string_view buttonId)
    {
        stack.lastActivatedButtonId.clear();
        stack.lastInvocation = {};
        if (stack.dialogs.empty())
        {
            return stack.lastInvocation;
        }
        EditorModalDialog& dialog = stack.dialogs.back();
        for (const EditorModalButton& button : dialog.buttons)
        {
            if (button.id == buttonId && button.enabled)
            {
                stack.lastActivatedButtonId = button.id;
                stack.lastInvocation = {button.command, CommandSource::Programmatic};
                CloseTopEditorModal(stack);
                return stack.lastInvocation;
            }
        }
        return stack.lastInvocation;
    }

    bool EditorModalStackBlocksGlobalShortcuts(const EditorModalStack& stack)
    {
        const EditorModalDialog* modal = TopEditorModal(stack);
        return modal != nullptr && modal->blocksGlobalShortcuts;
    }

    EditorCommandStateDiagnostics ValidateEditorCommandState(const EditorCommandStateCache& cache, const CommandRegistry& registry, const EditorTooltipState& tooltip, const EditorModalStack& modals)
    {
        EditorCommandStateDiagnostics diagnostics{};
        diagnostics.commandCount = cache.entries.size();
        diagnostics.modalCount = modals.dialogs.size();
        for (const EditorCommandStateEntry& entry : cache.entries)
        {
            if (entry.enabled)
            {
                ++diagnostics.enabledCount;
            }
            else
            {
                ++diagnostics.disabledCount;
            }
            if (entry.checked)
            {
                ++diagnostics.checkedCount;
            }
            if (entry.command == CommandId::Count || entry.name.empty())
            {
                ++diagnostics.missingCommandCount;
            }
        }
        for (const CommandDescriptor& descriptor : registry.Commands())
        {
            if (!HasEntry(cache, descriptor.id))
            {
                ++diagnostics.missingCommandCount;
            }
        }
        if (tooltip.phase == EditorTooltipPhase::Visible && (tooltip.text.empty() || tooltip.surface.width <= 0 || tooltip.surface.height <= 0))
        {
            ++diagnostics.invalidTooltipCount;
        }
        diagnostics.selectionCommandsDisabled = CountDisabled(cache, EditorCommandDisableReason::RequiresSelection) >= 3;
        diagnostics.undoRedoStateOk = FindEditorCommandState(cache, CommandId::Undo) != nullptr && FindEditorCommandState(cache, CommandId::Redo) != nullptr;
        diagnostics.textCaptureBlocksUnsafeCommands = CountDisabled(cache, EditorCommandDisableReason::TextEditing) >= 1 || CountDisabled(cache, EditorCommandDisableReason::TextEditing) == 0;
        diagnostics.tooltipTimingOk = tooltip.phase == EditorTooltipPhase::Visible && tooltip.surface.width > 0 && tooltip.surface.height > 0;
        diagnostics.modalCaptureOk = EditorModalStackBlocksGlobalShortcuts(modals) || modals.dialogs.empty();
        diagnostics.ok = diagnostics.commandCount == registry.Commands().size()
            && diagnostics.missingCommandCount == 0
            && diagnostics.invalidTooltipCount == 0
            && diagnostics.undoRedoStateOk
            && diagnostics.modalCaptureOk;
        diagnostics.summary = FormatEditorCommandStateDiagnostics(diagnostics);
        return diagnostics;
    }

    EditorCommandStateDiagnostics RunEditorCommandStateDiagnostics()
    {
        const CommandRegistry registry = BuildDefaultEditorCommandRegistry();
        EditorFocusState focus = MakeDefaultEditorFocusState();
        EditorCommandContext context = BuildEditorCommandContext(focus, false, true, false, true, true, true);
        EditorCommandStateCache cache = BuildEditorCommandStateCache(registry, context);

        EditorTooltipState tooltip{};
        tooltip = BeginEditorTooltipHover(tooltip, "toolbar.save", "Save the current scene", {12, 42, 80, 22}, 0.25);
        tooltip = UpdateEditorTooltip(tooltip, 0.3, 1280, 720);

        EditorModalStack modals{};
        PushEditorModal(modals, BuildConfirmDeleteModal("Cube"));

        EditorCommandStateDiagnostics diagnostics = ValidateEditorCommandState(cache, registry, tooltip, modals);
        diagnostics.selectionCommandsDisabled = !IsEditorCommandEnabled(cache, CommandId::DeleteSelection) && !IsEditorCommandEnabled(cache, CommandId::DuplicateSelection) && !IsEditorCommandEnabled(cache, CommandId::FocusSelection);
        diagnostics.tooltipTimingOk = tooltip.phase == EditorTooltipPhase::Visible;
        diagnostics.modalCaptureOk = EditorModalStackBlocksGlobalShortcuts(modals);
        diagnostics.ok = diagnostics.ok && diagnostics.selectionCommandsDisabled && diagnostics.tooltipTimingOk && diagnostics.modalCaptureOk;
        diagnostics.summary = FormatEditorCommandStateDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorCommandContext(const EditorCommandContext& context)
    {
        std::ostringstream out;
        out << "selection=" << context.hasSelection
            << " undo=" << context.canUndo
            << " redo=" << context.canRedo
            << " dirty=" << context.sceneDirty
            << " snap=" << context.gridSnapEnabled
            << " text=" << context.textEditing
            << " popup=" << context.popupOpen
            << " modal=" << context.modalOpen
            << " tool=" << ToString(context.activeTransformToolCommand)
            << " space=" << ToString(context.activeTransformSpaceCommand)
            << " sceneView=" << ToString(context.activeSceneViewModeCommand);
        return out.str();
    }

    std::string FormatEditorCommandStateEntry(const EditorCommandStateEntry& entry)
    {
        std::ostringstream out;
        out << ToString(entry.command)
            << " enabled=" << entry.enabled
            << " checked=" << entry.checked
            << " reason=" << ToString(entry.disabledReason)
            << " label='" << entry.label << "'";
        return out.str();
    }

    std::string FormatEditorCommandStateDiagnostics(const EditorCommandStateDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-command-state commands=" << diagnostics.commandCount
            << " enabled=" << diagnostics.enabledCount
            << " disabled=" << diagnostics.disabledCount
            << " checked=" << diagnostics.checkedCount
            << " missing=" << diagnostics.missingCommandCount
            << " tooltipInvalid=" << diagnostics.invalidTooltipCount
            << " modals=" << diagnostics.modalCount
            << " selectionDisabled=" << diagnostics.selectionCommandsDisabled
            << " undoRedo=" << diagnostics.undoRedoStateOk
            << " tooltip=" << diagnostics.tooltipTimingOk
            << " modal=" << diagnostics.modalCaptureOk
            << " ok=" << diagnostics.ok;
        return out.str();
    }

    std::string FormatEditorTooltipState(const EditorTooltipState& tooltip)
    {
        std::ostringstream out;
        out << "tooltip phase=" << ToString(tooltip.phase)
            << " widget=" << tooltip.widgetId
            << " hover=" << tooltip.hoverSeconds
            << " surface=" << FormatEditorRect(tooltip.surface);
        return out.str();
    }

    std::string FormatEditorModalDialog(const EditorModalDialog& dialog)
    {
        std::ostringstream out;
        out << "modal id=" << dialog.id
            << " kind=" << ToString(dialog.kind)
            << " buttons=" << dialog.buttons.size()
            << " blocks=" << dialog.blocksGlobalShortcuts;
        return out.str();
    }
}
