#include <AK/Commands/CommandRegistry.hpp>

#include <array>
#include <sstream>
#include <unordered_set>

namespace AK
{
    namespace
    {
        constexpr std::size_t CommandIndex(CommandId id)
        {
            return static_cast<std::size_t>(id);
        }

        const char* SafeText(const char* text)
        {
            return text ? text : "";
        }

        bool IsActionBound(const InputMap& inputMap, InputAction action)
        {
            for (const InputBinding& binding : inputMap.Bindings())
            {
                if (binding.action == action)
                {
                    return true;
                }
            }

            return false;
        }

        void AddEditorCommand(CommandRegistry& registry, CommandId id, const char* name, const char* displayName, const char* category, const char* description, bool hasShortcut, InputAction shortcutAction, bool toolbar, bool destructive, bool requiresSelection)
        {
            registry.Add({id, name, displayName, category, description, hasShortcut, shortcutAction, toolbar, destructive, requiresSelection});
        }
    }

    void CommandRegistry::Add(const CommandDescriptor& descriptor)
    {
        mCommands.push_back(descriptor);
    }

    const CommandDescriptor* CommandRegistry::Find(CommandId id) const
    {
        for (const CommandDescriptor& descriptor : mCommands)
        {
            if (descriptor.id == id)
            {
                return &descriptor;
            }
        }

        return nullptr;
    }

    const CommandDescriptor* CommandRegistry::FindByName(const std::string& name) const
    {
        for (const CommandDescriptor& descriptor : mCommands)
        {
            if (name == SafeText(descriptor.name))
            {
                return &descriptor;
            }
        }

        return nullptr;
    }

    const CommandDescriptor* CommandRegistry::FindByShortcut(InputAction action) const
    {
        for (const CommandDescriptor& descriptor : mCommands)
        {
            if (descriptor.hasShortcut && descriptor.shortcutAction == action)
            {
                return &descriptor;
            }
        }

        return nullptr;
    }

    bool CommandRegistry::Has(CommandId id) const
    {
        return Find(id) != nullptr;
    }

    const std::vector<CommandDescriptor>& CommandRegistry::Commands() const
    {
        return mCommands;
    }

