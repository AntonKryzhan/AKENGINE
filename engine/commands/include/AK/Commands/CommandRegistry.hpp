#pragma once

#include <AK/Input/InputActions.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class CommandId
    {
        CloseEditor,
        NewEntity,
        NewMesh,
        NewCamera,
        NewLight,
        DeleteSelection,
        SaveScene,
        LoadScene,
        RescanAssets,
        Undo,
        Redo,
        DuplicateSelection,
        FocusSelection,
        ResetViewport,
        ToggleGridSnap,
        SelectPrevious,
        SelectNext,
        RenameSelection,
        ToolTranslate,
        ToolRotate,
        ToolScale,
        ToolSpaceWorld,
        ToolSpaceLocal,
        ViewScene2D,
        ViewScene3D,
        ViewFrameAll,
        ViewAxisTop,
        ViewAxisFront,
        ViewAxisRight,
        Count
    };

    enum class CommandSource
    {
        Shortcut,
        Toolbar,
        Menu,
        CommandPalette,
        Programmatic
    };

    struct CommandDescriptor
    {
        CommandId id = CommandId::CloseEditor;
        const char* name = "";
        const char* displayName = "";
        const char* category = "";
        const char* description = "";
        bool hasShortcut = false;
        InputAction shortcutAction = InputAction::Cancel;
        bool toolbar = false;
        bool destructive = false;
        bool requiresSelection = false;
    };

    struct CommandInvocation
    {
        CommandId id = CommandId::CloseEditor;
        CommandSource source = CommandSource::Programmatic;
    };

    struct CommandDiagnostics
    {
        std::size_t commandCount = 0;
        std::size_t shortcutCommandCount = 0;
        std::size_t toolbarCommandCount = 0;
        std::size_t destructiveCommandCount = 0;
        std::size_t requiresSelectionCount = 0;
        std::size_t duplicateIdCount = 0;
        std::size_t duplicateNameCount = 0;
        std::size_t missingShortcutBindingCount = 0;
        std::size_t missingRequiredCommandCount = 0;
        std::string summary;
    };

    class CommandRegistry final
    {
    public:
        void Add(const CommandDescriptor& descriptor);
        const CommandDescriptor* Find(CommandId id) const;
        const CommandDescriptor* FindByName(const std::string& name) const;
        const CommandDescriptor* FindByShortcut(InputAction action) const;
        bool Has(CommandId id) const;
        const std::vector<CommandDescriptor>& Commands() const;

    private:
        std::vector<CommandDescriptor> mCommands;
    };

    CommandRegistry BuildDefaultEditorCommandRegistry();
    CommandDiagnostics BuildCommandDiagnostics(const CommandRegistry& registry, const InputMap& inputMap);
    std::vector<CommandInvocation> CollectCommandInvocations(const CommandRegistry& registry, const InputActionSnapshot& inputSnapshot);
    std::string BuildCommandProbeSummary();

    const char* ToString(CommandId id);
    const char* ToString(CommandSource source);
    std::string FormatCommand(const CommandDescriptor& descriptor);
    std::string FormatCommandInvocation(const CommandRegistry& registry, const CommandInvocation& invocation);
}
