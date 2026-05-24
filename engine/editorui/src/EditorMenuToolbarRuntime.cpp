#include <AK/EditorUI/EditorMenuToolbarRuntime.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr i32 MenuPaddingX = 10;
        constexpr i32 MenuRootExtraWidth = 22;
        constexpr i32 MenuRootMinWidth = 42;
        constexpr i32 MenuPopupWidth = 278;
        constexpr i32 MenuPopupRowHeight = 22;
        constexpr i32 MenuPopupPadding = 6;
        constexpr i32 ToolbarButtonWidth = 64;
        constexpr i32 ToolbarButtonGap = 4;

        bool Contains(EditorRect rect, i32 x, i32 y)
        {
            return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
        }

        bool RectUsable(EditorRect rect)
        {
            return rect.width > 0 && rect.height > 0;
        }

        std::string RootFromPath(const std::string& path)
        {
            const std::size_t slash = path.find('/');
            return slash == std::string::npos ? path : path.substr(0, slash);
        }

        std::vector<std::string> SplitPathTail(const std::string& path)
        {
            std::vector<std::string> result;
            std::size_t begin = path.find('/');
            if (begin == std::string::npos)
            {
                return result;
            }
            ++begin;
            while (begin < path.size())
            {
                const std::size_t slash = path.find('/', begin);
                result.push_back(path.substr(begin, slash == std::string::npos ? std::string::npos : slash - begin));
                if (slash == std::string::npos)
                {
                    break;
                }
                begin = slash + 1;
            }
            return result;
        }

        std::string MenuItemId(CommandId command)
        {
            return std::string("menu.command.") + ToString(command);
        }

        std::string ToolbarItemId(CommandId command)
        {
            return std::string("toolbar.command.") + ToString(command);
        }

        const EditorCommandStateEntry* CommandStateOrNull(const EditorCommandStateCache& cache, CommandId command)
        {
            return FindEditorCommandState(cache, command);
        }

        std::string ShortcutTextForCommand(const EditorShortcutProfile& shortcuts, CommandId command)
        {
            for (const EditorShortcutBindingDesc& binding : shortcuts.bindings)
            {
                if (binding.enabled && binding.command == command)
                {
                    return FormatEditorShortcutChord(binding.chord);
                }
            }
            return {};
        }

        EditorIconKind IconForCommand(CommandId command)
        {
            switch (command)
            {
                case CommandId::NewMesh: return EditorIconKind::Mesh;
                case CommandId::NewCamera: return EditorIconKind::Camera;
                case CommandId::NewLight: return EditorIconKind::Light;
                case CommandId::SaveScene: return EditorIconKind::Scene;
                case CommandId::RescanAssets: return EditorIconKind::Folder;
                case CommandId::DeleteSelection: return EditorIconKind::Error;
                case CommandId::ToggleGridSnap: return EditorIconKind::Bounds;
                case CommandId::FocusSelection: return EditorIconKind::Search;
                default: return EditorIconKind::Entity;
            }
        }

        void ClampPopup(EditorMenuToolbarPopupSurface& popup, i32 viewportWidth, i32 viewportHeight)
        {
            if (viewportWidth <= 0 || viewportHeight <= 0)
            {
                return;
            }
            if (popup.rect.x + popup.rect.width > viewportWidth - 8)
            {
                popup.rect.x = std::max<i32>(4, viewportWidth - popup.rect.width - 8);
            }
            if (popup.rect.y + popup.rect.height > viewportHeight - 8)
            {
                popup.rect.y = std::max<i32>(24, viewportHeight - popup.rect.height - 8);
            }
            popup.rect.x = std::max<i32>(4, popup.rect.x);
            popup.rect.y = std::max<i32>(24, popup.rect.y);
        }

        void AddPopupCommand(EditorMenuToolbarPopupSurface& popup,
                             const EditorMenuItem& item,
                             const EditorCommandStateCache& commandState,
                             const EditorShortcutProfile& shortcuts,
                             i32& y,
                             bool separatorBefore,
                             i32 depth,
                             const std::string& pathLabel)
        {
            if (separatorBefore)
            {
                y += 5;
            }

            EditorMenuToolbarCommandItemRuntime runtime{};
            runtime.id = MenuItemId(item.command);
            runtime.label = pathLabel.empty() ? item.label : pathLabel;
            runtime.shortcutText = ShortcutTextForCommand(shortcuts, item.command);
            runtime.icon = IconForCommand(item.command);
            runtime.command = item.command;
            runtime.separatorBefore = separatorBefore;
            runtime.depth = depth;
            runtime.destructive = item.destructive;
            runtime.rect = {popup.rect.x + MenuPopupPadding, y, popup.rect.width - MenuPopupPadding * 2, MenuPopupRowHeight};

            if (const EditorCommandStateEntry* state = CommandStateOrNull(commandState, item.command))
            {
                runtime.enabled = item.enabled && state->enabled && state->visible;
                runtime.checked = item.checked || state->checked;
                runtime.destructive = runtime.destructive || state->destructive;
            }
            else
            {
                runtime.enabled = false;
            }

            popup.items.push_back(std::move(runtime));
            y += MenuPopupRowHeight;
        }

        void BuildPopupItemsForRoot(EditorMenuToolbarPopupSurface& popup,
                                    const EditorMenuModel& menu,
                                    const EditorCommandStateCache& commandState,
                                    const EditorShortcutProfile& shortcuts,
                                    const std::string& root)
        {
            bool separatorBefore = false;
            i32 y = popup.rect.y + MenuPopupPadding;
            for (const EditorMenuItem& item : menu.items)
            {
                if (RootFromPath(item.path) != root)
                {
                    continue;
                }
                if (item.kind == EditorMenuItemKind::Separator)
                {
                    separatorBefore = true;
                    continue;
                }
                if (item.kind != EditorMenuItemKind::Command)
                {
                    continue;
                }

                const std::vector<std::string> tail = SplitPathTail(item.path);
                i32 depth = 0;
                std::string label = item.label;
                if (tail.size() > 1)
                {
                    depth = static_cast<i32>(tail.size()) - 1;
                    label = tail.empty() ? item.label : tail.back();
                }
                AddPopupCommand(popup, item, commandState, shortcuts, y, separatorBefore, depth, label);
                separatorBefore = false;
            }
        }

        usize CountMenuItemsForRoot(const EditorMenuModel& menu, const std::string& root)
        {
            usize count = 0;
            for (const EditorMenuItem& item : menu.items)
            {
                if (RootFromPath(item.path) == root && item.kind == EditorMenuItemKind::Command)
                {
                    ++count;
                }
            }
            return count;
        }

        std::vector<std::string> OrderedMenuRoots(const EditorMenuModel& menu)
        {
            std::vector<std::string> roots;
            for (const EditorMenuItem& item : menu.items)
            {
                const std::string root = RootFromPath(item.path);
                if (!root.empty() && std::find(roots.begin(), roots.end(), root) == roots.end())
                {
                    roots.push_back(root);
                }
            }
            return roots;
        }

        EditorMenuToolbarButtonRuntime BuildToolbarButton(const EditorToolbarItem& item,
                                                          const EditorCommandStateCache& commandState,
                                                          EditorRect rect)
        {
            EditorMenuToolbarButtonRuntime button{};
            button.id = ToolbarItemId(item.command);
            button.label = item.label.empty() ? ToString(item.command) : item.label;
            button.tooltip = item.tooltip;
            button.rect = rect;
            button.command = item.command;
            button.enabled = item.enabled;
            button.checked = item.active;
            button.destructive = item.command == CommandId::DeleteSelection;
            if (const EditorCommandStateEntry* state = CommandStateOrNull(commandState, item.command))
            {
                button.enabled = button.enabled && state->enabled && state->visible;
                button.checked = button.checked || state->checked;
                button.destructive = button.destructive || state->destructive;
                if (button.tooltip.empty())
                {
                    button.tooltip = state->tooltip;
                }
            }
            else
            {
                button.enabled = false;
            }
            return button;
        }
    }

    const char* ToString(EditorMenuToolbarHitKind kind)
    {
        switch (kind)
        {
            case EditorMenuToolbarHitKind::None: return "none";
            case EditorMenuToolbarHitKind::Outside: return "outside";
            case EditorMenuToolbarHitKind::MenuRoot: return "menu-root";
            case EditorMenuToolbarHitKind::MenuPopupItem: return "menu-popup-item";
            case EditorMenuToolbarHitKind::ToolbarButton: return "toolbar-button";
            case EditorMenuToolbarHitKind::ToolbarSearchBox: return "toolbar-search-box";
            case EditorMenuToolbarHitKind::ToolbarEmpty: return "toolbar-empty";
            default: return "unknown";
        }
    }

    EditorMenuToolbarRuntimeState MakeDefaultEditorMenuToolbarRuntimeState()
    {
        EditorMenuToolbarRuntimeState runtime{};
        runtime.lastInvocation.id = CommandId::Count;
        runtime.lastInvocation.source = CommandSource::Programmatic;
        runtime.revision = 1;
        return runtime;
    }

    EditorMenuToolbarSurface BuildEditorMenuToolbarSurface(const EditorMenuModel& menu,
                                                           const EditorToolbarModel& toolbar,
                                                           const EditorCommandStateCache& commandState,
                                                           const EditorShortcutProfile& shortcuts,
                                                           const EditorFrameLayout& frame,
                                                           std::string activeMenuId,
                                                           i32 viewportWidth,
                                                           i32 viewportHeight)
    {
        EditorMenuToolbarSurface surface{};
        surface.menuBar = frame.menuBar;
        surface.toolbar = frame.toolbar;
        surface.searchBox = {frame.toolbar.x + std::max<i32>(0, frame.toolbar.width - 310), frame.toolbar.y + 7, 296, std::max<i32>(18, frame.toolbar.height - 14)};

        i32 menuX = frame.menuBar.x + MenuPaddingX;
        const std::vector<std::string> roots = OrderedMenuRoots(menu);
        for (const std::string& root : roots)
        {
            EditorMenuToolbarRootRuntime item{};
            item.id = std::string("menu.root.") + root;
            item.label = root;
            item.itemCount = CountMenuItemsForRoot(menu, root);
            const i32 width = std::max<i32>(MenuRootMinWidth, static_cast<i32>(root.size()) * 8 + MenuRootExtraWidth);
            item.rect = {menuX, frame.menuBar.y, width, frame.menuBar.height};
            item.open = activeMenuId == item.id;
            surface.roots.push_back(std::move(item));
            menuX += width;
        }

        const i32 buttonY = frame.toolbar.y + 7;
        const i32 buttonH = std::max<i32>(18, frame.toolbar.height - 14);
        i32 x = frame.toolbar.x + 8;
        for (const EditorToolbarItem& item : toolbar.items)
        {
            if (item.kind != EditorToolbarItemKind::Command && item.kind != EditorToolbarItemKind::Toggle)
            {
                if (item.kind == EditorToolbarItemKind::Separator)
                {
                    x += ToolbarButtonGap + 6;
                }
                continue;
            }
            const i32 labelWidth = item.label == "Create" ? 76 : (item.label == "Camera" ? 70 : ToolbarButtonWidth);
            surface.toolbarButtons.push_back(BuildToolbarButton(item, commandState, {x, buttonY, labelWidth, buttonH}));
            x += labelWidth + ToolbarButtonGap;
        }

        if (!activeMenuId.empty())
        {
            const EditorMenuToolbarRootRuntime* activeRoot = nullptr;
            for (const EditorMenuToolbarRootRuntime& root : surface.roots)
            {
                if (root.id == activeMenuId)
                {
                    activeRoot = &root;
                    break;
                }
            }
            if (activeRoot)
            {
                surface.popup.id = activeRoot->id + ".popup";
                surface.popup.title = activeRoot->label;
                surface.popup.anchor = activeRoot->rect;
                surface.popup.open = true;
                surface.popup.rect = {activeRoot->rect.x, activeRoot->rect.y + std::max<i32>(1, activeRoot->rect.height), MenuPopupWidth, MenuPopupPadding * 2 + std::max<i32>(1, static_cast<i32>(activeRoot->itemCount)) * MenuPopupRowHeight};
                ClampPopup(surface.popup, viewportWidth, viewportHeight);
                BuildPopupItemsForRoot(surface.popup, menu, commandState, shortcuts, activeRoot->label);
                surface.popup.rect.height = MenuPopupPadding * 2;
                if (!surface.popup.items.empty())
                {
                    const EditorMenuToolbarCommandItemRuntime& last = surface.popup.items.back();
                    surface.popup.rect.height = std::max<i32>(MenuPopupPadding * 2, last.rect.y + last.rect.height - surface.popup.rect.y + MenuPopupPadding);
                }
                ClampPopup(surface.popup, viewportWidth, viewportHeight);
            }
        }
        return surface;
    }

    EditorMenuToolbarHitTestResult HitTestEditorMenuToolbarSurface(const EditorMenuToolbarSurface& surface, i32 x, i32 y)
    {
        EditorMenuToolbarHitTestResult hit{};
        if (surface.popup.open)
        {
            if (Contains(surface.popup.rect, x, y))
            {
                for (usize index = 0; index < surface.popup.items.size(); ++index)
                {
                    const EditorMenuToolbarCommandItemRuntime& item = surface.popup.items[index];
                    if (Contains(item.rect, x, y))
                    {
                        hit.kind = EditorMenuToolbarHitKind::MenuPopupItem;
                        hit.index = index;
                        hit.id = item.id;
                        hit.label = item.label;
                        hit.command = item.command;
                        hit.enabled = item.enabled;
                        hit.destructive = item.destructive;
                        return hit;
                    }
                }
                hit.kind = EditorMenuToolbarHitKind::MenuPopupItem;
                hit.id = surface.popup.id;
                hit.label = surface.popup.title;
                return hit;
            }
        }

        for (usize index = 0; index < surface.roots.size(); ++index)
        {
            const EditorMenuToolbarRootRuntime& root = surface.roots[index];
            if (Contains(root.rect, x, y))
            {
                hit.kind = EditorMenuToolbarHitKind::MenuRoot;
                hit.index = index;
                hit.id = root.id;
                hit.label = root.label;
                hit.enabled = root.itemCount > 0;
                return hit;
            }
        }

        if (Contains(surface.searchBox, x, y))
        {
            hit.kind = EditorMenuToolbarHitKind::ToolbarSearchBox;
            hit.id = "toolbar.search";
            hit.label = "Search";
            hit.enabled = true;
            return hit;
        }

        for (usize index = 0; index < surface.toolbarButtons.size(); ++index)
        {
            const EditorMenuToolbarButtonRuntime& button = surface.toolbarButtons[index];
            if (Contains(button.rect, x, y))
            {
                hit.kind = EditorMenuToolbarHitKind::ToolbarButton;
                hit.index = index;
                hit.id = button.id;
                hit.label = button.label;
                hit.command = button.command;
                hit.enabled = button.enabled;
                hit.destructive = button.destructive;
                return hit;
            }
        }

        if (Contains(surface.toolbar, x, y))
        {
            hit.kind = EditorMenuToolbarHitKind::ToolbarEmpty;
            hit.enabled = true;
            return hit;
        }
        hit.kind = EditorMenuToolbarHitKind::Outside;
        return hit;
    }

    void RefreshEditorMenuToolbarRuntimeSurface(EditorMenuToolbarRuntimeState& runtime,
                                                const EditorMenuModel& menu,
                                                const EditorToolbarModel& toolbar,
                                                const EditorCommandStateCache& commandState,
                                                const EditorShortcutProfile& shortcuts,
                                                const EditorFrameLayout& frame,
                                                i32 viewportWidth,
                                                i32 viewportHeight)
    {
        runtime.surface = BuildEditorMenuToolbarSurface(menu, toolbar, commandState, shortcuts, frame, runtime.activeMenuId, viewportWidth, viewportHeight);
        runtime.menuOpen = runtime.surface.popup.open;
        runtime.mouseCapture = runtime.menuOpen;
        runtime.keyboardCapture = runtime.menuOpen;
    }

    bool CloseEditorMenuToolbarRuntime(EditorMenuToolbarRuntimeState& runtime)
    {
        if (runtime.activeMenuId.empty() && !runtime.menuOpen)
        {
            return false;
        }
        runtime.activeMenuId.clear();
        runtime.menuOpen = false;
        runtime.mouseCapture = false;
        runtime.keyboardCapture = false;
        runtime.surface.popup = {};
        ++runtime.revision;
        return true;
    }

    EditorMenuToolbarRuntimeResult ApplyEditorMenuToolbarRuntimeInput(EditorMenuToolbarRuntimeState& runtime,
                                                                       const EditorMenuToolbarRuntimeInput& input)
    {
        EditorMenuToolbarRuntimeResult result{};
        result.command.id = CommandId::Count;
        result.command.source = CommandSource::Programmatic;
        if (!input.menu || !input.toolbar || !input.commandState || !input.shortcuts)
        {
            result.statusText = "menu/toolbar runtime missing model inputs";
            return result;
        }

        RefreshEditorMenuToolbarRuntimeSurface(runtime, *input.menu, *input.toolbar, *input.commandState, *input.shortcuts, input.frame, input.viewportWidth, input.viewportHeight);

        if (input.keyEscapePressed && runtime.menuOpen)
        {
            result.handled = CloseEditorMenuToolbarRuntime(runtime);
            result.closedMenu = result.handled;
            result.statusText = "Menu closed";
            return result;
        }

        const EditorMenuToolbarHitTestResult hit = HitTestEditorMenuToolbarSurface(runtime.surface, input.mouseX, input.mouseY);
        if (hit.kind != runtime.hover.kind || hit.id != runtime.hover.id)
        {
            runtime.hover = hit;
            result.hoverChanged = true;
        }

        if (!input.mouseLeftPressed)
        {
            return result;
        }

        result.handled = hit.kind != EditorMenuToolbarHitKind::Outside;
        switch (hit.kind)
        {
            case EditorMenuToolbarHitKind::MenuRoot:
            {
                if (!hit.enabled)
                {
                    result.statusText = "Menu has no commands: " + hit.label;
                    return result;
                }
                const bool sameMenu = runtime.activeMenuId == hit.id && runtime.menuOpen;
                if (sameMenu)
                {
                    result.closedMenu = CloseEditorMenuToolbarRuntime(runtime);
                    result.statusText = "Menu closed: " + hit.label;
                    return result;
                }
                runtime.activeMenuId = hit.id;
                ++runtime.revision;
                RefreshEditorMenuToolbarRuntimeSurface(runtime, *input.menu, *input.toolbar, *input.commandState, *input.shortcuts, input.frame, input.viewportWidth, input.viewportHeight);
                result.openedMenu = true;
                result.statusText = "Menu opened: " + hit.label;
                return result;
            }
            case EditorMenuToolbarHitKind::MenuPopupItem:
            {
                if (!hit.enabled || hit.command == CommandId::Count)
                {
                    result.statusText = "Menu item disabled: " + hit.label;
                    return result;
                }
                result.commandQueued = true;
                result.command = {hit.command, CommandSource::Menu};
                runtime.lastInvocation = result.command;
                result.closedMenu = CloseEditorMenuToolbarRuntime(runtime);
                result.statusText = "Menu command: " + hit.label;
                return result;
            }
            case EditorMenuToolbarHitKind::ToolbarButton:
            {
                if (!hit.enabled || hit.command == CommandId::Count)
                {
                    result.statusText = "Toolbar command disabled: " + hit.label;
                    return result;
                }
                CloseEditorMenuToolbarRuntime(runtime);
                result.commandQueued = true;
                result.command = {hit.command, CommandSource::Toolbar};
                runtime.lastInvocation = result.command;
                result.statusText = "Toolbar command: " + hit.label;
                return result;
            }
            case EditorMenuToolbarHitKind::ToolbarSearchBox:
            {
                CloseEditorMenuToolbarRuntime(runtime);
                result.openCommandPalette = true;
                result.statusText = "Open command palette";
                return result;
            }
            case EditorMenuToolbarHitKind::Outside:
            {
                if (runtime.menuOpen)
                {
                    result.handled = CloseEditorMenuToolbarRuntime(runtime);
                    result.closedMenu = result.handled;
                    result.statusText = "Menu closed";
                }
                return result;
            }
            case EditorMenuToolbarHitKind::ToolbarEmpty:
            case EditorMenuToolbarHitKind::None:
            default:
                return result;
        }
    }

    bool ValidateEditorMenuToolbarSurface(const EditorMenuToolbarSurface& surface, i32 viewportWidth, i32 viewportHeight)
    {
        if (!RectUsable(surface.menuBar) || !RectUsable(surface.toolbar) || !RectUsable(surface.searchBox))
        {
            return false;
        }
        if (surface.roots.size() < 5 || surface.toolbarButtons.size() < 5)
        {
            return false;
        }
        for (const EditorMenuToolbarRootRuntime& root : surface.roots)
        {
            if (root.id.empty() || root.label.empty() || !RectUsable(root.rect))
            {
                return false;
            }
        }
        for (const EditorMenuToolbarButtonRuntime& button : surface.toolbarButtons)
        {
            if (button.id.empty() || button.label.empty() || !RectUsable(button.rect))
            {
                return false;
            }
        }
        if (surface.popup.open)
        {
            if (!RectUsable(surface.popup.rect) || surface.popup.items.empty())
            {
                return false;
            }
            if (surface.popup.rect.x < 0 || surface.popup.rect.y < 0 || surface.popup.rect.x + surface.popup.rect.width > viewportWidth + 1 || surface.popup.rect.y + surface.popup.rect.height > viewportHeight + 1)
            {
                return false;
            }
        }
        return true;
    }

    std::string FormatEditorMenuToolbarHitTestResult(const EditorMenuToolbarHitTestResult& hit)
    {
        std::ostringstream out;
        out << "menu-toolbar-hit kind=" << ToString(hit.kind)
            << " id='" << hit.id << "' label='" << hit.label << "' command=" << ToString(hit.command)
            << " enabled=" << (hit.enabled ? 1 : 0);
        return out.str();
    }

    std::string FormatEditorMenuToolbarSurface(const EditorMenuToolbarSurface& surface)
    {
        std::ostringstream out;
        out << "editor-menu-toolbar roots=" << surface.roots.size()
            << " buttons=" << surface.toolbarButtons.size()
            << " popup=" << (surface.popup.open ? 1 : 0)
            << " popupItems=" << surface.popup.items.size()
            << " search=" << FormatEditorRect(surface.searchBox);
        return out.str();
    }

    std::string FormatEditorMenuToolbarRuntimeResult(const EditorMenuToolbarRuntimeResult& result)
    {
        std::ostringstream out;
        out << "editor-menu-toolbar-result handled=" << (result.handled ? 1 : 0)
            << " opened=" << (result.openedMenu ? 1 : 0)
            << " closed=" << (result.closedMenu ? 1 : 0)
            << " command=" << ToString(result.command.id)
            << " palette=" << (result.openCommandPalette ? 1 : 0)
            << " status='" << result.statusText << "'";
        return out.str();
    }

    EditorMenuToolbarRuntimeDiagnostics RunEditorMenuToolbarRuntimeDiagnostics()
    {
        EditorMenuToolbarRuntimeDiagnostics diagnostics{};
        CommandRegistry commands = BuildDefaultEditorCommandRegistry();
        EditorRuntimeBridge bridge = BuildDefaultEditorRuntimeBridge(1366, 768);
        EditorFocusState focus = MakeDefaultEditorFocusState();
        EditorCommandContext context = BuildEditorCommandContext(focus, true, true, false, true, true, true);
        EditorCommandStateCache commandState = BuildEditorCommandStateCache(commands, context);
        EditorShortcutProfile shortcuts = BuildDefaultEditorShortcutProfile(commands, BuildDefaultEditorInputMap());
        EditorMenuModel menu = BuildDefaultEditorMenuModel(commands);
        EditorToolbarModel toolbar = BuildDefaultEditorToolbarModel(commands);

        diagnostics.runtime = MakeDefaultEditorMenuToolbarRuntimeState();
        RefreshEditorMenuToolbarRuntimeSurface(diagnostics.runtime, menu, toolbar, commandState, shortcuts, bridge.plan.frame, 1366, 768);
        diagnostics.rootCount = diagnostics.runtime.surface.roots.size();
        diagnostics.toolbarButtonCount = diagnostics.runtime.surface.toolbarButtons.size();
        diagnostics.surfaceOk = ValidateEditorMenuToolbarSurface(diagnostics.runtime.surface, 1366, 768);

        diagnostics.menuHit = HitTestEditorMenuToolbarSurface(diagnostics.runtime.surface,
                                                              diagnostics.runtime.surface.roots.front().rect.x + 4,
                                                              diagnostics.runtime.surface.roots.front().rect.y + 4);
        diagnostics.rootHitOk = diagnostics.menuHit.kind == EditorMenuToolbarHitKind::MenuRoot;

        EditorMenuToolbarRuntimeInput input{};
        input.menu = &menu;
        input.toolbar = &toolbar;
        input.commandState = &commandState;
        input.shortcuts = &shortcuts;
        input.frame = bridge.plan.frame;
        input.viewportWidth = 1366;
        input.viewportHeight = 768;
        input.mouseX = diagnostics.runtime.surface.roots.front().rect.x + 4;
        input.mouseY = diagnostics.runtime.surface.roots.front().rect.y + 4;
        input.mouseLeftPressed = true;
        diagnostics.openResult = ApplyEditorMenuToolbarRuntimeInput(diagnostics.runtime, input);
        diagnostics.popupItemCount = diagnostics.runtime.surface.popup.items.size();

        for (const EditorMenuToolbarCommandItemRuntime& item : diagnostics.runtime.surface.popup.items)
        {
            if (!item.shortcutText.empty())
            {
                ++diagnostics.shortcutTextCount;
            }
        }

        diagnostics.popupHit = HitTestEditorMenuToolbarSurface(diagnostics.runtime.surface,
                                                               diagnostics.runtime.surface.popup.items.front().rect.x + 4,
                                                               diagnostics.runtime.surface.popup.items.front().rect.y + 4);
        input.mouseX = diagnostics.runtime.surface.popup.items.front().rect.x + 4;
        input.mouseY = diagnostics.runtime.surface.popup.items.front().rect.y + 4;
        input.mouseLeftPressed = true;
        diagnostics.commandResult = ApplyEditorMenuToolbarRuntimeInput(diagnostics.runtime, input);
        diagnostics.popupCommandOk = diagnostics.commandResult.commandQueued && diagnostics.commandResult.command.source == CommandSource::Menu;

        RefreshEditorMenuToolbarRuntimeSurface(diagnostics.runtime, menu, toolbar, commandState, shortcuts, bridge.plan.frame, 1366, 768);
        diagnostics.toolbarHit = HitTestEditorMenuToolbarSurface(diagnostics.runtime.surface,
                                                                 diagnostics.runtime.surface.toolbarButtons.front().rect.x + 4,
                                                                 diagnostics.runtime.surface.toolbarButtons.front().rect.y + 4);
        diagnostics.toolbarCommandOk = diagnostics.toolbarHit.kind == EditorMenuToolbarHitKind::ToolbarButton;

        const EditorMenuToolbarHitTestResult searchHit = HitTestEditorMenuToolbarSurface(diagnostics.runtime.surface,
                                                                                         diagnostics.runtime.surface.searchBox.x + 4,
                                                                                         diagnostics.runtime.surface.searchBox.y + 4);
        diagnostics.searchBoxOk = searchHit.kind == EditorMenuToolbarHitKind::ToolbarSearchBox;
        diagnostics.commandStateOk = !diagnostics.runtime.surface.toolbarButtons.empty();
        diagnostics.ok = diagnostics.rootHitOk && diagnostics.popupCommandOk && diagnostics.toolbarCommandOk && diagnostics.searchBoxOk && diagnostics.commandStateOk && diagnostics.surfaceOk;

        std::ostringstream out;
        out << "editor-menu-toolbar-runtime roots=" << diagnostics.rootCount
            << " buttons=" << diagnostics.toolbarButtonCount
            << " popupItems=" << diagnostics.popupItemCount
            << " shortcuts=" << diagnostics.shortcutTextCount
            << " rootHit=" << (diagnostics.rootHitOk ? 1 : 0)
            << " menuCommand=" << (diagnostics.popupCommandOk ? 1 : 0)
            << " toolbar=" << (diagnostics.toolbarCommandOk ? 1 : 0)
            << " search=" << (diagnostics.searchBoxOk ? 1 : 0)
            << " ok=" << (diagnostics.ok ? 1 : 0);
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string BuildEditorMenuToolbarRuntimeProbeSummary()
    {
        return RunEditorMenuToolbarRuntimeDiagnostics().summary;
    }
}
