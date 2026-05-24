#pragma once

#include <AK/Commands/CommandRegistry.hpp>
#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EditorSceneViewMode
    {
        Mode2D,
        Mode3D
    };

    enum class EditorSceneViewProjection
    {
        Orthographic,
        Perspective
    };

    enum class EditorSceneViewAxis
    {
        Top,
        Front,
        Right
    };

    struct EditorSceneViewCamera
    {
        EditorSceneViewMode mode = EditorSceneViewMode::Mode2D;
        EditorSceneViewProjection projection = EditorSceneViewProjection::Orthographic;
        float centerX = 0.0f;
        float focusY = 0.0f;
        float centerZ = 0.0f;
        float orthographicScale = 36.0f;
        float positionX = 0.0f;
        float positionY = 8.0f;
        float positionZ = -10.0f;
        float yawDegrees = 0.0f;
        float pitchDegrees = -35.0f;
        float fovYDegrees = 60.0f;
        float nearPlane = 0.05f;
        float farPlane = 10000.0f;
        float moveSpeed = 8.0f;
        u64 revision = 1;
    };

    struct EditorSceneViewRay
    {
        bool valid = false;
        float originX = 0.0f;
        float originY = 0.0f;
        float originZ = 0.0f;
        float dirX = 0.0f;
        float dirY = -1.0f;
        float dirZ = 0.0f;
    };

    struct EditorSceneViewGroundPoint
    {
        bool valid = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float distance = 0.0f;
    };

    struct EditorSceneViewBounds
    {
        bool valid = false;
        float minX = 0.0f;
        float minY = 0.0f;
        float minZ = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        float maxZ = 0.0f;
    };

    struct EditorSceneViewNavigationInput
    {
        EditorRect viewport{};
        i32 mouseX = 0;
        i32 mouseY = 0;
        i32 mouseDeltaX = 0;
        i32 mouseDeltaY = 0;
        i32 mouseWheelDelta = 0;
        bool pan2D = false;
        bool orbit3D = false;
        bool dolly3D = false;
        bool focusRequested = false;
        float focusX = 0.0f;
        float focusY = 0.0f;
        float focusZ = 0.0f;
        double deltaSeconds = 1.0 / 60.0;
    };

    struct EditorSceneViewNavigationResult
    {
        bool changed = false;
        bool modeChanged = false;
        std::string status;
    };

    enum class EditorSceneViewOverlayControl
    {
        None,
        Mode2D,
        Mode3D,
        GridSnap,
        ToolTranslate,
        ToolRotate,
        ToolScale,
        SpaceWorld,
        SpaceLocal,
        FocusSelection,
        FrameAll,
        ResetView,
        OrientationAxisX,
        OrientationAxisY,
        OrientationAxisZ
    };

    struct EditorSceneViewOverlayBuildInput
    {
        EditorRect viewport{};
        EditorSceneViewCamera camera{};
        CommandId activeTool = CommandId::ToolTranslate;
        CommandId activeSpace = CommandId::ToolSpaceWorld;
        bool gridVisible = true;
        bool snapEnabled = false;
        bool hasSelection = false;
        std::string selectedLabel;
    };

    struct EditorSceneViewOverlayControlDesc
    {
        EditorSceneViewOverlayControl control = EditorSceneViewOverlayControl::None;
        EditorRect rect{};
        std::string label;
        CommandId command = CommandId::CloseEditor;
        bool visible = false;
        bool enabled = false;
        bool active = false;
    };

    struct EditorSceneViewOverlaySurface
    {
        bool visible = false;
        EditorRect viewport{};
        EditorRect orientationRect{};
        std::vector<EditorSceneViewOverlayControlDesc> controls;
        std::string status;
    };

    struct EditorSceneViewOverlayHit
    {
        EditorSceneViewOverlayControl control = EditorSceneViewOverlayControl::None;
        CommandId command = CommandId::CloseEditor;
        std::size_t itemIndex = 0;
        bool actionable = false;
        bool active = false;
        std::string label;
    };

    struct EditorSceneViewDiagnostics
    {
        bool ok = false;
        bool modeSwitchOk = false;
        bool ray2DOk = false;
        bool ray3DOk = false;
        bool groundHitOk = false;
        bool navigationOk = false;
        bool focusOk = false;
        bool resetOk = false;
        bool frameOk = false;
        bool axisSnapOk = false;
        bool pickingStabilityOk = false;
        bool invalidInputOk = false;
        bool overlaySurfaceOk = false;
        bool overlayHitOk = false;
        bool overlayCommandOk = false;
        std::string summary;
    };

    const char* ToString(EditorSceneViewMode mode);
    const char* ToString(EditorSceneViewProjection projection);
    const char* ToString(EditorSceneViewAxis axis);
    const char* ToString(EditorSceneViewOverlayControl control);

    EditorSceneViewCamera MakeDefaultEditorSceneViewCamera();
    bool SanitizeEditorSceneViewCamera(EditorSceneViewCamera& camera);
    void SetEditorSceneViewMode(EditorSceneViewCamera& camera, EditorSceneViewMode mode);
    void ResetEditorSceneViewCamera(EditorSceneViewCamera& camera, EditorSceneViewMode mode);
    void FocusEditorSceneViewCamera(EditorSceneViewCamera& camera, float x, float y, float z);
    bool FrameEditorSceneViewCamera(EditorSceneViewCamera& camera, const EditorSceneViewBounds& bounds, EditorRect viewport);
    void SnapEditorSceneViewCameraToAxis(EditorSceneViewCamera& camera, EditorSceneViewAxis axis);
    EditorSceneViewRay BuildEditorSceneViewRay(const EditorSceneViewCamera& camera, EditorRect viewport, i32 screenX, i32 screenY);
    EditorSceneViewGroundPoint IntersectEditorSceneViewGroundPlane(const EditorSceneViewRay& ray, float planeY = 0.0f);
    EditorSceneViewGroundPoint ScreenToEditorSceneViewGroundPoint(const EditorSceneViewCamera& camera, EditorRect viewport, i32 screenX, i32 screenY, float planeY = 0.0f);
    EditorSceneViewNavigationResult ApplyEditorSceneViewNavigation(EditorSceneViewCamera& camera, const EditorSceneViewNavigationInput& input);
    EditorSceneViewOverlaySurface BuildEditorSceneViewOverlaySurface(const EditorSceneViewOverlayBuildInput& input);
    EditorSceneViewOverlayHit HitTestEditorSceneViewOverlay(const EditorSceneViewOverlaySurface& surface, i32 screenX, i32 screenY);

    EditorSceneViewDiagnostics RunEditorSceneViewDiagnostics();
    std::string FormatEditorSceneViewDiagnostics(const EditorSceneViewDiagnostics& diagnostics);
}
