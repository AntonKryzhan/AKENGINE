#include <AK/EditorUI/EditorDragDrop.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        bool RectUsable(EditorRect rect)
        {
            return rect.width > 0 && rect.height > 0;
        }

        bool RectContains(EditorRect rect, i32 x, i32 y)
        {
            return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
        }

        u64 HashStringStable(std::string_view text)
        {
            u64 hash = 1469598103934665603ull;
            for (char c : text)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        bool PayloadKindAccepted(EditorDragPayloadKind kind, const std::vector<EditorDragPayloadKind>& accepted)
        {
            return std::find(accepted.begin(), accepted.end(), kind) != accepted.end();
        }

        std::string DefaultDropMessage(EditorDropOperation operation, const EditorDragPayload& payload, const EditorDropTarget& target)
        {
            std::ostringstream out;
            out << ToString(operation) << " '" << payload.label << "' -> " << ToString(target.kind) << " '" << target.label << "'";
            return out.str();
        }

        EditorDropTarget MakeTarget(EditorDropTargetKind kind, EditorPanelId panel, const char* id, const char* label, EditorRect rect, EditorDropOperation operation, EditorDropVisual visual, std::initializer_list<EditorDragPayloadKind> accepted)
        {
            return MakeEditorDropTarget(kind, panel, id, label, rect, operation, visual, std::vector<EditorDragPayloadKind>(accepted));
        }
    }

    const char* ToString(EditorDragPayloadKind kind)
    {
        switch (kind)
        {
            case EditorDragPayloadKind::None: return "None";
            case EditorDragPayloadKind::Entity: return "Entity";
            case EditorDragPayloadKind::Asset: return "Asset";
            case EditorDragPayloadKind::Component: return "Component";
            case EditorDragPayloadKind::DockTab: return "DockTab";
            case EditorDragPayloadKind::Property: return "Property";
            case EditorDragPayloadKind::FilePath: return "FilePath";
        }
        return "None";
    }

    const char* ToString(EditorDragSourceKind kind)
    {
        switch (kind)
        {
            case EditorDragSourceKind::None: return "None";
            case EditorDragSourceKind::Hierarchy: return "Hierarchy";
            case EditorDragSourceKind::ProjectBrowser: return "ProjectBrowser";
            case EditorDragSourceKind::Inspector: return "Inspector";
            case EditorDragSourceKind::DockTab: return "DockTab";
            case EditorDragSourceKind::SceneView: return "SceneView";
            case EditorDragSourceKind::ExternalFile: return "ExternalFile";
        }
        return "None";
    }

    const char* ToString(EditorDropTargetKind kind)
    {
        switch (kind)
        {
            case EditorDropTargetKind::None: return "None";
            case EditorDropTargetKind::HierarchyRow: return "HierarchyRow";
            case EditorDropTargetKind::HierarchyRoot: return "HierarchyRoot";
            case EditorDropTargetKind::SceneView: return "SceneView";
            case EditorDropTargetKind::InspectorComponent: return "InspectorComponent";
            case EditorDropTargetKind::InspectorProperty: return "InspectorProperty";
            case EditorDropTargetKind::ProjectFolder: return "ProjectFolder";
            case EditorDropTargetKind::ProjectGrid: return "ProjectGrid";
            case EditorDropTargetKind::DockStack: return "DockStack";
            case EditorDropTargetKind::DockSplit: return "DockSplit";
            case EditorDropTargetKind::Console: return "Console";
        }
        return "None";
    }

    const char* ToString(EditorDropOperation operation)
    {
        switch (operation)
        {
            case EditorDropOperation::None: return "None";
            case EditorDropOperation::ReorderEntity: return "ReorderEntity";
            case EditorDropOperation::ParentEntity: return "ParentEntity";
            case EditorDropOperation::InstantiateAsset: return "InstantiateAsset";
            case EditorDropOperation::AssignAsset: return "AssignAsset";
            case EditorDropOperation::AssignMaterial: return "AssignMaterial";
            case EditorDropOperation::AddComponent: return "AddComponent";
            case EditorDropOperation::MoveAsset: return "MoveAsset";
            case EditorDropOperation::OpenAsset: return "OpenAsset";
            case EditorDropOperation::DockPanel: return "DockPanel";
            case EditorDropOperation::CopyPath: return "CopyPath";
        }
        return "None";
    }

    const char* ToString(EditorDragPhase phase)
    {
        switch (phase)
        {
            case EditorDragPhase::Idle: return "Idle";
            case EditorDragPhase::Pending: return "Pending";
            case EditorDragPhase::Dragging: return "Dragging";
            case EditorDragPhase::Dropped: return "Dropped";
            case EditorDragPhase::Cancelled: return "Cancelled";
            case EditorDragPhase::Rejected: return "Rejected";
        }
        return "Idle";
    }

    const char* ToString(EditorDropVisual visual)
    {
        switch (visual)
        {
            case EditorDropVisual::None: return "None";
            case EditorDropVisual::InsertBefore: return "InsertBefore";
            case EditorDropVisual::InsertAfter: return "InsertAfter";
            case EditorDropVisual::Into: return "Into";
            case EditorDropVisual::Replace: return "Replace";
            case EditorDropVisual::PreviewGhost: return "PreviewGhost";
            case EditorDropVisual::Forbidden: return "Forbidden";
        }
        return "None";
    }

    EditorDragPayload MakeEntityDragPayload(std::string id, std::string label, u64 stableToken)
    {
        EditorDragPayload payload{};
        payload.kind = EditorDragPayloadKind::Entity;
        payload.id = std::move(id);
        payload.label = std::move(label);
        payload.type = "entity";
        payload.icon = EditorIconKind::Entity;
        payload.stableToken = stableToken != 0 ? stableToken : HashStringStable(payload.id + payload.label + payload.type);
        payload.valid = !payload.id.empty();
        return payload;
    }

    EditorDragPayload MakeAssetDragPayload(std::string id, std::string label, std::string type, u64 stableToken)
    {
        EditorDragPayload payload{};
        payload.kind = EditorDragPayloadKind::Asset;
        payload.id = std::move(id);
        payload.label = std::move(label);
        payload.type = std::move(type);
        payload.icon = payload.type == "material" ? EditorIconKind::Material : payload.type == "mesh" ? EditorIconKind::Mesh : payload.type == "scene" ? EditorIconKind::Scene : EditorIconKind::Asset;
        payload.stableToken = stableToken != 0 ? stableToken : HashStringStable(payload.id + payload.label + payload.type);
        payload.valid = !payload.id.empty();
        return payload;
    }

    EditorDragPayload MakeComponentDragPayload(std::string id, std::string label, std::string type, u64 stableToken)
    {
        EditorDragPayload payload{};
        payload.kind = EditorDragPayloadKind::Component;
        payload.id = std::move(id);
        payload.label = std::move(label);
        payload.type = std::move(type);
        payload.icon = payload.type == "camera" ? EditorIconKind::Camera : payload.type == "light" ? EditorIconKind::Light : payload.type == "bounds" ? EditorIconKind::Bounds : EditorIconKind::Transform;
        payload.stableToken = stableToken != 0 ? stableToken : HashStringStable(payload.id + payload.label + payload.type);
        payload.valid = !payload.id.empty();
        return payload;
    }

    EditorDragPayload MakeDockTabDragPayload(EditorPanelId panel, std::string label)
    {
        EditorDragPayload payload{};
        payload.kind = EditorDragPayloadKind::DockTab;
        payload.id = std::to_string(panel.value);
        payload.label = std::move(label);
        payload.type = "dock_tab";
        payload.icon = EditorIconKind::None;
        payload.stableToken = HashStringStable(payload.id + payload.label + payload.type);
        payload.valid = panel.IsValid();
        return payload;
    }

    EditorDragSource MakeEditorDragSource(EditorDragSourceKind kind, EditorPanelId panel, std::string widgetId, EditorRect rect, i32 originX, i32 originY)
    {
        EditorDragSource source{};
        source.kind = kind;
        source.panel = panel;
        source.widgetId = std::move(widgetId);
        source.rect = rect;
        source.originX = originX;
        source.originY = originY;
        return source;
    }

    EditorDropTarget MakeEditorDropTarget(EditorDropTargetKind kind, EditorPanelId panel, std::string id, std::string label, EditorRect rect, EditorDropOperation operation, EditorDropVisual visual, std::vector<EditorDragPayloadKind> acceptedPayloads)
    {
        EditorDropTarget target{};
        target.kind = kind;
        target.panel = panel;
        target.id = std::move(id);
        target.label = std::move(label);
        target.rect = rect;
        target.operation = operation;
        target.visual = visual;
        target.acceptedPayloads = std::move(acceptedPayloads);
        target.enabled = RectUsable(rect) && !target.id.empty();
        return target;
    }

    EditorDragDropState MakeDefaultEditorDragDropState(i32 thresholdPixels)
    {
        EditorDragDropState state{};
        state.thresholdPixels = std::max<i32>(1, thresholdPixels);
        return state;
    }

    bool BeginEditorDrag(EditorDragDropState& state, EditorDragPayload payload, EditorDragSource source, i32 x, i32 y)
    {
        if (!payload.valid || source.kind == EditorDragSourceKind::None)
        {
            return false;
        }
        state.phase = EditorDragPhase::Pending;
        state.payload = std::move(payload);
        state.source = std::move(source);
        state.currentX = x;
        state.currentY = y;
        state.hoveredTarget = {};
        state.preview = {};
        state.mouseCaptured = true;
        state.dropAccepted = false;
        state.dropRejected = false;
        ++state.revision;
        return true;
    }

    bool CanEditorDropPayloadOnTarget(const EditorDragPayload& payload, const EditorDropTarget& target)
    {
        if (!payload.valid || !target.enabled || target.operation == EditorDropOperation::None)
        {
            return false;
        }
        if (!PayloadKindAccepted(payload.kind, target.acceptedPayloads))
        {
            return false;
        }
        if (payload.kind == EditorDragPayloadKind::Asset && target.operation == EditorDropOperation::AssignMaterial && payload.type != "material")
        {
            return false;
        }
        if (payload.kind == EditorDragPayloadKind::Asset && target.operation == EditorDropOperation::InstantiateAsset && !(payload.type == "mesh" || payload.type == "prefab" || payload.type == "scene"))
        {
            return false;
        }
        return true;
    }

    const EditorDropTarget* HitTestEditorDropTargets(const std::vector<EditorDropTarget>& targets, i32 x, i32 y)
    {
        for (auto it = targets.rbegin(); it != targets.rend(); ++it)
        {
            if (it->enabled && RectContains(it->rect, x, y))
            {
                return &(*it);
            }
        }
        return nullptr;
    }

    EditorDragPreview BuildEditorDragPreview(const EditorDragDropState& state)
    {
        EditorDragPreview preview{};
        if (state.phase != EditorDragPhase::Dragging && state.phase != EditorDragPhase::Pending)
        {
            return preview;
        }
        preview.rect = {state.currentX + 14, state.currentY + 12, 192, 26};
        preview.icon = state.payload.icon;
        preview.label = state.payload.label.empty() ? state.payload.id : state.payload.label;
        preview.allowed = CanEditorDropPayloadOnTarget(state.payload, state.hoveredTarget);
        preview.visual = preview.allowed ? state.hoveredTarget.visual : EditorDropVisual::Forbidden;
        preview.visible = true;

        if (preview.allowed && (state.hoveredTarget.visual == EditorDropVisual::InsertBefore || state.hoveredTarget.visual == EditorDropVisual::InsertAfter))
        {
            const i32 y = state.hoveredTarget.visual == EditorDropVisual::InsertBefore ? state.hoveredTarget.rect.y : state.hoveredTarget.rect.y + state.hoveredTarget.rect.height;
            preview.insertionLine = {state.hoveredTarget.rect.x, y, state.hoveredTarget.rect.width, 2};
        }
        return preview;
    }

    bool UpdateEditorDrag(EditorDragDropState& state, i32 x, i32 y, const std::vector<EditorDropTarget>& targets)
    {
        if (state.phase == EditorDragPhase::Idle || state.phase == EditorDragPhase::Dropped || state.phase == EditorDragPhase::Cancelled)
        {
            return false;
        }

        const i32 dx = x - state.source.originX;
        const i32 dy = y - state.source.originY;
        const i32 distanceSquared = dx * dx + dy * dy;
        const i32 thresholdSquared = state.thresholdPixels * state.thresholdPixels;
        state.currentX = x;
        state.currentY = y;

        bool changed = false;
        if (state.phase == EditorDragPhase::Pending && distanceSquared >= thresholdSquared)
        {
            state.phase = EditorDragPhase::Dragging;
            changed = true;
        }

        if (state.phase == EditorDragPhase::Dragging)
        {
            const EditorDropTarget* hit = HitTestEditorDropTargets(targets, x, y);
            EditorDropTarget next = hit ? *hit : EditorDropTarget{};
            if (next.id != state.hoveredTarget.id || next.kind != state.hoveredTarget.kind)
            {
                state.hoveredTarget = std::move(next);
                changed = true;
            }
            state.preview = BuildEditorDragPreview(state);
        }

        if (changed)
        {
            ++state.revision;
        }
        return changed;
    }

    CommandId CommandForEditorDropOperation(EditorDropOperation operation)
    {
        switch (operation)
        {
            case EditorDropOperation::InstantiateAsset: return CommandId::NewEntity;
            case EditorDropOperation::AssignMaterial: return CommandId::Count;
            case EditorDropOperation::AddComponent: return CommandId::NewEntity;
            case EditorDropOperation::MoveAsset: return CommandId::RescanAssets;
            case EditorDropOperation::OpenAsset: return CommandId::LoadScene;
            case EditorDropOperation::ReorderEntity: return CommandId::Count;
            case EditorDropOperation::ParentEntity: return CommandId::Count;
            case EditorDropOperation::DockPanel: return CommandId::Count;
            case EditorDropOperation::AssignAsset: return CommandId::Count;
            case EditorDropOperation::CopyPath: return CommandId::Count;
            case EditorDropOperation::None: return CommandId::Count;
        }
        return CommandId::Count;
    }

    EditorDropResult CompleteEditorDrop(EditorDragDropState& state, bool mouseReleased)
    {
        EditorDropResult result{};
        if (!mouseReleased || state.phase != EditorDragPhase::Dragging)
        {
            return result;
        }

        result.consumed = true;
        result.payload = state.payload;
        result.target = state.hoveredTarget;
        result.accepted = CanEditorDropPayloadOnTarget(state.payload, state.hoveredTarget);
        result.rejected = !result.accepted;
        result.operation = result.accepted ? state.hoveredTarget.operation : EditorDropOperation::None;
        result.message = result.accepted ? DefaultDropMessage(result.operation, state.payload, state.hoveredTarget) : "drop rejected";
        result.invocation.id = CommandForEditorDropOperation(result.operation);
        result.invocation.source = CommandSource::Programmatic;

        state.phase = result.accepted ? EditorDragPhase::Dropped : EditorDragPhase::Rejected;
        state.dropAccepted = result.accepted;
        state.dropRejected = result.rejected;
        state.mouseCaptured = false;
        state.preview = {};
        ++state.revision;
        return result;
    }

    bool CancelEditorDrag(EditorDragDropState& state)
    {
        if (state.phase == EditorDragPhase::Idle)
        {
            return false;
        }
        state.phase = EditorDragPhase::Cancelled;
        state.hoveredTarget = {};
        state.preview = {};
        state.mouseCaptured = false;
        state.dropAccepted = false;
        state.dropRejected = false;
        ++state.revision;
        return true;
    }

    std::vector<EditorDropTarget> BuildDefaultEditorDropTargets(EditorRect hierarchyBody, EditorRect sceneBody, EditorRect inspectorBody, EditorRect projectBody, EditorRect dockBody)
    {
        std::vector<EditorDropTarget> targets;
        targets.reserve(9);
        targets.push_back(MakeTarget(EditorDropTargetKind::DockStack, {3}, "dock.center", "Center Dock Stack", dockBody, EditorDropOperation::DockPanel, EditorDropVisual::Into, {EditorDragPayloadKind::DockTab}));
        targets.push_back(MakeTarget(EditorDropTargetKind::HierarchyRoot, {1}, "hierarchy.root", "Scene Root", {hierarchyBody.x + 8, hierarchyBody.y + 32, std::max<i32>(1, hierarchyBody.width - 16), 22}, EditorDropOperation::ParentEntity, EditorDropVisual::Into, {EditorDragPayloadKind::Entity}));
        targets.push_back(MakeTarget(EditorDropTargetKind::HierarchyRow, {1}, "hierarchy.row.0", "Entity Row", {hierarchyBody.x + 8, hierarchyBody.y + 58, std::max<i32>(1, hierarchyBody.width - 16), 22}, EditorDropOperation::ReorderEntity, EditorDropVisual::InsertAfter, {EditorDragPayloadKind::Entity}));
        targets.push_back(MakeTarget(EditorDropTargetKind::SceneView, {3}, "scene.viewport", "Scene View", sceneBody, EditorDropOperation::InstantiateAsset, EditorDropVisual::PreviewGhost, {EditorDragPayloadKind::Asset, EditorDragPayloadKind::FilePath}));
        targets.push_back(MakeTarget(EditorDropTargetKind::InspectorComponent, {5}, "inspector.components", "Inspector Components", {inspectorBody.x + 8, inspectorBody.y + 110, std::max<i32>(1, inspectorBody.width - 16), std::max<i32>(22, inspectorBody.height - 132)}, EditorDropOperation::AddComponent, EditorDropVisual::Into, {EditorDragPayloadKind::Component}));
        targets.push_back(MakeTarget(EditorDropTargetKind::InspectorProperty, {5}, "inspector.material", "Material Slot", {inspectorBody.x + 8, inspectorBody.y + 198, std::max<i32>(1, inspectorBody.width - 16), 24}, EditorDropOperation::AssignMaterial, EditorDropVisual::Replace, {EditorDragPayloadKind::Asset}));
        targets.push_back(MakeTarget(EditorDropTargetKind::ProjectFolder, {8}, "project.assets", "Assets Folder", {projectBody.x + 8, projectBody.y + 34, 180, std::max<i32>(22, projectBody.height - 42)}, EditorDropOperation::MoveAsset, EditorDropVisual::Into, {EditorDragPayloadKind::Asset, EditorDragPayloadKind::FilePath}));
        targets.push_back(MakeTarget(EditorDropTargetKind::ProjectGrid, {8}, "project.grid", "Project Grid", {projectBody.x + 196, projectBody.y + 34, std::max<i32>(1, projectBody.width - 204), std::max<i32>(22, projectBody.height - 42)}, EditorDropOperation::OpenAsset, EditorDropVisual::PreviewGhost, {EditorDragPayloadKind::Asset}));
        return targets;
    }

    EditorDragDropDiagnostics ValidateEditorDragDropState(const EditorDragDropState& state, const std::vector<EditorDropTarget>& targets)
    {
        EditorDragDropDiagnostics diagnostics{};
        diagnostics.phase = state.phase;
        diagnostics.operation = state.hoveredTarget.operation;
        diagnostics.targetCount = targets.size();
        for (const EditorDropTarget& target : targets)
        {
            if (!RectUsable(target.rect))
            {
                ++diagnostics.invalidTargetRectCount;
            }
            if (!target.acceptedPayloads.empty())
            {
                ++diagnostics.acceptingTargetCount;
            }
        }
        diagnostics.thresholdOk = state.thresholdPixels > 0;
        diagnostics.hierarchyDropOk = std::any_of(targets.begin(), targets.end(), [](const EditorDropTarget& target) { return target.kind == EditorDropTargetKind::HierarchyRow && PayloadKindAccepted(EditorDragPayloadKind::Entity, target.acceptedPayloads); });
        diagnostics.sceneDropOk = std::any_of(targets.begin(), targets.end(), [](const EditorDropTarget& target) { return target.kind == EditorDropTargetKind::SceneView && PayloadKindAccepted(EditorDragPayloadKind::Asset, target.acceptedPayloads); });
        diagnostics.inspectorDropOk = std::any_of(targets.begin(), targets.end(), [](const EditorDropTarget& target) { return target.kind == EditorDropTargetKind::InspectorComponent && PayloadKindAccepted(EditorDragPayloadKind::Component, target.acceptedPayloads); });
        diagnostics.projectDropOk = std::any_of(targets.begin(), targets.end(), [](const EditorDropTarget& target) { return target.kind == EditorDropTargetKind::ProjectFolder && PayloadKindAccepted(EditorDragPayloadKind::Asset, target.acceptedPayloads); });
        diagnostics.commandRouteOk = CommandForEditorDropOperation(EditorDropOperation::InstantiateAsset) == CommandId::NewEntity && CommandForEditorDropOperation(EditorDropOperation::MoveAsset) == CommandId::RescanAssets;
        diagnostics.ok = diagnostics.targetCount >= 7 && diagnostics.acceptingTargetCount == diagnostics.targetCount && diagnostics.invalidTargetRectCount == 0 && diagnostics.thresholdOk && diagnostics.hierarchyDropOk && diagnostics.sceneDropOk && diagnostics.inspectorDropOk && diagnostics.projectDropOk && diagnostics.commandRouteOk;
        diagnostics.summary = FormatEditorDragDropDiagnostics(diagnostics);
        return diagnostics;
    }

    EditorDragDropDiagnostics RunEditorDragDropDiagnostics()
    {
        const EditorRect hierarchy{0, 64, 280, 560};
        const EditorRect scene{288, 64, 890, 560};
        const EditorRect inspector{1186, 64, 340, 560};
        const EditorRect project{0, 632, 1526, 180};
        const EditorRect dock{288, 64, 890, 560};
        const std::vector<EditorDropTarget> targets = BuildDefaultEditorDropTargets(hierarchy, scene, inspector, project, dock);

        EditorDragDropState state = MakeDefaultEditorDragDropState();
        const EditorDragPayload mesh = MakeAssetDragPayload("asset.mesh.cube", "Cube Mesh", "mesh");
        BeginEditorDrag(state, mesh, MakeEditorDragSource(EditorDragSourceKind::ProjectBrowser, {8}, "asset.cube", {420, 690, 80, 70}, 424, 694), 424, 694);
        const bool stillPending = state.phase == EditorDragPhase::Pending;
        UpdateEditorDrag(state, 426, 696, targets);
        const bool thresholdHeld = stillPending && state.phase == EditorDragPhase::Pending;
        UpdateEditorDrag(state, scene.x + 120, scene.y + 120, targets);
        EditorDropResult sceneDrop = CompleteEditorDrop(state, true);

        EditorDragDropState componentDrag = MakeDefaultEditorDragDropState();
        BeginEditorDrag(componentDrag, MakeComponentDragPayload("component.light", "Light", "light"), MakeEditorDragSource(EditorDragSourceKind::Inspector, {5}, "component.light", {1200, 180, 320, 24}, 1204, 184), 1204, 184);
        UpdateEditorDrag(componentDrag, inspector.x + 60, inspector.y + 150, targets);
        EditorDropResult componentDrop = CompleteEditorDrop(componentDrag, true);

        EditorDragDropState entityDrag = MakeDefaultEditorDragDropState();
        BeginEditorDrag(entityDrag, MakeEntityDragPayload("entity.1", "Cube"), MakeEditorDragSource(EditorDragSourceKind::Hierarchy, {1}, "entity.cube", {8, 124, 260, 22}, 14, 130), 14, 130);
        UpdateEditorDrag(entityDrag, hierarchy.x + 48, hierarchy.y + 70, targets);
        EditorDropResult entityDrop = CompleteEditorDrop(entityDrag, true);

        EditorDragDropDiagnostics diagnostics = ValidateEditorDragDropState(state, targets);
        diagnostics.thresholdOk = diagnostics.thresholdOk && thresholdHeld;
        diagnostics.sceneDropOk = diagnostics.sceneDropOk && sceneDrop.accepted && sceneDrop.operation == EditorDropOperation::InstantiateAsset;
        diagnostics.inspectorDropOk = diagnostics.inspectorDropOk && componentDrop.accepted && componentDrop.operation == EditorDropOperation::AddComponent;
        diagnostics.hierarchyDropOk = diagnostics.hierarchyDropOk && entityDrop.accepted && entityDrop.operation == EditorDropOperation::ReorderEntity;
        diagnostics.ok = diagnostics.ok && diagnostics.thresholdOk && diagnostics.sceneDropOk && diagnostics.inspectorDropOk && diagnostics.hierarchyDropOk;
        diagnostics.summary = FormatEditorDragDropDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorDragPayload(const EditorDragPayload& payload)
    {
        std::ostringstream out;
        out << ToString(payload.kind) << ":" << payload.id << ":" << payload.label << ":" << payload.type;
        return out.str();
    }

    std::string FormatEditorDropTarget(const EditorDropTarget& target)
    {
        std::ostringstream out;
        out << ToString(target.kind) << ":" << target.id << ":" << target.label << ":" << ToString(target.operation) << ":" << FormatEditorRect(target.rect);
        return out.str();
    }

    std::string FormatEditorDragDropDiagnostics(const EditorDragDropDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-dragdrop phase=" << ToString(diagnostics.phase)
            << " op=" << ToString(diagnostics.operation)
            << " targets=" << diagnostics.targetCount
            << " accepting=" << diagnostics.acceptingTargetCount
            << " invalidRects=" << diagnostics.invalidTargetRectCount
            << " threshold=" << (diagnostics.thresholdOk ? "ok" : "bad")
            << " hierarchy=" << (diagnostics.hierarchyDropOk ? "ok" : "bad")
            << " scene=" << (diagnostics.sceneDropOk ? "ok" : "bad")
            << " inspector=" << (diagnostics.inspectorDropOk ? "ok" : "bad")
            << " project=" << (diagnostics.projectDropOk ? "ok" : "bad")
            << " commands=" << (diagnostics.commandRouteOk ? "ok" : "bad")
            << " ok=" << (diagnostics.ok ? "true" : "false");
        return out.str();
    }
}
