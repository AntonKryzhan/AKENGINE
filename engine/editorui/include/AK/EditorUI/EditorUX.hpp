#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorWidgets.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorPopupAnchorKind
    {
        None,
        MainMenu,
        Toolbar,
        Hierarchy,
        Inspector,
        SceneView,
        ProjectBrowser
    };

    struct EditorPopupWorkflowDesc
    {
        std::string id;
        std::string label;
        EditorPopupAnchorKind anchorKind = EditorPopupAnchorKind::None;
        EditorRect anchor{};
        EditorContextMenuModel menu{};
        EditorPopupModel popup{};
        bool commandBacked = false;
        bool destructive = false;
    };

    struct EditorUXFrame
    {
        std::vector<EditorPopupWorkflowDesc> workflows;
        EditorWidgetSkin compactSkin{};
        i32 inspectorTargetWidth = 340;
        i32 hierarchyTargetWidth = 300;
        i32 projectTargetHeight = 205;
        i32 sceneToolbarHeight = 24;
        u64 revision = 1;
    };

    struct EditorUXDiagnostics
    {
        std::size_t workflowCount = 0;
        std::size_t openMenuCount = 0;
        std::size_t menuItemCount = 0;
        std::size_t popupRowCount = 0;
        std::size_t commandBackedCount = 0;
        std::size_t destructiveItemCount = 0;
        std::size_t invalidAnchorCount = 0;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorPopupAnchorKind kind);

    EditorPopupWorkflowDesc BuildGameObjectCreateWorkflow(EditorRect anchor);
    EditorPopupWorkflowDesc BuildHierarchyContextWorkflow(EditorRect anchor);
    EditorPopupWorkflowDesc BuildProjectContextWorkflow(EditorRect anchor);
    EditorPopupWorkflowDesc BuildAddComponentWorkflow(EditorRect anchor);
    EditorPopupWorkflowDesc BuildSceneShadingWorkflow(EditorRect anchor);
    EditorPopupWorkflowDesc BuildGridSnapWorkflow(EditorRect anchor);
    EditorUXFrame BuildDefaultEditorUXFrame(i32 width, i32 height);
    EditorUXDiagnostics ValidateEditorUXFrame(const EditorUXFrame& frame);
    std::string FormatEditorPopupWorkflow(const EditorPopupWorkflowDesc& workflow);
    std::string FormatEditorUXDiagnostics(const EditorUXDiagnostics& diagnostics);
}
