#include <AK/EditorUI/EditorUXRuntime.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr i32 MenuWidth = 238;
        constexpr i32 MenuRowHeight = 22;
        constexpr i32 MenuPadding = 6;
        constexpr i32 PopupPanelWidth = 242;

        bool RectUsable(EditorRect rect)
        {
            return rect.width > 0 && rect.height > 0;
        }

        CommandId CommandFromUXItemId(const std::string& id)
        {
            if (id == "create.empty" || id == "create.emptyChild" || id == "create.mesh" || id == "create.cube" || id == "create.sphere" || id == "create.capsule" || id == "create.plane")
            {
                return CommandId::NewEntity;
            }
            if (id == "create.camera")
            {
                return CommandId::NewCamera;
            }
            if (id == "create.light")
            {
                return CommandId::NewLight;
            }
            if (id == "rename")
            {
                return CommandId::RenameSelection;
            }
            if (id == "duplicate")
            {
                return CommandId::DuplicateSelection;
            }
            if (id == "delete" || id == "deleteAsset")
            {
                return CommandId::DeleteSelection;
            }
            if (id == "reimport")
            {
                return CommandId::RescanAssets;
            }
            if (id == "grid.snap" || id == "scene.gridSnap" || id == "grid.visible")
            {
                return CommandId::ToggleGridSnap;
            }
            if (id == "component.mesh" || id == "component.camera" || id == "component.light" || id == "component.bounds")
            {
                return CommandId::NewEntity;
            }
            return CommandId::Count;
        }

        EditorRect ClampSurfaceRect(EditorRect rect, i32 viewportWidth, i32 viewportHeight)
        {
            if (viewportWidth <= 0 || viewportHeight <= 0)
            {
                return rect;
            }

            if (rect.x + rect.width > viewportWidth - 8)
            {
                rect.x = std::max<i32>(8, viewportWidth - rect.width - 8);
            }
            if (rect.y + rect.height > viewportHeight - 28)
            {
                rect.y = std::max<i32>(28, viewportHeight - rect.height - 8);
            }
            rect.x = std::max<i32>(4, rect.x);
            rect.y = std::max<i32>(24, rect.y);
            return rect;
        }

        void AppendMenuItemRecursive(EditorUXPopupSurface& surface, const EditorContextMenuItem& item, i32& y, i32 depth)
        {
            if (item.separatorBefore)
            {
                y += 5;
            }

            EditorUXPopupItemRuntime runtime{};
            runtime.id = item.id;
            runtime.label = depth > 0 ? std::string(static_cast<std::size_t>(depth * 2), ' ') + item.label : item.label;
            runtime.icon = item.icon;
            runtime.enabled = item.enabled;
            runtime.checked = item.checked;
            runtime.destructive = item.destructive;
            runtime.separatorBefore = item.separatorBefore;
            runtime.hasSubmenu = !item.children.empty();
            runtime.command = CommandFromUXItemId(item.id);
            runtime.rect = {surface.rect.x + MenuPadding, y, surface.rect.width - MenuPadding * 2, MenuRowHeight};
            surface.items.push_back(std::move(runtime));
            y += MenuRowHeight;

            if (!item.children.empty())
            {
                for (const EditorContextMenuItem& child : item.children)
                {
                    AppendMenuItemRecursive(surface, child, y, depth + 1);
                }
            }
        }

        i32 EstimateMenuHeight(const EditorContextMenuModel& menu)
        {
            i32 height = MenuPadding * 2;
            const auto addHeight = [&height](const auto& self, const std::vector<EditorContextMenuItem>& items) -> void
            {
                for (const EditorContextMenuItem& item : items)
                {
                    if (item.separatorBefore)
                    {
                        height += 5;
                    }
                    height += MenuRowHeight;
                    self(self, item.children);
                }
            };
            addHeight(addHeight, menu.items);
            return height;
        }

        EditorUXPopupSurface BuildMenuSurface(const EditorPopupWorkflowDesc& workflow, i32 viewportWidth, i32 viewportHeight)
        {
            EditorUXPopupSurface surface{};
            surface.id = workflow.menu.id.empty() ? workflow.id : workflow.menu.id;
            surface.title = workflow.label;
            surface.anchorKind = workflow.anchorKind;
            surface.anchor = workflow.anchor;
            surface.open = workflow.menu.open;
            surface.popupPanel = false;
            surface.rect = {workflow.anchor.x, workflow.anchor.y + std::max<i32>(1, workflow.anchor.height), MenuWidth, EstimateMenuHeight(workflow.menu)};
            surface.rect = ClampSurfaceRect(surface.rect, viewportWidth, viewportHeight);

            i32 y = surface.rect.y + MenuPadding;
            for (const EditorContextMenuItem& item : workflow.menu.items)
            {
                AppendMenuItemRecursive(surface, item, y, 0);
            }
            return surface;
        }

        EditorUXPopupSurface BuildPopupPanelSurface(const EditorPopupWorkflowDesc& workflow, i32 viewportWidth, i32 viewportHeight)
        {
            EditorUXPopupSurface surface{};
            surface.id = workflow.popup.id.empty() ? workflow.id : workflow.popup.id;
            surface.title = workflow.popup.title.empty() ? workflow.label : workflow.popup.title;
            surface.anchorKind = workflow.anchorKind;
            surface.anchor = workflow.anchor;
            surface.open = workflow.popup.open;
            surface.popupPanel = true;
            const i32 rowCount = static_cast<i32>(workflow.popup.rows.size());
            surface.rect = {workflow.anchor.x, workflow.anchor.y + std::max<i32>(1, workflow.anchor.height), PopupPanelWidth, std::max<i32>(68, MenuPadding * 2 + 24 + rowCount * MenuRowHeight)};
            surface.rect = ClampSurfaceRect(surface.rect, viewportWidth, viewportHeight);

            i32 y = surface.rect.y + MenuPadding + 24;
            for (const EditorWidgetDesc& row : workflow.popup.rows)
            {
                EditorUXPopupItemRuntime item{};
                item.id = row.id;
                item.label = row.label;
                item.icon = row.icon;
                item.enabled = !HasEditorWidgetState(row.state, EditorWidgetState::Disabled);
                item.checked = HasEditorWidgetState(row.state, EditorWidgetState::Checked);
                item.command = CommandFromUXItemId(row.id);
                item.rect = {surface.rect.x + MenuPadding, y, surface.rect.width - MenuPadding * 2, MenuRowHeight};
                surface.items.push_back(std::move(item));
                y += MenuRowHeight;
            }
            return surface;
        }
    }

    const char* ToString(EditorUXPopupCloseReason reason)
    {
        switch (reason)
        {
            case EditorUXPopupCloseReason::None: return "none";
            case EditorUXPopupCloseReason::Escape: return "escape";
            case EditorUXPopupCloseReason::OutsideClick: return "outside-click";
            case EditorUXPopupCloseReason::CommandAccepted: return "command-accepted";
            case EditorUXPopupCloseReason::Replaced: return "replaced";
        }
        return "unknown";
    }

    EditorRect MakeEditorRect(i32 x, i32 y, i32 width, i32 height)
    {
        return {x, y, width, height};
    }

    bool EditorRectContains(EditorRect rect, i32 x, i32 y)
    {
        return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
    }

    EditorUXPopupSurface BuildEditorUXPopupSurface(const EditorPopupWorkflowDesc& workflow, i32 viewportWidth, i32 viewportHeight)
    {
        if (workflow.menu.open && !workflow.menu.items.empty())
        {
            return BuildMenuSurface(workflow, viewportWidth, viewportHeight);
        }
        if (workflow.popup.open && !workflow.popup.rows.empty())
        {
            return BuildPopupPanelSurface(workflow, viewportWidth, viewportHeight);
        }
        EditorUXPopupSurface surface{};
        surface.id = workflow.id;
        surface.title = workflow.label;
        surface.anchorKind = workflow.anchorKind;
        surface.anchor = workflow.anchor;
        surface.rect = workflow.anchor;
        return surface;
    }

    bool OpenEditorUXPopup(EditorUXRuntime& runtime, EditorPopupWorkflowDesc workflow, i32 viewportWidth, i32 viewportHeight)
    {
        if (runtime.popupOpen)
        {
            runtime.lastCloseReason = EditorUXPopupCloseReason::Replaced;
        }
        runtime.popupOpen = true;
        runtime.activeWorkflow = std::move(workflow);
        runtime.surface = BuildEditorUXPopupSurface(runtime.activeWorkflow, viewportWidth, viewportHeight);
        runtime.hoveredItemId.clear();
        runtime.lastAcceptedItemId.clear();
        ++runtime.revision;
        return runtime.surface.open && RectUsable(runtime.surface.rect);
    }

    bool CloseEditorUXPopup(EditorUXRuntime& runtime, EditorUXPopupCloseReason reason)
    {
        if (!runtime.popupOpen)
        {
            return false;
        }
        runtime.popupOpen = false;
        runtime.activeWorkflow = {};
        runtime.surface = {};
        runtime.hoveredItemId.clear();
        runtime.lastCloseReason = reason;
        ++runtime.revision;
        return true;
    }

    EditorUXHitResult HitTestEditorUXPopup(EditorUXRuntime& runtime, i32 x, i32 y)
    {
        EditorUXHitResult result{};
        if (!runtime.popupOpen || !runtime.surface.open)
        {
            runtime.hoveredItemId.clear();
            return result;
        }

        result.insidePopup = EditorRectContains(runtime.surface.rect, x, y);
        if (!result.insidePopup)
        {
            runtime.hoveredItemId.clear();
            return result;
        }

        for (const EditorUXPopupItemRuntime& item : runtime.surface.items)
        {
            if (EditorRectContains(item.rect, x, y))
            {
                result.insideItem = true;
                result.itemId = item.id;
                result.command = item.command;
                result.enabled = item.enabled;
                runtime.hoveredItemId = item.id;
                return result;
            }
        }
        runtime.hoveredItemId.clear();
        return result;
    }

    EditorUXActivationResult ActivateEditorUXPopupItem(EditorUXRuntime& runtime, i32 x, i32 y)
    {
        EditorUXActivationResult result{};
        result.invocation = {CommandId::Count, CommandSource::Menu};
        if (!runtime.popupOpen)
        {
            return result;
        }

        const EditorUXHitResult hit = HitTestEditorUXPopup(runtime, x, y);
        if (!hit.insidePopup)
        {
            result.consumed = CloseEditorUXPopup(runtime, EditorUXPopupCloseReason::OutsideClick);
            result.closePopup = result.consumed;
            return result;
        }

        result.consumed = true;
        if (!hit.insideItem || !hit.enabled)
        {
            return result;
        }

        result.itemId = hit.itemId;
        runtime.lastAcceptedItemId = hit.itemId;
        if (hit.command != CommandId::Count)
        {
            result.invocation = {hit.command, CommandSource::Menu};
            result.message = "popup command: " + hit.itemId;
        }
        else
        {
            result.message = "popup item: " + hit.itemId;
        }
        CloseEditorUXPopup(runtime, EditorUXPopupCloseReason::CommandAccepted);
        result.closePopup = true;
        return result;
    }

    EditorUXRuntimeDiagnostics ValidateEditorUXRuntime(const EditorUXRuntime& runtime)
    {
        EditorUXRuntimeDiagnostics diagnostics{};
        diagnostics.popupOpen = runtime.popupOpen;
        diagnostics.surfaceItemCount = runtime.surface.items.size();
        if (runtime.popupOpen && (!runtime.surface.open || !RectUsable(runtime.surface.rect)))
        {
            ++diagnostics.invalidRectCount;
        }
        for (const EditorUXPopupItemRuntime& item : runtime.surface.items)
        {
            if (!RectUsable(item.rect))
            {
                ++diagnostics.invalidRectCount;
            }
            if (item.command != CommandId::Count)
            {
                ++diagnostics.commandItemCount;
            }
            if (!item.enabled)
            {
                ++diagnostics.disabledItemCount;
            }
            if (item.destructive)
            {
                ++diagnostics.destructiveItemCount;
            }
        }
        diagnostics.ok = !runtime.popupOpen || (diagnostics.surfaceItemCount > 0 && diagnostics.invalidRectCount == 0);
        diagnostics.summary = FormatEditorUXRuntimeDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorUXPopupSurface(const EditorUXPopupSurface& surface)
    {
        std::ostringstream out;
        out << surface.id << " title=" << surface.title
            << " anchor=" << ToString(surface.anchorKind)
            << " rect=" << FormatEditorRect(surface.rect)
            << " items=" << surface.items.size()
            << " panel=" << (surface.popupPanel ? "true" : "false")
            << " open=" << (surface.open ? "true" : "false");
        return out.str();
    }

    std::string FormatEditorUXRuntimeDiagnostics(const EditorUXRuntimeDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-ux-runtime open=" << (diagnostics.popupOpen ? "true" : "false")
            << " items=" << diagnostics.surfaceItemCount
            << " commands=" << diagnostics.commandItemCount
            << " disabled=" << diagnostics.disabledItemCount
            << " destructive=" << diagnostics.destructiveItemCount
            << " invalidRects=" << diagnostics.invalidRectCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        return out.str();
    }
}
