#include <AK/EditorUI/EditorInteraction.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace AK
{
    namespace
    {
        bool IsSameStableSelection(const EditorSelectionSnapshot& before, const EditorSelectionSnapshot& after)
        {
            if (before.items.size() != after.items.size())
            {
                return false;
            }
            for (std::size_t i = 0; i < before.items.size(); ++i)
            {
                if (before.items[i].stableId != after.items[i].stableId || before.items[i].domain != after.items[i].domain)
                {
                    return false;
                }
            }
            return before.active.stableId == after.active.stableId && before.active.domain == after.active.domain;
        }

        EditorPropertyDesc* FindProperty(EditorPanelModelFrame& frame, const std::string& path)
        {
            for (EditorComponentInspector& component : frame.inspector.components)
            {
                for (EditorPropertyDesc& property : component.properties)
                {
                    if (property.path == path)
                    {
                        return &property;
                    }
                }
            }
            return nullptr;
        }


        const EditorAssetItem* FindAsset(const EditorAssetBrowserModel& model, const std::string& stableId)
        {
            for (const EditorAssetItem& item : model.items)
            {
                if (item.guid == stableId || std::string("asset:") + item.guid == stableId)
                {
                    return &item;
                }
            }
            return nullptr;
        }

        const EditorHierarchyNode* FindHierarchyNodeRecursive(const EditorHierarchyNode& node, const std::string& stableId)
        {
            if (node.stableId == stableId)
            {
                return &node;
            }
            for (const EditorHierarchyNode& child : node.children)
            {
                if (const EditorHierarchyNode* found = FindHierarchyNodeRecursive(child, stableId))
                {
                    return found;
                }
            }
            return nullptr;
        }

        const EditorHierarchyNode* FindHierarchyNode(const EditorHierarchyModel& model, const std::string& stableId)
        {
            for (const EditorHierarchyNode& root : model.roots)
            {
                if (const EditorHierarchyNode* found = FindHierarchyNodeRecursive(root, stableId))
                {
                    return found;
                }
            }
            return nullptr;
        }

        void SetHierarchySelectedRecursive(EditorHierarchyNode& node, const std::string& stableId)
        {
            node.selected = (node.stableId == stableId);
            for (EditorHierarchyNode& child : node.children)
            {
                SetHierarchySelectedRecursive(child, stableId);
            }
        }

        void SetHierarchySelected(EditorHierarchyModel& model, const std::string& stableId)
        {
            for (EditorHierarchyNode& root : model.roots)
            {
                SetHierarchySelectedRecursive(root, stableId);
            }
            ++model.revision;
        }

        bool IsWritableProperty(const EditorPropertyDesc& property)
        {
            return !HasFlag(property.flags, EditorPropertyFlag::ReadOnly) && property.value.type != EditorPropertyType::Section;
        }

        bool ValuesDiffer(const EditorPropertyValue& a, const EditorPropertyValue& b)
        {
            if (a.type != b.type || a.text != b.text || a.boolean != b.boolean)
            {
                return true;
            }
            for (int i = 0; i < 4; ++i)
            {
                if (a.numbers[i] != b.numbers[i])
                {
                    return true;
                }
            }
            return false;
        }

        void PushMessage(EditorInteractionFrameResult& result, std::string message)
        {
            result.messages.push_back(std::move(message));
        }

        void RebuildPanelSearch(EditorInteractionContext& context)
        {
            context.panels.searchIndex = BuildPanelModelSearchIndex(context.panels, context.bridge.searchIndex);
        }

        EditorSelectionItem SelectionFromSearchResult(const EditorSearchResult& result)
        {
            EditorSelectionItem item{};
            item.stableId = result.stableId;
            item.displayName = result.title;
            if (result.kind == EditorSearchResultKind::Entity)
            {
                item.domain = EditorSelectionDomain::SceneEntity;
                item.entity = result.entity;
            }
            else if (result.kind == EditorSearchResultKind::Asset)
            {
                item.domain = EditorSelectionDomain::Asset;
                if (item.stableId.rfind("asset:", 0) == 0)
                {
                    item.stableId = item.stableId.substr(6);
                }
            }
            else if (result.kind == EditorSearchResultKind::Panel)
            {
                item.domain = EditorSelectionDomain::Panel;
            }
            return item;
        }

        void AppendCommand(EditorInteractionContext& context, EditorInteractionFrameResult& result, CommandId command, CommandSource source)
        {
            CommandInvocation invocation{command, source};
            result.commands.push_back(invocation);
            context.pendingCommands.push_back(invocation);
            result.consumed = true;
            ++context.revision;
        }
    }

    const char* ToString(EditorInteractionKind kind)
    {
        switch (kind)
        {
            case EditorInteractionKind::None: return "None";
            case EditorInteractionKind::PointerSelect: return "PointerSelect";
            case EditorInteractionKind::CommandShortcut: return "CommandShortcut";
            case EditorInteractionKind::CommandPaletteOpen: return "CommandPaletteOpen";
            case EditorInteractionKind::CommandPaletteQuery: return "CommandPaletteQuery";
            case EditorInteractionKind::CommandPaletteMove: return "CommandPaletteMove";
            case EditorInteractionKind::CommandPaletteAccept: return "CommandPaletteAccept";
            case EditorInteractionKind::HierarchySelect: return "HierarchySelect";
            case EditorInteractionKind::AssetSelect: return "AssetSelect";
            case EditorInteractionKind::PropertyBeginEdit: return "PropertyBeginEdit";
            case EditorInteractionKind::PropertyCommitEdit: return "PropertyCommitEdit";
            case EditorInteractionKind::PropertyCancelEdit: return "PropertyCancelEdit";
            case EditorInteractionKind::PanelFocus: return "PanelFocus";
            case EditorInteractionKind::OverlayToggle: return "OverlayToggle";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorPropertyChangeStatus status)
    {
        switch (status)
        {
            case EditorPropertyChangeStatus::Pending: return "Pending";
            case EditorPropertyChangeStatus::Accepted: return "Accepted";
            case EditorPropertyChangeStatus::Rejected: return "Rejected";
            default: return "Unknown";
        }
    }

    const char* ToString(EditorSelectionChangeReason reason)
    {
        switch (reason)
        {
            case EditorSelectionChangeReason::UserClick: return "UserClick";
            case EditorSelectionChangeReason::KeyboardNavigation: return "KeyboardNavigation";
            case EditorSelectionChangeReason::SearchResult: return "SearchResult";
            case EditorSelectionChangeReason::Command: return "Command";
            case EditorSelectionChangeReason::Programmatic: return "Programmatic";
            default: return "Unknown";
        }
    }

    EditorInteractionContext BuildDefaultEditorInteractionContext(i32 width, i32 height)
    {
        EditorInteractionContext context{};
        context.bridge = BuildDefaultEditorRuntimeBridge(width, height);
        context.panels = BuildDefaultEditorPanelModelFrame(context.bridge);
        context.palette.results = SearchEditorIndex(context.panels.searchIndex, {"", 16, true});
        context.revision = context.bridge.revision + context.panels.revision;
        return context;
    }

    EditorInteractionFrameResult ProcessEditorInteraction(EditorInteractionContext& context, const EditorInteractionEvent& event)
    {
        switch (event.kind)
        {
            case EditorInteractionKind::CommandShortcut:
                return InvokeEditorCommand(context, event.command, event.commandSource);
            case EditorInteractionKind::CommandPaletteOpen:
            {
                EditorInteractionFrameResult result{};
                result.consumed = OpenEditorCommandPalette(context, event.text);
                result.modelDirty = result.consumed;
                if (result.consumed)
                {
                    PushMessage(result, "command palette opened");
                }
                return result;
            }
            case EditorInteractionKind::CommandPaletteQuery:
            {
                EditorInteractionFrameResult result{};
                result.consumed = UpdateEditorCommandPalette(context, event.text);
                result.modelDirty = result.consumed;
                if (result.consumed)
                {
                    PushMessage(result, "command palette query updated");
                }
                return result;
            }
            case EditorInteractionKind::CommandPaletteMove:
            {
                EditorInteractionFrameResult result{};
                result.consumed = MoveEditorCommandPaletteSelection(context, event.direction);
                return result;
            }
            case EditorInteractionKind::CommandPaletteAccept:
                return AcceptEditorCommandPaletteSelection(context);
            case EditorInteractionKind::HierarchySelect:
                return SelectEditorHierarchyItem(context, event.stableId, EditorSelectionChangeReason::UserClick);
            case EditorInteractionKind::AssetSelect:
                return SelectEditorAssetItem(context, event.stableId, EditorSelectionChangeReason::UserClick);
            case EditorInteractionKind::PropertyBeginEdit:
            {
                EditorInteractionFrameResult result{};
                result.consumed = BeginEditorPropertyEdit(context, event.propertyPath);
                return result;
            }
            case EditorInteractionKind::PropertyCommitEdit:
                return CommitEditorPropertyEdit(context, event.propertyPath, event.propertyValue);
            case EditorInteractionKind::PropertyCancelEdit:
            {
                EditorInteractionFrameResult result{};
                result.consumed = CancelEditorPropertyEdit(context);
                return result;
            }
            case EditorInteractionKind::PanelFocus:
            {
                EditorInteractionFrameResult result{};
                const EditorPanelDesc* panel = context.bridge.panels.FindByName(event.text);
                if (panel)
                {
                    FocusEditorPanel(context.bridge, panel->id);
                    result.consumed = true;
                    result.layoutDirty = true;
                    ++context.revision;
                }
                return result;
            }
            case EditorInteractionKind::OverlayToggle:
            {
                EditorInteractionFrameResult result{};
                const bool visible = event.direction >= 0;
                result.consumed = ToggleEditorOverlay(context.bridge, event.overlayId, visible);
                result.layoutDirty = result.consumed;
                if (result.consumed)
                {
                    PushMessage(result, std::string("overlay toggled id=") + std::to_string(event.overlayId));
                    ++context.revision;
                }
                return result;
            }
            case EditorInteractionKind::PointerSelect:
            {
                EditorInteractionFrameResult result{};
                const EditorPointerEvent pointer{event.x, event.y, EditorPointerButton::Left, true, false, false};
                std::vector<CommandInvocation> commands = RouteEditorPointerEvent(context.bridge, pointer);
                for (const CommandInvocation& command : commands)
                {
                    result.commands.push_back(command);
                    context.pendingCommands.push_back(command);
                }
                result.consumed = !commands.empty();
                return result;
            }
            default:
                return {};
        }
    }

    bool OpenEditorCommandPalette(EditorInteractionContext& context, std::string query)
    {
        context.palette.open = true;
        context.palette.query = std::move(query);
        context.palette.selectedIndex = 0;
        context.palette.results = SearchEditorIndex(context.panels.searchIndex, {context.palette.query, 24, true});
        ++context.palette.revision;
        ++context.revision;
        return true;
    }

    bool UpdateEditorCommandPalette(EditorInteractionContext& context, std::string query)
    {
        if (!context.palette.open)
        {
            context.palette.open = true;
        }
        context.palette.query = std::move(query);
        context.palette.results = SearchEditorIndex(context.panels.searchIndex, {context.palette.query, 24, true});
        if (context.palette.selectedIndex >= context.palette.results.size())
        {
            context.palette.selectedIndex = context.palette.results.empty() ? 0 : context.palette.results.size() - 1;
        }
        ++context.palette.revision;
        ++context.revision;
        return true;
    }

    bool MoveEditorCommandPaletteSelection(EditorInteractionContext& context, i32 delta)
    {
        if (!context.palette.open || context.palette.results.empty())
        {
            return false;
        }
        const i32 count = static_cast<i32>(context.palette.results.size());
        i32 next = static_cast<i32>(context.palette.selectedIndex) + delta;
        while (next < 0)
        {
            next += count;
        }
        next %= count;
        context.palette.selectedIndex = static_cast<std::size_t>(next);
        ++context.palette.revision;
        ++context.revision;
        return true;
    }

    EditorInteractionFrameResult AcceptEditorCommandPaletteSelection(EditorInteractionContext& context)
    {
        EditorInteractionFrameResult result{};
        if (!context.palette.open || context.palette.results.empty() || context.palette.selectedIndex >= context.palette.results.size())
        {
            return result;
        }

        const EditorSearchResult selected = context.palette.results[context.palette.selectedIndex];
        result.consumed = true;
        context.palette.open = false;
        ++context.palette.revision;
        ++context.revision;

        if (selected.kind == EditorSearchResultKind::Command)
        {
            AppendCommand(context, result, selected.command, CommandSource::CommandPalette);
            PushMessage(result, std::string("command palette accepted command ") + ToString(selected.command));
        }
        else if (selected.kind == EditorSearchResultKind::Entity)
        {
            return SelectEditorHierarchyItem(context, selected.stableId, EditorSelectionChangeReason::SearchResult);
        }
        else if (selected.kind == EditorSearchResultKind::Asset)
        {
            EditorSelectionItem item = SelectionFromSearchResult(selected);
            return SelectEditorAssetItem(context, item.stableId, EditorSelectionChangeReason::SearchResult);
        }
        else if (selected.kind == EditorSearchResultKind::Panel)
        {
            FocusEditorPanel(context.bridge, selected.panel);
            result.layoutDirty = true;
            PushMessage(result, std::string("focused panel ") + selected.title);
        }
        else if (selected.kind == EditorSearchResultKind::Setting)
        {
            const std::string prefix = "property:";
            if (selected.stableId.rfind(prefix, 0) == 0)
            {
                BeginEditorPropertyEdit(context, selected.stableId.substr(prefix.size()));
                result.modelDirty = true;
                PushMessage(result, std::string("editing property ") + selected.title);
            }
        }
        return result;
    }

    bool BeginEditorPropertyEdit(EditorInteractionContext& context, const std::string& propertyPath)
    {
        const EditorPropertyDesc* property = FindProperty(context.panels, propertyPath);
        if (!property || !IsWritableProperty(*property))
        {
            return false;
        }
        context.propertyEdit.editing = true;
        context.propertyEdit.propertyPath = propertyPath;
        context.propertyEdit.originalValue = property->value;
        context.propertyEdit.workingValue = property->value;
        ++context.propertyEdit.revision;
        ++context.revision;
        return true;
    }

    EditorInteractionFrameResult CommitEditorPropertyEdit(EditorInteractionContext& context, const std::string& propertyPath, EditorPropertyValue value)
    {
        EditorInteractionFrameResult result{};
        EditorPropertyDesc* property = FindProperty(context.panels, propertyPath);
        if (!property)
        {
            EditorPropertyChangeRequest rejected{};
            rejected.propertyPath = propertyPath;
            rejected.newValue = std::move(value);
            rejected.status = EditorPropertyChangeStatus::Rejected;
            result.propertyChanges.push_back(rejected);
            context.pendingPropertyChanges.push_back(rejected);
            PushMessage(result, "property commit rejected: missing path");
            return result;
        }
        if (!IsWritableProperty(*property))
        {
            EditorPropertyChangeRequest rejected{};
            rejected.propertyPath = propertyPath;
            rejected.oldValue = property->value;
            rejected.newValue = std::move(value);
            rejected.status = EditorPropertyChangeStatus::Rejected;
            result.propertyChanges.push_back(rejected);
            context.pendingPropertyChanges.push_back(rejected);
            PushMessage(result, "property commit rejected: readonly property");
            return result;
        }

        EditorPropertyChangeRequest request{};
        request.propertyPath = propertyPath;
        request.oldValue = property->value;
        request.newValue = std::move(value);
        request.status = EditorPropertyChangeStatus::Accepted;
        request.requiresSceneDirty = ValuesDiffer(request.oldValue, request.newValue) && HasFlag(property->flags, EditorPropertyFlag::Serialized);
        request.requiresProxyRebuild = HasFlag(property->flags, EditorPropertyFlag::RequiresRebuild);
        request.undoable = request.requiresSceneDirty;
        request.revision = ++context.revision;

        property->value = request.newValue;
        property->flags = property->flags | EditorPropertyFlag::Dirty;
        ++context.panels.revision;
        RebuildPanelSearch(context);

        context.propertyEdit.editing = false;
        ++context.propertyEdit.revision;
        context.pendingPropertyChanges.push_back(request);
        result.propertyChanges.push_back(request);
        result.consumed = true;
        result.sceneDirty = request.requiresSceneDirty;
        result.modelDirty = true;
        PushMessage(result, std::string("property committed ") + propertyPath);
        return result;
    }

    bool CancelEditorPropertyEdit(EditorInteractionContext& context)
    {
        if (!context.propertyEdit.editing)
        {
            return false;
        }
        context.propertyEdit.editing = false;
        context.propertyEdit.propertyPath.clear();
        ++context.propertyEdit.revision;
        ++context.revision;
        return true;
    }

    EditorInteractionFrameResult SelectEditorHierarchyItem(EditorInteractionContext& context, const std::string& stableId, EditorSelectionChangeReason reason)
    {
        EditorInteractionFrameResult result{};
        const EditorHierarchyNode* node = FindHierarchyNode(context.panels.hierarchy, stableId);
        if (!node)
        {
            PushMessage(result, "hierarchy selection rejected: missing entity");
            return result;
        }

        EditorSelectionChangeRequest request{};
        request.before = CaptureSelection(context.bridge.selection);
        EditorSelectionItem item{};
        item.domain = EditorSelectionDomain::SceneEntity;
        item.entity = node->entity;
        item.stableId = node->stableId;
        item.displayName = node->name;
        SetSelection(context.bridge.selection, item);
        SetHierarchySelected(context.panels.hierarchy, stableId);
        context.panels.inspector.selection = CaptureSelection(context.bridge.selection);
        request.after = CaptureSelection(context.bridge.selection);
        request.reason = reason;
        request.undoable = !IsSameStableSelection(request.before, request.after);
        request.revision = ++context.revision;

        context.pendingSelectionChanges.push_back(request);
        result.selectionChanges.push_back(request);
        result.consumed = true;
        result.modelDirty = true;
        PushMessage(result, std::string("selected hierarchy item ") + stableId);
        return result;
    }

    EditorInteractionFrameResult SelectEditorAssetItem(EditorInteractionContext& context, const std::string& stableId, EditorSelectionChangeReason reason)
    {
        EditorInteractionFrameResult result{};
        const EditorAssetItem* asset = FindAsset(context.panels.assets, stableId);
        if (!asset)
        {
            PushMessage(result, "asset selection rejected: missing asset");
            return result;
        }

        EditorSelectionChangeRequest request{};
        request.before = CaptureSelection(context.bridge.selection);
        EditorSelectionItem item{};
        item.domain = EditorSelectionDomain::Asset;
        item.stableId = asset->guid;
        item.displayName = asset->name;
        SetSelection(context.bridge.selection, item);
        request.after = CaptureSelection(context.bridge.selection);
        request.reason = reason;
        request.undoable = !IsSameStableSelection(request.before, request.after);
        request.revision = ++context.revision;

        for (std::size_t i = 0; i < context.panels.assets.items.size(); ++i)
        {
            if (context.panels.assets.items[i].guid == asset->guid)
            {
                context.panels.assets.selectedIndex = i;
                break;
            }
        }
        ++context.panels.assets.revision;

        context.pendingSelectionChanges.push_back(request);
        result.selectionChanges.push_back(request);
        result.consumed = true;
        result.modelDirty = true;
        PushMessage(result, std::string("selected asset ") + asset->name);
        return result;
    }

    EditorInteractionFrameResult InvokeEditorCommand(EditorInteractionContext& context, CommandId command, CommandSource source)
    {
        EditorInteractionFrameResult result{};
        const CommandDescriptor* descriptor = context.bridge.commands.Find(command);
        if (!descriptor)
        {
            PushMessage(result, std::string("command rejected: unknown ") + ToString(command));
            return result;
        }
        if (descriptor->requiresSelection && context.bridge.selection.items.empty())
        {
            PushMessage(result, std::string("command rejected: selection required ") + descriptor->displayName);
            return result;
        }
        AppendCommand(context, result, command, source);
        PushMessage(result, std::string("command invoked ") + descriptor->displayName);
        result.sceneDirty = (command == CommandId::NewEntity || command == CommandId::NewMesh || command == CommandId::NewCamera || command == CommandId::NewLight || command == CommandId::DeleteSelection || command == CommandId::DuplicateSelection || command == CommandId::RenameSelection);
        return result;
    }

    EditorInteractionDiagnostics ValidateEditorInteractionContext(const EditorInteractionContext& context)
    {
        EditorInteractionDiagnostics diagnostics{};
        diagnostics.pendingCommandCount = context.pendingCommands.size();
        diagnostics.pendingSelectionChangeCount = context.pendingSelectionChanges.size();
        diagnostics.pendingPropertyChangeCount = context.pendingPropertyChanges.size();
        diagnostics.paletteResultCount = context.palette.results.size();
        diagnostics.paletteOpen = context.palette.open;
        diagnostics.propertyEditing = context.propertyEdit.editing;
        diagnostics.selectionValid = ValidateEditorSelection(context.bridge.selection).ok;
        for (const EditorPropertyChangeRequest& request : context.pendingPropertyChanges)
        {
            if (request.status == EditorPropertyChangeStatus::Rejected)
            {
                ++diagnostics.rejectedPropertyChangeCount;
            }
        }
        diagnostics.ok = diagnostics.selectionValid
            && diagnostics.pendingCommandCount >= 1
            && diagnostics.pendingSelectionChangeCount >= 1
            && diagnostics.pendingPropertyChangeCount >= 1
            && diagnostics.paletteResultCount >= 1
            && diagnostics.rejectedPropertyChangeCount == 0;

        std::ostringstream out;
        out << "editor-interaction commands=" << diagnostics.pendingCommandCount
            << " selections=" << diagnostics.pendingSelectionChangeCount
            << " properties=" << diagnostics.pendingPropertyChangeCount
            << " paletteResults=" << diagnostics.paletteResultCount
            << " paletteOpen=" << (diagnostics.paletteOpen ? "true" : "false")
            << " propertyEditing=" << (diagnostics.propertyEditing ? "true" : "false")
            << " rejected=" << diagnostics.rejectedPropertyChangeCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string FormatEditorInteractionEvent(const EditorInteractionEvent& event)
    {
        std::ostringstream out;
        out << ToString(event.kind)
            << " command=" << ToString(event.command)
            << " source=" << ToString(event.commandSource)
            << " text=" << event.text
            << " stableId=" << event.stableId
            << " property=" << event.propertyPath;
        return out.str();
    }

    std::string FormatEditorPropertyChangeRequest(const EditorPropertyChangeRequest& request)
    {
        std::ostringstream out;
        out << request.propertyPath
            << " status=" << ToString(request.status)
            << " old=" << request.oldValue.text
            << " new=" << request.newValue.text
            << " sceneDirty=" << (request.requiresSceneDirty ? "true" : "false")
            << " proxyRebuild=" << (request.requiresProxyRebuild ? "true" : "false")
            << " undoable=" << (request.undoable ? "true" : "false");
        return out.str();
    }

    std::string FormatEditorSelectionChangeRequest(const EditorSelectionChangeRequest& request)
    {
        std::ostringstream out;
        out << ToString(request.reason)
            << " before=" << request.before.items.size()
            << " after=" << request.after.items.size()
            << " active=" << request.after.active.displayName
            << " undoable=" << (request.undoable ? "true" : "false");
        return out.str();
    }

    std::string FormatEditorInteractionPaletteState(const EditorInteractionPaletteState& palette)
    {
        std::ostringstream out;
        out << "palette open=" << (palette.open ? "true" : "false")
            << " query=" << palette.query
            << " results=" << palette.results.size()
            << " selected=" << palette.selectedIndex;
        if (!palette.results.empty() && palette.selectedIndex < palette.results.size())
        {
            out << " title=" << palette.results[palette.selectedIndex].title;
        }
        return out.str();
    }

    std::string FormatEditorInteractionDiagnostics(const EditorInteractionDiagnostics& diagnostics)
    {
        return diagnostics.summary;
    }
}
