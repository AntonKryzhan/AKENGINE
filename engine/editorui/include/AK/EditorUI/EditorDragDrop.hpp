#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>
#include <AK/EditorUI/EditorWidgets.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorDragPayloadKind
    {
        None,
        Entity,
        Asset,
        Component,
        DockTab,
        Property,
        FilePath
    };

    enum class EditorDragSourceKind
    {
        None,
        Hierarchy,
        ProjectBrowser,
        Inspector,
        DockTab,
        SceneView,
        ExternalFile
    };

    enum class EditorDropTargetKind
    {
        None,
        HierarchyRow,
        HierarchyRoot,
        SceneView,
        InspectorComponent,
        InspectorProperty,
        ProjectFolder,
        ProjectGrid,
        DockStack,
        DockSplit,
        Console
    };

    enum class EditorDropOperation
    {
        None,
        ReorderEntity,
        ParentEntity,
        InstantiateAsset,
        AssignAsset,
        AssignMaterial,
        AddComponent,
        MoveAsset,
        OpenAsset,
        DockPanel,
        CopyPath
    };

    enum class EditorDragPhase
    {
        Idle,
        Pending,
        Dragging,
        Dropped,
        Cancelled,
        Rejected
    };

    enum class EditorDropVisual
    {
        None,
        InsertBefore,
        InsertAfter,
        Into,
        Replace,
        PreviewGhost,
        Forbidden
    };

    struct EditorDragPayload
    {
        EditorDragPayloadKind kind = EditorDragPayloadKind::None;
        std::string id;
        std::string label;
        std::string type;
        EditorIconKind icon = EditorIconKind::None;
        u64 stableToken = 0;
        bool valid = false;
    };

    struct EditorDragSource
    {
        EditorDragSourceKind kind = EditorDragSourceKind::None;
        EditorPanelId panel{};
        std::string widgetId;
        EditorRect rect{};
        i32 originX = 0;
        i32 originY = 0;
    };

    struct EditorDropTarget
    {
        EditorDropTargetKind kind = EditorDropTargetKind::None;
        EditorPanelId panel{};
        std::string id;
        std::string label;
        EditorRect rect{};
        std::vector<EditorDragPayloadKind> acceptedPayloads;
        EditorDropOperation operation = EditorDropOperation::None;
        EditorDropVisual visual = EditorDropVisual::None;
        bool enabled = true;
        bool destructive = false;
    };

    struct EditorDragPreview
    {
        EditorRect rect{};
        EditorRect insertionLine{};
        std::string label;
        EditorIconKind icon = EditorIconKind::None;
        EditorDropVisual visual = EditorDropVisual::None;
        bool allowed = false;
        bool visible = false;
    };

    struct EditorDragDropState
    {
        EditorDragPhase phase = EditorDragPhase::Idle;
        EditorDragPayload payload{};
        EditorDragSource source{};
        EditorDropTarget hoveredTarget{};
        EditorDragPreview preview{};
        i32 currentX = 0;
        i32 currentY = 0;
        i32 thresholdPixels = 5;
        bool mouseCaptured = false;
        bool dropAccepted = false;
        bool dropRejected = false;
        u64 revision = 1;
    };

    struct EditorDropResult
    {
        bool consumed = false;
        bool accepted = false;
        bool rejected = false;
        EditorDropOperation operation = EditorDropOperation::None;
        EditorDragPayload payload{};
        EditorDropTarget target{};
        CommandInvocation invocation{};
        std::string message;
    };

    struct EditorDragDropDiagnostics
    {
        EditorDragPhase phase = EditorDragPhase::Idle;
        EditorDropOperation operation = EditorDropOperation::None;
        std::size_t targetCount = 0;
        std::size_t acceptingTargetCount = 0;
        std::size_t invalidTargetRectCount = 0;
        bool thresholdOk = false;
        bool hierarchyDropOk = false;
        bool sceneDropOk = false;
        bool inspectorDropOk = false;
        bool projectDropOk = false;
        bool commandRouteOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorDragPayloadKind kind);
    const char* ToString(EditorDragSourceKind kind);
    const char* ToString(EditorDropTargetKind kind);
    const char* ToString(EditorDropOperation operation);
    const char* ToString(EditorDragPhase phase);
    const char* ToString(EditorDropVisual visual);

    EditorDragPayload MakeEntityDragPayload(std::string id, std::string label, u64 stableToken = 0);
    EditorDragPayload MakeAssetDragPayload(std::string id, std::string label, std::string type, u64 stableToken = 0);
    EditorDragPayload MakeComponentDragPayload(std::string id, std::string label, std::string type, u64 stableToken = 0);
    EditorDragPayload MakeDockTabDragPayload(EditorPanelId panel, std::string label);

    EditorDragSource MakeEditorDragSource(EditorDragSourceKind kind, EditorPanelId panel, std::string widgetId, EditorRect rect, i32 originX, i32 originY);
    EditorDropTarget MakeEditorDropTarget(EditorDropTargetKind kind, EditorPanelId panel, std::string id, std::string label, EditorRect rect, EditorDropOperation operation, EditorDropVisual visual, std::vector<EditorDragPayloadKind> acceptedPayloads);

    EditorDragDropState MakeDefaultEditorDragDropState(i32 thresholdPixels = 5);
    bool BeginEditorDrag(EditorDragDropState& state, EditorDragPayload payload, EditorDragSource source, i32 x, i32 y);
    bool UpdateEditorDrag(EditorDragDropState& state, i32 x, i32 y, const std::vector<EditorDropTarget>& targets);
    EditorDropResult CompleteEditorDrop(EditorDragDropState& state, bool mouseReleased);
    bool CancelEditorDrag(EditorDragDropState& state);

    bool CanEditorDropPayloadOnTarget(const EditorDragPayload& payload, const EditorDropTarget& target);
    const EditorDropTarget* HitTestEditorDropTargets(const std::vector<EditorDropTarget>& targets, i32 x, i32 y);
    EditorDragPreview BuildEditorDragPreview(const EditorDragDropState& state);
    CommandId CommandForEditorDropOperation(EditorDropOperation operation);

    std::vector<EditorDropTarget> BuildDefaultEditorDropTargets(EditorRect hierarchyBody, EditorRect sceneBody, EditorRect inspectorBody, EditorRect projectBody, EditorRect dockBody);
    EditorDragDropDiagnostics ValidateEditorDragDropState(const EditorDragDropState& state, const std::vector<EditorDropTarget>& targets);
    EditorDragDropDiagnostics RunEditorDragDropDiagnostics();
    std::string FormatEditorDragPayload(const EditorDragPayload& payload);
    std::string FormatEditorDropTarget(const EditorDropTarget& target);
    std::string FormatEditorDragDropDiagnostics(const EditorDragDropDiagnostics& diagnostics);
}
