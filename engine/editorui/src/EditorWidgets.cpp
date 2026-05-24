#include <AK/EditorUI/EditorWidgets.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_set>

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

        std::string CleanUnityLinePrefix(std::string value)
        {
            if (value.rfind("> ", 0) == 0)
            {
                value.erase(0, 2);
            }
            return value;
        }

        EditorIconKind GuessHierarchyIcon(std::string_view text)
        {
            if (text.find("[M]") != std::string_view::npos || text.find("Mesh") != std::string_view::npos)
            {
                return EditorIconKind::Mesh;
            }
            if (text.find("[C]") != std::string_view::npos || text.find("Camera") != std::string_view::npos)
            {
                return EditorIconKind::Camera;
            }
            if (text.find("[L]") != std::string_view::npos || text.find("Light") != std::string_view::npos)
            {
                return EditorIconKind::Light;
            }
            return EditorIconKind::Entity;
        }

        EditorIconKind GuessAssetIcon(std::string_view text)
        {
            if (text.find("shader") != std::string_view::npos || text.find(".slang") != std::string_view::npos || text.find(".hlsl") != std::string_view::npos)
            {
                return EditorIconKind::Shader;
            }
            if (text.find("material") != std::string_view::npos || text.find("mat") != std::string_view::npos)
            {
                return EditorIconKind::Material;
            }
            if (text.find("mesh") != std::string_view::npos || text.find("cube") != std::string_view::npos)
            {
                return EditorIconKind::Mesh;
            }
            if (text.find("/") == std::string_view::npos && text.find(".") == std::string_view::npos)
            {
                return EditorIconKind::Folder;
            }
            return EditorIconKind::Asset;
        }

        void AddWidget(std::vector<EditorWidgetDesc>& widgets, EditorWidgetKind kind, EditorIconKind icon, EditorRect rect, std::string id, std::string label, std::string value = {}, EditorWidgetState state = EditorWidgetState::Normal, i32 depth = 0)
        {
            EditorWidgetDesc widget{};
            widget.kind = kind;
            widget.icon = icon;
            widget.rect = rect;
            widget.id = std::move(id);
            widget.label = std::move(label);
            widget.value = std::move(value);
            widget.state = state;
            widget.depth = depth;
            widgets.push_back(std::move(widget));
        }

        void AddVector3Field(std::vector<EditorWidgetDesc>& widgets, EditorRect row, const char* id, const char* label, const char* value, const EditorWidgetSkin& skin)
        {
            AddWidget(widgets, EditorWidgetKind::Vec3Field, EditorIconKind::None, row, id, label, value, EditorWidgetState::Normal);
            const i32 fieldLeft = row.x + skin.inspectorLabelWidth;
            const i32 fieldWidth = std::max<i32>(36, (row.width - skin.inspectorLabelWidth - skin.padding * 2) / 3);
            AddWidget(widgets, EditorWidgetKind::NumericField, EditorIconKind::None, MakeRect(fieldLeft, row.y, fieldWidth, skin.fieldHeight), std::string(id) + ".x", "X", "0.00");
            AddWidget(widgets, EditorWidgetKind::NumericField, EditorIconKind::None, MakeRect(fieldLeft + fieldWidth + 4, row.y, fieldWidth, skin.fieldHeight), std::string(id) + ".y", "Y", "0.00");
            AddWidget(widgets, EditorWidgetKind::NumericField, EditorIconKind::None, MakeRect(fieldLeft + (fieldWidth + 4) * 2, row.y, fieldWidth, skin.fieldHeight), std::string(id) + ".z", "Z", value);
        }

        EditorContextMenuItem MakeContextMenuItem(std::string id, std::string label, EditorIconKind icon = EditorIconKind::None, bool separatorBefore = false, bool enabled = true, bool checked = false, bool destructive = false)
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
    }

    const char* ToString(EditorWidgetKind kind)
    {
        switch (kind)
        {
            case EditorWidgetKind::Label: return "Label";
            case EditorWidgetKind::Button: return "Button";
            case EditorWidgetKind::IconButton: return "IconButton";
            case EditorWidgetKind::Toggle: return "Toggle";
            case EditorWidgetKind::CheckBox: return "CheckBox";
            case EditorWidgetKind::Dropdown: return "Dropdown";
            case EditorWidgetKind::SearchBox: return "SearchBox";
            case EditorWidgetKind::TextField: return "TextField";
            case EditorWidgetKind::NumericField: return "NumericField";
            case EditorWidgetKind::Vec3Field: return "Vec3Field";
            case EditorWidgetKind::Foldout: return "Foldout";
            case EditorWidgetKind::Separator: return "Separator";
            case EditorWidgetKind::TreeRow: return "TreeRow";
            case EditorWidgetKind::AssetGridItem: return "AssetGridItem";
            case EditorWidgetKind::ContextMenu: return "ContextMenu";
            case EditorWidgetKind::PopupPanel: return "PopupPanel";
            case EditorWidgetKind::ComponentHeader: return "ComponentHeader";
            case EditorWidgetKind::InspectorObjectHeader: return "InspectorObjectHeader";
            case EditorWidgetKind::SceneToolbar: return "SceneToolbar";
        }
        return "Unknown";
    }

    const char* ToString(EditorIconKind kind)
    {
        switch (kind)
        {
            case EditorIconKind::None: return "None";
            case EditorIconKind::Scene: return "Scene";
            case EditorIconKind::Folder: return "Folder";
            case EditorIconKind::Asset: return "Asset";
            case EditorIconKind::Entity: return "Entity";
            case EditorIconKind::Mesh: return "Mesh";
            case EditorIconKind::Camera: return "Camera";
            case EditorIconKind::Light: return "Light";
            case EditorIconKind::Material: return "Material";
            case EditorIconKind::Shader: return "Shader";
            case EditorIconKind::Script: return "Script";
            case EditorIconKind::Transform: return "Transform";
            case EditorIconKind::Bounds: return "Bounds";
            case EditorIconKind::Console: return "Console";
            case EditorIconKind::Diagnostics: return "Diagnostics";
            case EditorIconKind::Search: return "Search";
            case EditorIconKind::Plus: return "Plus";
            case EditorIconKind::Lock: return "Lock";
            case EditorIconKind::Eye: return "Eye";
            case EditorIconKind::Hand: return "Hand";
            case EditorIconKind::Move: return "Move";
            case EditorIconKind::Rotate: return "Rotate";
            case EditorIconKind::Scale: return "Scale";
            case EditorIconKind::Play: return "Play";
            case EditorIconKind::Pause: return "Pause";
            case EditorIconKind::Step: return "Step";
            case EditorIconKind::Warning: return "Warning";
            case EditorIconKind::Error: return "Error";
        }
        return "Unknown";
    }

    const char* ToString(EditorWidgetState state)
    {
        if (state == EditorWidgetState::Normal)
        {
            return "Normal";
        }
        if (HasEditorWidgetState(state, EditorWidgetState::Selected))
        {
            return "Selected";
        }
        if (HasEditorWidgetState(state, EditorWidgetState::Active))
        {
            return "Active";
        }
        if (HasEditorWidgetState(state, EditorWidgetState::Disabled))
        {
            return "Disabled";
        }
        if (HasEditorWidgetState(state, EditorWidgetState::Expanded))
        {
            return "Expanded";
        }
        return "Mixed";
    }

    EditorWidgetSkin MakeEditorWidgetSkinFromTheme(const EditorTheme& theme)
    {
        EditorWidgetSkin skin{};
        skin.rowHeight = theme.metrics.rowHeight;
        skin.headerHeight = theme.metrics.headerHeight;
        skin.tabHeight = theme.metrics.tabHeight;
        skin.fieldHeight = theme.metrics.fieldHeight;
        skin.iconSize = theme.metrics.iconSize;
        skin.padding = theme.metrics.padding;
        skin.indent = theme.metrics.iconSize;
        skin.inspectorLabelWidth = theme.metrics.inspectorLabelWidth;
        skin.assetCellWidth = theme.metrics.assetCellWidth;
        skin.assetCellHeight = theme.metrics.assetCellHeight;
        return skin;
    }

    EditorWidgetSkin MakeDefaultEditorWidgetSkin()
    {
        return MakeEditorWidgetSkinFromTheme(BuildDefaultEditorTheme());
    }

    std::vector<EditorWidgetDesc> BuildHierarchyTreeWidgets(EditorRect body, const std::vector<std::string>& hierarchyLines, const EditorWidgetSkin& skin)
    {
        std::vector<EditorWidgetDesc> widgets;
        AddWidget(widgets, EditorWidgetKind::SearchBox, EditorIconKind::Search, MakeRect(body.x + skin.padding, body.y + skin.padding, body.width - skin.padding * 2, skin.fieldHeight + 3), "hierarchy.search", "Search", "Search Hierarchy");
        AddWidget(widgets, EditorWidgetKind::TreeRow, EditorIconKind::Scene, MakeRect(body.x, body.y + 34, body.width, skin.rowHeight), "hierarchy.scene", "Sandbox", "Scene", EditorWidgetState::Expanded, 0);

        const i32 firstY = body.y + 34 + skin.rowHeight;
        const std::size_t maxRows = body.height > 60 ? static_cast<std::size_t>((body.height - 60) / skin.rowHeight) : 0;
        const std::size_t count = std::min(hierarchyLines.size(), maxRows);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::string clean = CleanUnityLinePrefix(hierarchyLines[i]);
            const bool selected = hierarchyLines[i].rfind("> ", 0) == 0;
            const i32 rowY = firstY + static_cast<i32>(i) * skin.rowHeight;
            AddWidget(widgets, EditorWidgetKind::TreeRow, GuessHierarchyIcon(clean), MakeRect(body.x, rowY, body.width, skin.rowHeight), "hierarchy.row." + std::to_string(i), clean, {}, selected ? EditorWidgetState::Selected : EditorWidgetState::Normal, 1);
        }
        return widgets;
    }

    std::vector<EditorWidgetDesc> BuildInspectorPropertyWidgets(EditorRect body, const std::string& selectedName, const std::vector<std::string>& inspectorLines, const EditorWidgetSkin& skin)
    {
        std::vector<EditorWidgetDesc> widgets;
        AddWidget(widgets, EditorWidgetKind::InspectorObjectHeader, EditorIconKind::Entity, MakeRect(body.x, body.y, body.width, 68), "inspector.object", selectedName.empty() ? "None" : selectedName, "Tag: Untagged | Layer: Default", EditorWidgetState::Active);
        i32 y = body.y + 76;
        AddWidget(widgets, EditorWidgetKind::ComponentHeader, EditorIconKind::Transform, MakeRect(body.x + 6, y, body.width - 12, skin.headerHeight), "component.transform", "Transform", {}, EditorWidgetState::Expanded);
        y += skin.headerHeight + 8;
        AddVector3Field(widgets, MakeRect(body.x + 12, y, body.width - 24, skin.fieldHeight), "transform.position", "Position", "0.00", skin);
        y += skin.rowHeight;
        AddVector3Field(widgets, MakeRect(body.x + 12, y, body.width - 24, skin.fieldHeight), "transform.rotation", "Rotation", "0.00", skin);
        y += skin.rowHeight;
        AddVector3Field(widgets, MakeRect(body.x + 12, y, body.width - 24, skin.fieldHeight), "transform.scale", "Scale", "1.00", skin);
        y += skin.rowHeight + 10;

        bool meshAdded = false;
        bool boundsAdded = false;
        bool cameraAdded = false;
        bool lightAdded = false;
        for (const std::string& line : inspectorLines)
        {
            if (!meshAdded && line.find("Mesh") != std::string::npos)
            {
                AddWidget(widgets, EditorWidgetKind::ComponentHeader, EditorIconKind::Mesh, MakeRect(body.x + 6, y, body.width - 12, skin.headerHeight), "component.mesh", "Mesh Renderer", {}, EditorWidgetState::Expanded);
                y += skin.headerHeight + 5;
                AddWidget(widgets, EditorWidgetKind::TextField, EditorIconKind::None, MakeRect(body.x + 12, y, body.width - 24, skin.fieldHeight), "mesh.asset", "Mesh", "builtin:cube");
                y += skin.rowHeight + 8;
                meshAdded = true;
            }
            if (!cameraAdded && line.find("Camera") != std::string::npos)
            {
                AddWidget(widgets, EditorWidgetKind::ComponentHeader, EditorIconKind::Camera, MakeRect(body.x + 6, y, body.width - 12, skin.headerHeight), "component.camera", "Camera", {}, EditorWidgetState::Expanded);
                y += skin.headerHeight + 5;
                AddWidget(widgets, EditorWidgetKind::NumericField, EditorIconKind::None, MakeRect(body.x + 12, y, body.width - 24, skin.fieldHeight), "camera.fov", "Field of View", "60");
                y += skin.rowHeight + 8;
                cameraAdded = true;
            }
            if (!lightAdded && line.find("Light") != std::string::npos)
            {
                AddWidget(widgets, EditorWidgetKind::ComponentHeader, EditorIconKind::Light, MakeRect(body.x + 6, y, body.width - 12, skin.headerHeight), "component.light", "Light", {}, EditorWidgetState::Expanded);
                y += skin.headerHeight + 5;
                AddWidget(widgets, EditorWidgetKind::NumericField, EditorIconKind::None, MakeRect(body.x + 12, y, body.width - 24, skin.fieldHeight), "light.intensity", "Intensity", "1.00");
                y += skin.rowHeight + 8;
                lightAdded = true;
            }
            if (!boundsAdded && line.find("Bounds") != std::string::npos)
            {
                AddWidget(widgets, EditorWidgetKind::ComponentHeader, EditorIconKind::Bounds, MakeRect(body.x + 6, y, body.width - 12, skin.headerHeight), "component.bounds", "Bounds", {}, EditorWidgetState::Expanded);
                y += skin.headerHeight + 5;
                AddWidget(widgets, EditorWidgetKind::TextField, EditorIconKind::None, MakeRect(body.x + 12, y, body.width - 24, skin.fieldHeight), "bounds.world", "World AABB", "valid");
                y += skin.rowHeight + 8;
                boundsAdded = true;
            }
        }

        AddWidget(widgets, EditorWidgetKind::Button, EditorIconKind::Plus, MakeRect(body.x + 18, body.height + body.y - 30, body.width - 36, 22), "inspector.addComponent", "Add Component", {}, EditorWidgetState::Normal, 0);
        return widgets;
    }

    std::vector<EditorWidgetDesc> BuildProjectBrowserWidgets(EditorRect body, const std::vector<std::string>& assetLines, const EditorWidgetSkin& skin)
    {
        std::vector<EditorWidgetDesc> widgets;
        const i32 treeWidth = std::min<i32>(180, std::max<i32>(118, body.width / 4));
        AddWidget(widgets, EditorWidgetKind::SearchBox, EditorIconKind::Search, MakeRect(body.x + skin.padding, body.y + skin.padding, body.width - skin.padding * 2, skin.fieldHeight + 3), "project.search", "Search", "Search Project");
        AddWidget(widgets, EditorWidgetKind::TreeRow, EditorIconKind::Folder, MakeRect(body.x + 4, body.y + 36, treeWidth - 8, skin.rowHeight), "project.favorites", "Favorites", {}, EditorWidgetState::Expanded, 0);
        AddWidget(widgets, EditorWidgetKind::TreeRow, EditorIconKind::Folder, MakeRect(body.x + 4, body.y + 36 + skin.rowHeight, treeWidth - 8, skin.rowHeight), "project.assets", "Assets", {}, EditorWidgetState::Selected | EditorWidgetState::Expanded, 0);
        AddWidget(widgets, EditorWidgetKind::Separator, EditorIconKind::None, MakeRect(body.x + treeWidth, body.y + 34, 1, body.height - 40), "project.split", "");

        const i32 gridX = body.x + treeWidth + skin.padding;
        const i32 gridY = body.y + 38;
        const i32 cols = std::max<i32>(1, (body.width - treeWidth - skin.padding * 2) / skin.assetCellWidth);
        const std::size_t count = std::min<std::size_t>(assetLines.size(), 64);
        for (std::size_t i = 0; i < count; ++i)
        {
            const i32 col = static_cast<i32>(i) % cols;
            const i32 row = static_cast<i32>(i) / cols;
            const EditorRect cell = MakeRect(gridX + col * skin.assetCellWidth, gridY + row * skin.assetCellHeight, skin.assetCellWidth - 6, skin.assetCellHeight - 6);
            AddWidget(widgets, EditorWidgetKind::AssetGridItem, GuessAssetIcon(assetLines[i]), cell, "project.asset." + std::to_string(i), assetLines[i], {}, EditorWidgetState::Normal);
        }
        return widgets;
    }

    std::vector<EditorWidgetDesc> BuildSceneViewToolbarWidgets(EditorRect body, const EditorWidgetSkin& skin)
    {
        std::vector<EditorWidgetDesc> widgets;
        const i32 y = body.y + 6;
        i32 x = body.x + 8;
        AddWidget(widgets, EditorWidgetKind::Dropdown, EditorIconKind::None, MakeRect(x, y, 66, 22), "scene.pivot", "Pivot", "Center");
        x += 72;
        AddWidget(widgets, EditorWidgetKind::Dropdown, EditorIconKind::None, MakeRect(x, y, 70, 22), "scene.space", "Global", "Local");
        x += 78;
        AddWidget(widgets, EditorWidgetKind::Toggle, EditorIconKind::None, MakeRect(x, y, 42, 22), "scene.2d", "2D", {}, EditorWidgetState::Normal);
        x += 48;
        AddWidget(widgets, EditorWidgetKind::Dropdown, EditorIconKind::None, MakeRect(x, y, 86, 22), "scene.shading", "Shaded", {});
        AddWidget(widgets, EditorWidgetKind::Dropdown, EditorIconKind::None, MakeRect(body.x + body.width - 84, y, 76, 22), "scene.gizmos", "Gizmos", {});
        (void)skin;
        return widgets;
    }

    EditorContextMenuModel BuildDefaultHierarchyContextMenu(EditorRect anchor)
    {
        EditorContextMenuModel menu{};
        menu.id = "hierarchy.context";
        menu.anchor = anchor;
        menu.open = true;
        menu.items.push_back(MakeContextMenuItem("cut", "Cut"));
        menu.items.push_back(MakeContextMenuItem("copy", "Copy"));
        menu.items.push_back(MakeContextMenuItem("paste", "Paste"));
        menu.items.push_back(MakeContextMenuItem("rename", "Rename", EditorIconKind::None, true));
        menu.items.push_back(MakeContextMenuItem("duplicate", "Duplicate"));
        menu.items.push_back(MakeContextMenuItem("delete", "Delete", EditorIconKind::Error, false, true, false, true));
        EditorContextMenuItem create{};
        create.id = "create";
        create.label = "Create Empty";
        create.icon = EditorIconKind::Plus;
        create.separatorBefore = true;
        create.children.push_back(MakeContextMenuItem("create.empty", "Empty GameObject", EditorIconKind::Entity));
        create.children.push_back(MakeContextMenuItem("create.cube", "3D Object / Cube", EditorIconKind::Mesh));
        create.children.push_back(MakeContextMenuItem("create.camera", "Camera", EditorIconKind::Camera));
        create.children.push_back(MakeContextMenuItem("create.light", "Light", EditorIconKind::Light));
        menu.items.push_back(std::move(create));
        return menu;
    }

    EditorPopupModel BuildDefaultSceneStatsPopup(EditorRect anchor)
    {
        EditorPopupModel popup{};
        popup.id = "scene.stats";
        popup.title = "Stats";
        popup.anchor = anchor;
        popup.open = true;
        AddWidget(popup.rows, EditorWidgetKind::Label, EditorIconKind::Diagnostics, MakeRect(anchor.x, anchor.y, 220, 20), "stats.graphics", "Graphics", "batches / tris / verts");
        AddWidget(popup.rows, EditorWidgetKind::Label, EditorIconKind::Diagnostics, MakeRect(anchor.x, anchor.y + 22, 220, 20), "stats.cpu", "CPU", "main / render thread");
        AddWidget(popup.rows, EditorWidgetKind::Label, EditorIconKind::Diagnostics, MakeRect(anchor.x, anchor.y + 44, 220, 20), "stats.memory", "Memory", "frame / assets / gpu planned");
        return popup;
    }

    EditorWidgetFrame BuildDefaultEditorWidgetFrame(i32 width, i32 height)
    {
        EditorWidgetFrame frame{};
        frame.skin = MakeDefaultEditorWidgetSkin();
        frame.hierarchyWidgets = BuildHierarchyTreeWidgets(MakeRect(0, 0, 260, height - 220), {"[E] SampleCube", "[C] EditorCamera", "[L] DirectionalLight"}, frame.skin);
        frame.inspectorWidgets = BuildInspectorPropertyWidgets(MakeRect(width - 340, 0, 340, height - 220), "SampleCube", {"TransformComponent", "MeshComponent", "BoundsComponent"}, frame.skin);
        frame.projectWidgets = BuildProjectBrowserWidgets(MakeRect(0, height - 190, width, 170), {"Assets", "readme.txt", "shaders/ak_primitive_mesh.slang", "builtin:cube", "material:default"}, frame.skin);
        frame.sceneToolbarWidgets = BuildSceneViewToolbarWidgets(MakeRect(270, 50, width - 620, 34), frame.skin);
        frame.contextMenus.push_back(BuildDefaultHierarchyContextMenu(MakeRect(42, 128, 210, 220)));
        frame.popups.push_back(BuildDefaultSceneStatsPopup(MakeRect(width - 450, 92, 220, 90)));
        return frame;
    }

    EditorWidgetDiagnostics ValidateEditorWidgetFrame(const EditorWidgetFrame& frame)
    {
        EditorWidgetDiagnostics diagnostics{};
        diagnostics.hierarchyWidgetCount = frame.hierarchyWidgets.size();
        diagnostics.inspectorWidgetCount = frame.inspectorWidgets.size();
        diagnostics.projectWidgetCount = frame.projectWidgets.size();
        diagnostics.sceneToolbarWidgetCount = frame.sceneToolbarWidgets.size();
        diagnostics.contextMenuCount = frame.contextMenus.size();
        diagnostics.popupCount = frame.popups.size();

        std::unordered_set<std::string> ids;
        auto validateWidget = [&](const EditorWidgetDesc& widget)
        {
            if (widget.id.empty())
            {
                ++diagnostics.emptyIdCount;
            }
            if (!RectValid(widget.rect))
            {
                ++diagnostics.invalidRectCount;
            }
            if (!widget.id.empty())
            {
                ids.insert(widget.id);
            }
        };

        for (const EditorWidgetDesc& widget : frame.hierarchyWidgets) validateWidget(widget);
        for (const EditorWidgetDesc& widget : frame.inspectorWidgets) validateWidget(widget);
        for (const EditorWidgetDesc& widget : frame.projectWidgets) validateWidget(widget);
        for (const EditorWidgetDesc& widget : frame.sceneToolbarWidgets) validateWidget(widget);
        for (const EditorPopupModel& popup : frame.popups)
        {
            for (const EditorWidgetDesc& widget : popup.rows) validateWidget(widget);
        }

        for (const EditorContextMenuModel& menu : frame.contextMenus)
        {
            if (menu.id.empty())
            {
                ++diagnostics.emptyIdCount;
            }
            if (!RectValid(menu.anchor))
            {
                ++diagnostics.invalidRectCount;
            }
        }

        diagnostics.ok = diagnostics.invalidRectCount == 0 && diagnostics.emptyIdCount == 0
            && diagnostics.hierarchyWidgetCount > 0 && diagnostics.inspectorWidgetCount > 0
            && diagnostics.projectWidgetCount > 0 && diagnostics.sceneToolbarWidgetCount > 0;
        diagnostics.summary = FormatEditorWidgetDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorWidget(const EditorWidgetDesc& widget)
    {
        std::ostringstream out;
        out << ToString(widget.kind) << " id=" << widget.id << " label=" << widget.label << " icon=" << ToString(widget.icon)
            << " rect=" << FormatEditorRect(widget.rect) << " state=" << ToString(widget.state);
        return out.str();
    }

    std::string FormatEditorWidgetDiagnostics(const EditorWidgetDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-widgets hierarchy=" << diagnostics.hierarchyWidgetCount
            << " inspector=" << diagnostics.inspectorWidgetCount
            << " project=" << diagnostics.projectWidgetCount
            << " sceneToolbar=" << diagnostics.sceneToolbarWidgetCount
            << " contextMenus=" << diagnostics.contextMenuCount
            << " popups=" << diagnostics.popupCount
            << " invalidRects=" << diagnostics.invalidRectCount
            << " emptyIds=" << diagnostics.emptyIdCount
            << " ok=" << (diagnostics.ok ? "true" : "false");
        return out.str();
    }
}
