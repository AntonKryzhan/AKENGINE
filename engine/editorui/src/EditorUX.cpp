#include <AK/EditorUI/EditorUX.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    namespace
    {
        EditorRect MakeRect(i32 x, i32 y, i32 width, i32 height)
        {
            return {x, y, width, height};
        }

        bool RectValid(EditorRect rect)
        {
            return rect.width > 0 && rect.height > 0;
        }

        EditorContextMenuItem Item(std::string id, std::string label, EditorIconKind icon = EditorIconKind::None, bool separatorBefore = false, bool enabled = true, bool checked = false, bool destructive = false)
        {
            EditorContextMenuItem item{};
            item.id = std::move(id);
            item.label = std::move(label);
            item.icon = icon;
            item.separatorBefore = separatorBefore;
            item.enabled = enabled;
            item.checked = checked;
            item.destructive = destructive;
            return item;
        }

        void AddPopupRow(EditorPopupModel& popup, EditorWidgetKind kind, EditorIconKind icon, EditorRect rect, std::string id, std::string label, std::string value = {}, EditorWidgetState state = EditorWidgetState::Normal)
        {
            EditorWidgetDesc row{};
            row.kind = kind;
            row.icon = icon;
            row.rect = rect;
            row.id = std::move(id);
            row.label = std::move(label);
            row.value = std::move(value);
            row.state = state;
            popup.rows.push_back(std::move(row));
        }

        EditorPopupWorkflowDesc MenuWorkflow(std::string id, std::string label, EditorPopupAnchorKind anchorKind, EditorRect anchor)
        {
            EditorPopupWorkflowDesc workflow{};
            workflow.id = std::move(id);
            workflow.label = std::move(label);
            workflow.anchorKind = anchorKind;
            workflow.anchor = anchor;
            workflow.menu.id = workflow.id + ".menu";
            workflow.menu.anchor = anchor;
            workflow.menu.open = true;
            workflow.commandBacked = true;
            return workflow;
        }

        std::size_t CountItems(const std::vector<EditorContextMenuItem>& items, std::size_t& destructive)
        {
            std::size_t count = 0;
            for (const EditorContextMenuItem& item : items)
            {
                ++count;
                if (item.destructive)
                {
                    ++destructive;
                }
                count += CountItems(item.children, destructive);
            }
            return count;
        }
    }

    const char* ToString(EditorPopupAnchorKind kind)
    {
        switch (kind)
        {
            case EditorPopupAnchorKind::None: return "none";
            case EditorPopupAnchorKind::MainMenu: return "main-menu";
            case EditorPopupAnchorKind::Toolbar: return "toolbar";
            case EditorPopupAnchorKind::Hierarchy: return "hierarchy";
            case EditorPopupAnchorKind::Inspector: return "inspector";
            case EditorPopupAnchorKind::SceneView: return "scene-view";
            case EditorPopupAnchorKind::ProjectBrowser: return "project-browser";
        }
        return "unknown";
    }

    EditorPopupWorkflowDesc BuildGameObjectCreateWorkflow(EditorRect anchor)
    {
        EditorPopupWorkflowDesc workflow = MenuWorkflow("gameobject.create", "Create GameObject", EditorPopupAnchorKind::MainMenu, anchor);
        workflow.menu.items.push_back(Item("create.empty", "Create Empty", EditorIconKind::Entity));
        workflow.menu.items.push_back(Item("create.emptyChild", "Create Empty Child", EditorIconKind::Entity));
        EditorContextMenuItem object3D = Item("create.3d", "3D Object", EditorIconKind::Mesh, true);
        object3D.children.push_back(Item("create.cube", "Cube", EditorIconKind::Mesh));
        object3D.children.push_back(Item("create.sphere", "Sphere", EditorIconKind::Mesh));
        object3D.children.push_back(Item("create.capsule", "Capsule", EditorIconKind::Mesh));
        object3D.children.push_back(Item("create.plane", "Plane", EditorIconKind::Mesh));
        workflow.menu.items.push_back(std::move(object3D));
        workflow.menu.items.push_back(Item("create.camera", "Camera", EditorIconKind::Camera, true));
        workflow.menu.items.push_back(Item("create.light", "Light", EditorIconKind::Light));
        return workflow;
    }

    EditorPopupWorkflowDesc BuildHierarchyContextWorkflow(EditorRect anchor)
    {
        EditorPopupWorkflowDesc workflow = MenuWorkflow("hierarchy.context", "Hierarchy Context", EditorPopupAnchorKind::Hierarchy, anchor);
        workflow.menu.items.push_back(Item("cut", "Cut"));
        workflow.menu.items.push_back(Item("copy", "Copy"));
        workflow.menu.items.push_back(Item("paste", "Paste", EditorIconKind::None, false, false));
        workflow.menu.items.push_back(Item("rename", "Rename", EditorIconKind::None, true));
        workflow.menu.items.push_back(Item("duplicate", "Duplicate"));
        workflow.menu.items.push_back(Item("delete", "Delete", EditorIconKind::Error, false, true, false, true));
        workflow.destructive = true;
        EditorContextMenuItem create = Item("create", "Create", EditorIconKind::Plus, true);
        create.children.push_back(Item("create.empty", "Empty GameObject", EditorIconKind::Entity));
        create.children.push_back(Item("create.mesh", "Mesh", EditorIconKind::Mesh));
        create.children.push_back(Item("create.camera", "Camera", EditorIconKind::Camera));
        create.children.push_back(Item("create.light", "Light", EditorIconKind::Light));
        workflow.menu.items.push_back(std::move(create));
        return workflow;
    }

    EditorPopupWorkflowDesc BuildProjectContextWorkflow(EditorRect anchor)
    {
        EditorPopupWorkflowDesc workflow = MenuWorkflow("project.context", "Project Context", EditorPopupAnchorKind::ProjectBrowser, anchor);
        workflow.menu.items.push_back(Item("showInExplorer", "Show in Explorer", EditorIconKind::Folder));
        workflow.menu.items.push_back(Item("reimport", "Reimport", EditorIconKind::Asset));
        workflow.menu.items.push_back(Item("copyPath", "Copy Path"));
        EditorContextMenuItem create = Item("create.asset", "Create", EditorIconKind::Plus, true);
        create.children.push_back(Item("create.material", "Material", EditorIconKind::Material));
        create.children.push_back(Item("create.shader", "Shader", EditorIconKind::Shader));
        create.children.push_back(Item("create.folder", "Folder", EditorIconKind::Folder));
        workflow.menu.items.push_back(std::move(create));
        workflow.menu.items.push_back(Item("deleteAsset", "Delete", EditorIconKind::Error, true, true, false, true));
        workflow.destructive = true;
        return workflow;
    }

    EditorPopupWorkflowDesc BuildAddComponentWorkflow(EditorRect anchor)
    {
        EditorPopupWorkflowDesc workflow = MenuWorkflow("inspector.addComponent", "Add Component", EditorPopupAnchorKind::Inspector, anchor);
        workflow.menu.items.push_back(Item("component.mesh", "Mesh Renderer", EditorIconKind::Mesh));
        workflow.menu.items.push_back(Item("component.camera", "Camera", EditorIconKind::Camera));
        workflow.menu.items.push_back(Item("component.light", "Light", EditorIconKind::Light));
        workflow.menu.items.push_back(Item("component.bounds", "Bounds", EditorIconKind::Bounds));
        workflow.menu.items.push_back(Item("component.script", "Script", EditorIconKind::Script, true, false));
        return workflow;
    }

    EditorPopupWorkflowDesc BuildSceneShadingWorkflow(EditorRect anchor)
    {
        EditorPopupWorkflowDesc workflow = MenuWorkflow("scene.shading", "Scene Shading", EditorPopupAnchorKind::SceneView, anchor);
        workflow.menu.items.push_back(Item("shaded", "Shaded", EditorIconKind::None, false, true, true));
        workflow.menu.items.push_back(Item("wireframe", "Wireframe"));
        workflow.menu.items.push_back(Item("overdraw", "Overdraw"));
        workflow.menu.items.push_back(Item("rendergraph", "RenderGraph Debug", EditorIconKind::Diagnostics, true));
        return workflow;
    }

    EditorPopupWorkflowDesc BuildGridSnapWorkflow(EditorRect anchor)
    {
        EditorPopupWorkflowDesc workflow{};
        workflow.id = "scene.gridSnap";
        workflow.label = "Grid and Snap";
        workflow.anchorKind = EditorPopupAnchorKind::SceneView;
        workflow.anchor = anchor;
        workflow.commandBacked = true;
        workflow.popup.id = "scene.gridSnap.popup";
        workflow.popup.title = "Grid and Snap";
        workflow.popup.anchor = anchor;
        workflow.popup.open = true;
        AddPopupRow(workflow.popup, EditorWidgetKind::CheckBox, EditorIconKind::None, MakeRect(anchor.x, anchor.y, 220, 22), "grid.visible", "Grid Visible", "On", EditorWidgetState::Checked);
        AddPopupRow(workflow.popup, EditorWidgetKind::CheckBox, EditorIconKind::None, MakeRect(anchor.x, anchor.y + 24, 220, 22), "grid.snap", "Snap Enabled", "Off");
        AddPopupRow(workflow.popup, EditorWidgetKind::NumericField, EditorIconKind::None, MakeRect(anchor.x, anchor.y + 48, 220, 22), "grid.step", "Move Snap", "0.50");
        AddPopupRow(workflow.popup, EditorWidgetKind::NumericField, EditorIconKind::None, MakeRect(anchor.x, anchor.y + 72, 220, 22), "grid.rotate", "Rotate Snap", "15°");
        return workflow;
    }

    EditorUXFrame BuildDefaultEditorUXFrame(i32 width, i32 height)
    {
        EditorUXFrame frame{};
        frame.compactSkin = MakeDefaultEditorWidgetSkin();
        frame.compactSkin.rowHeight = 20;
        frame.compactSkin.headerHeight = 23;
        frame.compactSkin.tabHeight = 22;
        frame.compactSkin.fieldHeight = 18;
        frame.compactSkin.inspectorLabelWidth = 68;
        frame.inspectorTargetWidth = std::clamp(width / 5, 300, 380);
        frame.hierarchyTargetWidth = std::clamp(width / 6, 260, 320);
        frame.projectTargetHeight = std::clamp(height / 4, 170, 230);
        frame.sceneToolbarHeight = 24;
        frame.workflows.push_back(BuildGameObjectCreateWorkflow(MakeRect(158, 22, 220, 210)));
        frame.workflows.push_back(BuildHierarchyContextWorkflow(MakeRect(52, 148, 230, 260)));
        frame.workflows.push_back(BuildProjectContextWorkflow(MakeRect(360, height - frame.projectTargetHeight + 58, 240, 230)));
        frame.workflows.push_back(BuildAddComponentWorkflow(MakeRect(width - frame.inspectorTargetWidth + 20, height - 70, 240, 178)));
        frame.workflows.push_back(BuildSceneShadingWorkflow(MakeRect(frame.hierarchyTargetWidth + 320, 108, 210, 160)));
        frame.workflows.push_back(BuildGridSnapWorkflow(MakeRect(frame.hierarchyTargetWidth + 210, 108, 220, 118)));
        return frame;
    }

    EditorUXDiagnostics ValidateEditorUXFrame(const EditorUXFrame& frame)
    {
        EditorUXDiagnostics diagnostics{};
        diagnostics.workflowCount = frame.workflows.size();
        for (const EditorPopupWorkflowDesc& workflow : frame.workflows)
        {
            if (!RectValid(workflow.anchor))
            {
                ++diagnostics.invalidAnchorCount;
            }
            if (workflow.commandBacked)
            {
                ++diagnostics.commandBackedCount;
            }
            if (workflow.menu.open)
            {
                ++diagnostics.openMenuCount;
                diagnostics.menuItemCount += CountItems(workflow.menu.items, diagnostics.destructiveItemCount);
            }
            if (workflow.popup.open)
            {
                ++diagnostics.openMenuCount;
                diagnostics.popupRowCount += workflow.popup.rows.size();
            }
            if (workflow.destructive)
            {
                ++diagnostics.destructiveItemCount;
            }
        }
        diagnostics.ok = diagnostics.workflowCount >= 5
            && diagnostics.openMenuCount >= 5
            && diagnostics.menuItemCount >= 20
            && diagnostics.popupRowCount >= 4
            && diagnostics.commandBackedCount >= 5
            && diagnostics.invalidAnchorCount == 0;
        diagnostics.summary = FormatEditorUXDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorPopupWorkflow(const EditorPopupWorkflowDesc& workflow)
    {
        std::ostringstream out;
        out << workflow.id << " label=" << workflow.label
            << " anchor=" << ToString(workflow.anchorKind)
            << " rect=" << FormatEditorRect(workflow.anchor)
            << " menuItems=" << workflow.menu.items.size()
            << " popupRows=" << workflow.popup.rows.size();
        return out.str();
    }

    std::string FormatEditorUXDiagnostics(const EditorUXDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-ux workflows=" << diagnostics.workflowCount
            << " open=" << diagnostics.openMenuCount
            << " menuItems=" << diagnostics.menuItemCount
            << " popupRows=" << diagnostics.popupRowCount
            << " commandBacked=" << diagnostics.commandBackedCount
            << " destructive=" << diagnostics.destructiveItemCount
            << " invalidAnchors=" << diagnostics.invalidAnchorCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        return out.str();
    }
}