    CommandRegistry BuildDefaultEditorCommandRegistry()
    {
        CommandRegistry registry;

        AddEditorCommand(registry, CommandId::CloseEditor, "editor.close", "Close Editor", "Editor", "Close the editor window.", true, InputAction::Cancel, false, false, false);
        AddEditorCommand(registry, CommandId::NewEntity, "scene.new_entity", "New Entity", "Scene", "Create a new empty entity.", true, InputAction::NewEntity, true, false, false);
        AddEditorCommand(registry, CommandId::NewMesh, "scene.new_mesh", "New Mesh", "Scene", "Create a mesh entity using the built-in cube mesh.", false, InputAction::NewEntity, true, false, false);
        AddEditorCommand(registry, CommandId::NewCamera, "scene.new_camera", "New Camera", "Scene", "Create a camera entity.", false, InputAction::NewEntity, true, false, false);
        AddEditorCommand(registry, CommandId::NewLight, "scene.new_light", "New Light", "Scene", "Create a light entity.", false, InputAction::NewEntity, true, false, false);
        AddEditorCommand(registry, CommandId::DeleteSelection, "scene.delete_selection", "Delete Selection", "Scene", "Delete the selected entity after confirmation.", true, InputAction::DeleteSelection, true, true, true);
        AddEditorCommand(registry, CommandId::SaveScene, "scene.save", "Save Scene", "Scene", "Save the current scene through the atomic scene save path.", true, InputAction::SaveScene, true, false, false);
        AddEditorCommand(registry, CommandId::LoadScene, "scene.load", "Load Scene", "Scene", "Load the sandbox scene from disk.", true, InputAction::LoadScene, true, false, false);
        AddEditorCommand(registry, CommandId::RescanAssets, "assets.rescan", "Rescan Assets", "Assets", "Rescan the project asset directory.", true, InputAction::RescanAssets, true, false, false);
        AddEditorCommand(registry, CommandId::Undo, "edit.undo", "Undo", "Edit", "Undo the last scene edit.", true, InputAction::Undo, false, false, false);
        AddEditorCommand(registry, CommandId::Redo, "edit.redo", "Redo", "Edit", "Redo the last undone scene edit.", true, InputAction::Redo, false, false, false);
        AddEditorCommand(registry, CommandId::DuplicateSelection, "edit.duplicate_selection", "Duplicate Selection", "Edit", "Duplicate the selected entity and its components.", true, InputAction::DuplicateSelection, false, false, true);
        AddEditorCommand(registry, CommandId::FocusSelection, "viewport.focus_selection", "Focus Selection", "Viewport", "Center the viewport on the selected entity.", true, InputAction::FocusSelection, false, false, true);
        AddEditorCommand(registry, CommandId::ResetViewport, "viewport.reset", "Reset Viewport", "Viewport", "Reset viewport camera position and zoom.", true, InputAction::ResetViewport, false, false, false);
        AddEditorCommand(registry, CommandId::ToggleGridSnap, "viewport.toggle_grid_snap", "Toggle Grid Snap", "Viewport", "Enable or disable grid-snapped editing.", true, InputAction::ToggleGridSnap, false, false, false);
        AddEditorCommand(registry, CommandId::SelectPrevious, "selection.previous", "Select Previous", "Selection", "Select the previous entity in the hierarchy.", true, InputAction::SelectPrevious, false, false, false);
        AddEditorCommand(registry, CommandId::SelectNext, "selection.next", "Select Next", "Selection", "Select the next entity in the hierarchy.", true, InputAction::SelectNext, false, false, false);
        AddEditorCommand(registry, CommandId::RenameSelection, "selection.rename", "Rename Selection", "Selection", "Enter rename mode for the selected entity.", true, InputAction::RenameSelection, false, false, true);
        AddEditorCommand(registry, CommandId::ToolTranslate, "tools.translate", "Translate", "Tools", "Use the Scene View translation gizmo.", false, InputAction::Cancel, true, false, false);
        AddEditorCommand(registry, CommandId::ToolRotate, "tools.rotate", "Rotate", "Tools", "Use the Scene View rotation gizmo.", false, InputAction::Cancel, true, false, false);
        AddEditorCommand(registry, CommandId::ToolScale, "tools.scale", "Scale", "Tools", "Use the Scene View scale gizmo.", false, InputAction::Cancel, true, false, false);
        AddEditorCommand(registry, CommandId::ToolSpaceWorld, "tools.space_world", "World Space", "Tools", "Use world-space Scene View transform axes.", false, InputAction::Cancel, true, false, false);
        AddEditorCommand(registry, CommandId::ToolSpaceLocal, "tools.space_local", "Local Space", "Tools", "Use selected-entity local Scene View transform axes.", false, InputAction::Cancel, true, false, false);
        AddEditorCommand(registry, CommandId::ViewScene2D, "view.scene_2d", "Scene View 2D", "View", "Use a 2D orthographic Scene View camera.", false, InputAction::Cancel, true, false, false);
        AddEditorCommand(registry, CommandId::ViewScene3D, "view.scene_3d", "Scene View 3D", "View", "Use a 3D perspective Scene View camera.", false, InputAction::Cancel, true, false, false);
        AddEditorCommand(registry, CommandId::ViewFrameAll, "view.frame_all", "Frame All", "View", "Frame all visible scene bounds in the active Scene View camera.", false, InputAction::Cancel, false, false, false);
        AddEditorCommand(registry, CommandId::ViewAxisTop, "view.axis_top", "View Top", "View", "Snap the Scene View camera to the top axis.", false, InputAction::Cancel, false, false, false);
        AddEditorCommand(registry, CommandId::ViewAxisFront, "view.axis_front", "View Front", "View", "Snap the Scene View camera to the front axis.", false, InputAction::Cancel, false, false, false);
        AddEditorCommand(registry, CommandId::ViewAxisRight, "view.axis_right", "View Right", "View", "Snap the Scene View camera to the right axis.", false, InputAction::Cancel, false, false, false);

        return registry;
    }

