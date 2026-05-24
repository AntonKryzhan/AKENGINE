#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorTransformGizmoMode
    {
        Translate,
        Rotate,
        Scale
    };

    enum class EditorTransformGizmoHandle
    {
        None,
        CenterPlaneXZ,
        AxisX,
        AxisY,
        AxisZ
    };

    enum class EditorTransformGizmoSpace
    {
        World,
        Local
    };

    struct EditorTransformGizmoViewport
    {
        EditorRect rect{};
        float centerX = 0.0f;
        float centerZ = 0.0f;
        float scale = 36.0f;
    };

    struct EditorTransformGizmoTarget
    {
        bool valid = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float rotationX = 0.0f;
        float rotationY = 0.0f;
        float rotationZ = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
        std::string label;
    };

    struct EditorTransformGizmoBuildInput
    {
        EditorTransformGizmoViewport viewport{};
        EditorTransformGizmoTarget target{};
        EditorTransformGizmoMode mode = EditorTransformGizmoMode::Translate;
        EditorTransformGizmoSpace space = EditorTransformGizmoSpace::World;
        bool snapEnabled = false;
        float snapStep = 0.5f;
    };

    struct EditorTransformGizmoHandleRect
    {
        EditorTransformGizmoHandle handle = EditorTransformGizmoHandle::None;
        EditorRect rect{};
        std::string label;
        bool visible = false;
    };

    struct EditorTransformGizmoSurface
    {
        bool visible = false;
        EditorTransformGizmoMode mode = EditorTransformGizmoMode::Translate;
        EditorTransformGizmoSpace space = EditorTransformGizmoSpace::World;
        EditorTransformGizmoTarget target{};
        i32 originScreenX = 0;
        i32 originScreenY = 0;
        float viewportScale = 36.0f;
        bool snapEnabled = false;
        float snapStep = 0.5f;
        std::vector<EditorTransformGizmoHandleRect> handles;
        std::string status;
    };

    struct EditorTransformGizmoRuntime
    {
        bool active = false;
        EditorTransformGizmoMode mode = EditorTransformGizmoMode::Translate;
        EditorTransformGizmoSpace space = EditorTransformGizmoSpace::World;
        EditorTransformGizmoHandle handle = EditorTransformGizmoHandle::None;
        i32 startMouseX = 0;
        i32 startMouseY = 0;
        i32 currentMouseX = 0;
        i32 currentMouseY = 0;
        float startX = 0.0f;
        float startY = 0.0f;
        float startZ = 0.0f;
        float startRotationX = 0.0f;
        float startRotationY = 0.0f;
        float startRotationZ = 0.0f;
        float startScaleX = 1.0f;
        float startScaleY = 1.0f;
        float startScaleZ = 1.0f;
        float startYawDegrees = 0.0f;
        float viewportScale = 36.0f;
        bool snapEnabled = false;
        float snapStep = 0.5f;
        u64 revision = 1;
    };

    struct EditorTransformGizmoDragResult
    {
        bool active = false;
        bool changed = false;
        EditorTransformGizmoMode mode = EditorTransformGizmoMode::Translate;
        EditorTransformGizmoSpace space = EditorTransformGizmoSpace::World;
        EditorTransformGizmoHandle handle = EditorTransformGizmoHandle::None;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float rotationX = 0.0f;
        float rotationY = 0.0f;
        float rotationZ = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
        float deltaX = 0.0f;
        float deltaY = 0.0f;
        float deltaZ = 0.0f;
        float deltaRotationX = 0.0f;
        float deltaRotationY = 0.0f;
        float deltaRotationZ = 0.0f;
        float deltaScaleX = 0.0f;
        float deltaScaleY = 0.0f;
        float deltaScaleZ = 0.0f;
        std::string status;
    };

    struct EditorTransformGizmoDiagnostics
    {
        bool ok = false;
        bool visible = false;
        std::size_t handleCount = 0;
        std::size_t visibleHandleCount = 0;
        bool axisHitOk = false;
        bool planeDragOk = false;
        bool snapOk = false;
        bool yDragOk = false;
        bool rotateDragOk = false;
        bool scaleDragOk = false;
        bool modeSurfaceOk = false;
        bool localSpaceOk = false;
        bool localHitOk = false;
        std::string summary;
    };

    const char* ToString(EditorTransformGizmoMode mode);
    const char* ToString(EditorTransformGizmoHandle handle);
    const char* ToString(EditorTransformGizmoSpace space);

    bool EditorTransformGizmoRectContains(EditorRect rect, i32 x, i32 y);
    EditorTransformGizmoSurface BuildEditorTransformGizmoSurface(const EditorTransformGizmoBuildInput& input);
    EditorTransformGizmoHandle HitTestEditorTransformGizmo(const EditorTransformGizmoSurface& surface, i32 x, i32 y);
    bool BeginEditorTransformGizmoDrag(EditorTransformGizmoRuntime& runtime, const EditorTransformGizmoSurface& surface, EditorTransformGizmoHandle handle, i32 mouseX, i32 mouseY);
    EditorTransformGizmoDragResult UpdateEditorTransformGizmoDrag(EditorTransformGizmoRuntime& runtime, i32 mouseX, i32 mouseY);
    void EndEditorTransformGizmoDrag(EditorTransformGizmoRuntime& runtime);

    EditorTransformGizmoDiagnostics RunEditorTransformGizmoDiagnostics();
    std::string FormatEditorTransformGizmoDiagnostics(const EditorTransformGizmoDiagnostics& diagnostics);
}