    CommandDiagnostics BuildCommandDiagnostics(const CommandRegistry& registry, const InputMap& inputMap)
    {
        CommandDiagnostics diagnostics{};
        diagnostics.commandCount = registry.Commands().size();

        std::unordered_set<std::size_t> ids;
        std::unordered_set<std::string> names;
        for (const CommandDescriptor& descriptor : registry.Commands())
        {
            if (!ids.insert(CommandIndex(descriptor.id)).second)
            {
                ++diagnostics.duplicateIdCount;
            }

            const std::string name = SafeText(descriptor.name);
            if (name.empty() || !names.insert(name).second)
            {
                ++diagnostics.duplicateNameCount;
            }

            if (descriptor.hasShortcut)
            {
                ++diagnostics.shortcutCommandCount;
                if (!IsActionBound(inputMap, descriptor.shortcutAction))
                {
                    ++diagnostics.missingShortcutBindingCount;
                }
            }

            if (descriptor.toolbar)
            {
                ++diagnostics.toolbarCommandCount;
            }
            if (descriptor.destructive)
            {
                ++diagnostics.destructiveCommandCount;
            }
            if (descriptor.requiresSelection)
            {
                ++diagnostics.requiresSelectionCount;
            }
        }

        for (std::size_t index = 0; index < static_cast<std::size_t>(CommandId::Count); ++index)
        {
            if (!registry.Has(static_cast<CommandId>(index)))
            {
                ++diagnostics.missingRequiredCommandCount;
            }
        }

        std::ostringstream out;
        out << "Command registry: commands=" << diagnostics.commandCount
            << " shortcuts=" << diagnostics.shortcutCommandCount
            << " toolbar=" << diagnostics.toolbarCommandCount
            << " destructive=" << diagnostics.destructiveCommandCount
            << " selection=" << diagnostics.requiresSelectionCount
            << " duplicates=" << (diagnostics.duplicateIdCount + diagnostics.duplicateNameCount)
            << " missingShortcut=" << diagnostics.missingShortcutBindingCount
            << " missingCommand=" << diagnostics.missingRequiredCommandCount;
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::vector<CommandInvocation> CollectCommandInvocations(const CommandRegistry& registry, const InputActionSnapshot& inputSnapshot)
    {
        std::vector<CommandInvocation> invocations;
        for (const CommandDescriptor& descriptor : registry.Commands())
        {
            if (descriptor.hasShortcut && inputSnapshot.Pressed(descriptor.shortcutAction))
            {
                invocations.push_back({descriptor.id, CommandSource::Shortcut});
            }
        }

        return invocations;
    }

    std::string BuildCommandProbeSummary()
    {
        const InputMap inputMap = BuildDefaultEditorInputMap();
        const CommandRegistry registry = BuildDefaultEditorCommandRegistry();
        return BuildCommandDiagnostics(registry, inputMap).summary;
    }

    const char* ToString(CommandId id)
    {
        switch (id)
        {
            case CommandId::CloseEditor:
                return "CloseEditor";
            case CommandId::NewEntity:
                return "NewEntity";
            case CommandId::NewMesh:
                return "NewMesh";
            case CommandId::NewCamera:
                return "NewCamera";
            case CommandId::NewLight:
                return "NewLight";
            case CommandId::DeleteSelection:
                return "DeleteSelection";
            case CommandId::SaveScene:
                return "SaveScene";
            case CommandId::LoadScene:
                return "LoadScene";
            case CommandId::RescanAssets:
                return "RescanAssets";
            case CommandId::Undo:
                return "Undo";
            case CommandId::Redo:
                return "Redo";
            case CommandId::DuplicateSelection:
                return "DuplicateSelection";
            case CommandId::FocusSelection:
                return "FocusSelection";
            case CommandId::ResetViewport:
                return "ResetViewport";
            case CommandId::ToggleGridSnap:
                return "ToggleGridSnap";
            case CommandId::SelectPrevious:
                return "SelectPrevious";
            case CommandId::SelectNext:
                return "SelectNext";
            case CommandId::RenameSelection:
                return "RenameSelection";
            case CommandId::ToolTranslate:
                return "ToolTranslate";
            case CommandId::ToolRotate:
                return "ToolRotate";
            case CommandId::ToolScale:
                return "ToolScale";
            case CommandId::ToolSpaceWorld:
                return "ToolSpaceWorld";
            case CommandId::ToolSpaceLocal:
                return "ToolSpaceLocal";
            case CommandId::ViewScene2D:
                return "ViewScene2D";
            case CommandId::ViewScene3D:
                return "ViewScene3D";
            case CommandId::ViewFrameAll:
                return "ViewFrameAll";
            case CommandId::ViewAxisTop:
                return "ViewAxisTop";
            case CommandId::ViewAxisFront:
                return "ViewAxisFront";
            case CommandId::ViewAxisRight:
                return "ViewAxisRight";
            default:
                return "Unknown";
        }
    }

    const char* ToString(CommandSource source)
    {
        switch (source)
        {
            case CommandSource::Shortcut:
                return "Shortcut";
            case CommandSource::Toolbar:
                return "Toolbar";
            case CommandSource::Menu:
                return "Menu";
            case CommandSource::CommandPalette:
                return "CommandPalette";
            case CommandSource::Programmatic:
                return "Programmatic";
            default:
                return "Unknown";
        }
    }

    std::string FormatCommand(const CommandDescriptor& descriptor)
    {
        std::ostringstream out;
        out << SafeText(descriptor.name) << " [" << SafeText(descriptor.category) << "] "
            << SafeText(descriptor.displayName);
        if (descriptor.hasShortcut)
        {
            out << " shortcut=" << ToString(descriptor.shortcutAction);
        }
        if (descriptor.toolbar)
        {
            out << " toolbar=yes";
        }
        if (descriptor.destructive)
        {
            out << " destructive=yes";
        }
        return out.str();
    }

    std::string FormatCommandInvocation(const CommandRegistry& registry, const CommandInvocation& invocation)
    {
        const CommandDescriptor* descriptor = registry.Find(invocation.id);
        std::ostringstream out;
        out << ToString(invocation.source) << ": ";
        if (descriptor)
        {
            out << SafeText(descriptor->name) << " / " << SafeText(descriptor->displayName);
        }
        else
        {
            out << ToString(invocation.id);
        }
        return out.str();
    }
}
