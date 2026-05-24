#include <AK/EditorUI/EditorTransformGizmo.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr float MinViewportScale = 8.0f;
        constexpr float MaxViewportScale = 160.0f;
        constexpr float ChangeEpsilon = 0.0001f;
        constexpr float MinScaleValue = 0.01f;
        constexpr float RotationDegreesPerPixel = 0.5f;
        constexpr float Pi = 3.14159265358979323846f;

        struct GizmoAxis2D
        {
            float x = 1.0f;
            float z = 0.0f;
        };

        EditorRect MakeRect(i32 x, i32 y, i32 width, i32 height)
        {
            return {x, y, width, height};
        }

        i32 RectRight(EditorRect rect)
        {
            return rect.x + rect.width;
        }

        i32 RectBottom(EditorRect rect)
        {
            return rect.y + rect.height;
        }

        bool RectUsable(EditorRect rect)
        {
            return rect.width > 0 && rect.height > 0;
        }

        bool IsPointNearViewport(EditorRect viewport, i32 x, i32 y, i32 pad)
        {
            return x >= viewport.x - pad && x <= RectRight(viewport) + pad && y >= viewport.y - pad && y <= RectBottom(viewport) + pad;
        }


        float DegreesToRadians(float degrees)
        {
            return degrees * Pi / 180.0f;
        }

        GizmoAxis2D LocalXAxisFromYaw(float yawDegrees)
        {
            const float radians = DegreesToRadians(yawDegrees);
            return {std::cos(radians), std::sin(radians)};
        }

        GizmoAxis2D LocalZAxisFromYaw(float yawDegrees)
        {
            const float radians = DegreesToRadians(yawDegrees);
            return {-std::sin(radians), std::cos(radians)};
        }

        GizmoAxis2D AxisForHandle(EditorTransformGizmoHandle handle, EditorTransformGizmoSpace space, float yawDegrees)
        {
            if (space == EditorTransformGizmoSpace::Local)
            {
                if (handle == EditorTransformGizmoHandle::AxisX)
                {
                    return LocalXAxisFromYaw(yawDegrees);
                }
                if (handle == EditorTransformGizmoHandle::AxisZ)
                {
                    return LocalZAxisFromYaw(yawDegrees);
                }
            }

            if (handle == EditorTransformGizmoHandle::AxisZ)
            {
                return {0.0f, 1.0f};
            }
            return {1.0f, 0.0f};
        }

        i32 AxisEndpointScreenX(i32 originX, GizmoAxis2D axis, i32 axisLength)
        {
            return originX + static_cast<i32>(std::round(axis.x * static_cast<float>(axisLength)));
        }

        i32 AxisEndpointScreenY(i32 originY, GizmoAxis2D axis, i32 axisLength)
        {
            return originY - static_cast<i32>(std::round(axis.z * static_cast<float>(axisLength)));
        }

        float ApplySnap(float value, bool enabled, float step)
        {
            if (!enabled || step <= ChangeEpsilon || !std::isfinite(value))
            {
                return value;
            }

            return std::round(value / step) * step;
        }

        float ClampScale(float value)
        {
            if (!std::isfinite(value))
            {
                return 1.0f;
            }
            return std::max(MinScaleValue, value);
        }

        bool Changed(float a, float b)
        {
            return std::fabs(a - b) > ChangeEpsilon;
        }

        bool AnyTransformChanged(const EditorTransformGizmoDragResult& result, const EditorTransformGizmoRuntime& runtime)
        {
            return Changed(result.x, runtime.startX)
                || Changed(result.y, runtime.startY)
                || Changed(result.z, runtime.startZ)
                || Changed(result.rotationX, runtime.startRotationX)
                || Changed(result.rotationY, runtime.startRotationY)
                || Changed(result.rotationZ, runtime.startRotationZ)
                || Changed(result.scaleX, runtime.startScaleX)
                || Changed(result.scaleY, runtime.startScaleY)
                || Changed(result.scaleZ, runtime.startScaleZ);
        }

        std::string FormatDragStatus(const EditorTransformGizmoDragResult& result)
        {
            std::ostringstream out;
            out << "Transform gizmo " << ToString(result.mode) << '/' << ToString(result.space) << ' ' << ToString(result.handle);
            if (result.mode == EditorTransformGizmoMode::Rotate)
            {
                out << ": rot=(" << result.rotationX << ", " << result.rotationY << ", " << result.rotationZ << ")";
            }
            else if (result.mode == EditorTransformGizmoMode::Scale)
            {
                out << ": scale=(" << result.scaleX << ", " << result.scaleY << ", " << result.scaleZ << ")";
            }
            else
            {
                out << ": pos=(" << result.x << ", " << result.y << ", " << result.z << ")";
            }
            return out.str();
        }

        void AddHandle(EditorTransformGizmoSurface& surface, EditorTransformGizmoHandle handle, EditorRect rect, const char* label)
        {
            EditorTransformGizmoHandleRect out{};
            out.handle = handle;
            out.rect = rect;
            out.label = label ? label : "";
            out.visible = RectUsable(rect);
            surface.handles.push_back(out);
        }

        void AddAxisHandle(EditorTransformGizmoSurface& surface, EditorTransformGizmoHandle handle, i32 originX, i32 originY, i32 axisLength, i32 axisHit, i32 centerSize, const char* label)
        {
            const GizmoAxis2D axis = AxisForHandle(handle, surface.space, surface.target.rotationY);
            const i32 tipX = AxisEndpointScreenX(originX, axis, axisLength);
            const i32 tipY = AxisEndpointScreenY(originY, axis, axisLength);
            const i32 handleSize = std::max(centerSize, axisHit + 8);
            AddHandle(surface, handle, MakeRect(tipX - handleSize / 2, tipY - handleSize / 2, handleSize, handleSize), label);
        }

        void AddTranslateHandles(EditorTransformGizmoSurface& surface, i32 originX, i32 originY, i32 axisLength, i32 axisHit, i32 centerSize, i32 yHandleOffset)
        {
            AddHandle(surface, EditorTransformGizmoHandle::CenterPlaneXZ, MakeRect(originX - centerSize / 2, originY - centerSize / 2, centerSize, centerSize), "XZ");
            AddAxisHandle(surface, EditorTransformGizmoHandle::AxisX, originX, originY, axisLength, axisHit, centerSize, "X");
            AddAxisHandle(surface, EditorTransformGizmoHandle::AxisZ, originX, originY, axisLength, axisHit, centerSize, "Z");
            AddHandle(surface, EditorTransformGizmoHandle::AxisY, MakeRect(originX - yHandleOffset - 8, originY - yHandleOffset - 8, 16, 16), "Y");
        }

        void AddRotateHandles(EditorTransformGizmoSurface& surface, i32 originX, i32 originY, i32 axisLength, i32 axisHit, i32 centerSize, i32 yHandleOffset)
        {
            AddHandle(surface, EditorTransformGizmoHandle::CenterPlaneXZ, MakeRect(originX - centerSize / 2 - 2, originY - centerSize / 2 - 2, centerSize + 4, centerSize + 4), "RY");
            AddAxisHandle(surface, EditorTransformGizmoHandle::AxisX, originX, originY, axisLength, axisHit, centerSize, "RX");
            AddAxisHandle(surface, EditorTransformGizmoHandle::AxisZ, originX, originY, axisLength, axisHit, centerSize, "RZ");
            AddHandle(surface, EditorTransformGizmoHandle::AxisY, MakeRect(originX - yHandleOffset - 10, originY - yHandleOffset - 10, 20, 20), "RY");
        }

        void AddScaleHandles(EditorTransformGizmoSurface& surface, i32 originX, i32 originY, i32 axisLength, i32 axisHit, i32 centerSize, i32 yHandleOffset)
        {
            AddHandle(surface, EditorTransformGizmoHandle::CenterPlaneXZ, MakeRect(originX - centerSize / 2, originY - centerSize / 2, centerSize, centerSize), "S");
            AddAxisHandle(surface, EditorTransformGizmoHandle::AxisX, originX, originY, axisLength, axisHit, centerSize, "SX");
            AddAxisHandle(surface, EditorTransformGizmoHandle::AxisZ, originX, originY, axisLength, axisHit, centerSize, "SZ");
            AddHandle(surface, EditorTransformGizmoHandle::AxisY, MakeRect(originX - yHandleOffset - 9, originY - yHandleOffset - 9, 18, 18), "SY");
        }
    }

    const char* ToString(EditorTransformGizmoMode mode)
    {
        switch (mode)
        {
            case EditorTransformGizmoMode::Translate:
                return "Translate";
            case EditorTransformGizmoMode::Rotate:
                return "Rotate";
            case EditorTransformGizmoMode::Scale:
                return "Scale";
            default:
                return "Unknown";
        }
    }

    const char* ToString(EditorTransformGizmoHandle handle)
    {
        switch (handle)
        {
            case EditorTransformGizmoHandle::CenterPlaneXZ:
                return "CenterPlaneXZ";
            case EditorTransformGizmoHandle::AxisX:
                return "AxisX";
            case EditorTransformGizmoHandle::AxisY:
                return "AxisY";
            case EditorTransformGizmoHandle::AxisZ:
                return "AxisZ";
            default:
                return "None";
        }
    }


    const char* ToString(EditorTransformGizmoSpace space)
    {
        switch (space)
        {
            case EditorTransformGizmoSpace::Local:
                return "Local";
            case EditorTransformGizmoSpace::World:
            default:
                return "World";
        }
    }

    bool EditorTransformGizmoRectContains(EditorRect rect, i32 x, i32 y)
    {
        return x >= rect.x && x <= RectRight(rect) && y >= rect.y && y <= RectBottom(rect);
    }

    EditorTransformGizmoSurface BuildEditorTransformGizmoSurface(const EditorTransformGizmoBuildInput& input)
    {
        EditorTransformGizmoSurface surface{};
        surface.mode = input.mode;
        surface.space = input.space;
        surface.target = input.target;
        surface.snapEnabled = input.snapEnabled;
        surface.snapStep = input.snapStep;

        if (!input.target.valid || !RectUsable(input.viewport.rect))
        {
            surface.status = "Transform gizmo hidden: no valid target";
            return surface;
        }

        const float scale = std::clamp(input.viewport.scale, MinViewportScale, MaxViewportScale);
        const i32 viewportCenterX = input.viewport.rect.x + input.viewport.rect.width / 2;
        const i32 viewportCenterY = input.viewport.rect.y + input.viewport.rect.height / 2;
        const i32 originX = viewportCenterX + static_cast<i32>(std::round((input.target.x - input.viewport.centerX) * scale));
        const i32 originY = viewportCenterY - static_cast<i32>(std::round((input.target.z - input.viewport.centerZ) * scale));

        if (!IsPointNearViewport(input.viewport.rect, originX, originY, 96))
        {
            surface.status = "Transform gizmo hidden: target outside viewport";
            return surface;
        }

        surface.visible = true;
        surface.originScreenX = originX;
        surface.originScreenY = originY;
        surface.viewportScale = scale;

        const i32 axisLength = std::clamp(static_cast<i32>(std::round(scale * 1.25f)), 34, 72);
        const i32 axisHit = 10;
        const i32 centerSize = 18;
        const i32 yHandleOffset = std::clamp(static_cast<i32>(std::round(scale * 0.85f)), 26, 46);

        switch (surface.mode)
        {
            case EditorTransformGizmoMode::Rotate:
                AddRotateHandles(surface, originX, originY, axisLength, axisHit, centerSize, yHandleOffset);
                break;
            case EditorTransformGizmoMode::Scale:
                AddScaleHandles(surface, originX, originY, axisLength, axisHit, centerSize, yHandleOffset);
                break;
            case EditorTransformGizmoMode::Translate:
            default:
                AddTranslateHandles(surface, originX, originY, axisLength, axisHit, centerSize, yHandleOffset);
                break;
        }

        std::ostringstream out;
        out << "Transform gizmo: " << ToString(surface.mode) << "/" << ToString(surface.space) << " target=" << (surface.target.label.empty() ? "selection" : surface.target.label);
        if (surface.snapEnabled)
        {
            out << " snap=" << surface.snapStep;
        }
        surface.status = out.str();
        return surface;
    }

    EditorTransformGizmoHandle HitTestEditorTransformGizmo(const EditorTransformGizmoSurface& surface, i32 x, i32 y)
    {
        if (!surface.visible)
        {
            return EditorTransformGizmoHandle::None;
        }

        for (const EditorTransformGizmoHandleRect& handle : surface.handles)
        {
            if (handle.visible && EditorTransformGizmoRectContains(handle.rect, x, y))
            {
                return handle.handle;
            }
        }

        return EditorTransformGizmoHandle::None;
    }

    bool BeginEditorTransformGizmoDrag(EditorTransformGizmoRuntime& runtime, const EditorTransformGizmoSurface& surface, EditorTransformGizmoHandle handle, i32 mouseX, i32 mouseY)
    {
        if (!surface.visible || !surface.target.valid || handle == EditorTransformGizmoHandle::None)
        {
            return false;
        }

        runtime.active = true;
        runtime.mode = surface.mode;
        runtime.space = surface.space;
        runtime.handle = handle;
        runtime.startMouseX = mouseX;
        runtime.startMouseY = mouseY;
        runtime.currentMouseX = mouseX;
        runtime.currentMouseY = mouseY;
        runtime.startX = surface.target.x;
        runtime.startY = surface.target.y;
        runtime.startZ = surface.target.z;
        runtime.startRotationX = surface.target.rotationX;
        runtime.startRotationY = surface.target.rotationY;
        runtime.startRotationZ = surface.target.rotationZ;
        runtime.startScaleX = surface.target.scaleX;
        runtime.startScaleY = surface.target.scaleY;
        runtime.startScaleZ = surface.target.scaleZ;
        runtime.startYawDegrees = surface.target.rotationY;
        runtime.viewportScale = std::clamp(surface.viewportScale, MinViewportScale, MaxViewportScale);
        runtime.snapEnabled = surface.snapEnabled;
        runtime.snapStep = surface.snapStep;
        ++runtime.revision;
        return true;
    }

    EditorTransformGizmoDragResult UpdateEditorTransformGizmoDrag(EditorTransformGizmoRuntime& runtime, i32 mouseX, i32 mouseY)
    {
        EditorTransformGizmoDragResult result{};
        if (!runtime.active)
        {
            return result;
        }

        runtime.currentMouseX = mouseX;
        runtime.currentMouseY = mouseY;
        ++runtime.revision;

        result.active = true;
        result.mode = runtime.mode;
        result.space = runtime.space;
        result.handle = runtime.handle;
        result.x = runtime.startX;
        result.y = runtime.startY;
        result.z = runtime.startZ;
        result.rotationX = runtime.startRotationX;
        result.rotationY = runtime.startRotationY;
        result.rotationZ = runtime.startRotationZ;
        result.scaleX = runtime.startScaleX;
        result.scaleY = runtime.startScaleY;
        result.scaleZ = runtime.startScaleZ;

        const float scale = std::max(1.0f, runtime.viewportScale);
        const float dxPixels = static_cast<float>(mouseX - runtime.startMouseX);
        const float dyPixels = static_cast<float>(mouseY - runtime.startMouseY);
        const float dxWorld = dxPixels / scale;
        const float dzWorld = -dyPixels / scale;
        const float dyWorld = -dyPixels / scale;

        switch (runtime.mode)
        {
            case EditorTransformGizmoMode::Rotate:
            {
                const float yawDegrees = dxPixels * RotationDegreesPerPixel;
                const float pitchDegrees = -dyPixels * RotationDegreesPerPixel;
                const float rollDegrees = dxPixels * RotationDegreesPerPixel;
                if (runtime.handle == EditorTransformGizmoHandle::CenterPlaneXZ || runtime.handle == EditorTransformGizmoHandle::AxisY)
                {
                    result.rotationY = runtime.startRotationY + yawDegrees;
                }
                else if (runtime.handle == EditorTransformGizmoHandle::AxisX)
                {
                    result.rotationX = runtime.startRotationX + pitchDegrees;
                }
                else if (runtime.handle == EditorTransformGizmoHandle::AxisZ)
                {
                    result.rotationZ = runtime.startRotationZ + rollDegrees;
                }

                if (runtime.snapEnabled)
                {
                    result.rotationX = ApplySnap(result.rotationX, true, runtime.snapStep);
                    result.rotationY = ApplySnap(result.rotationY, true, runtime.snapStep);
                    result.rotationZ = ApplySnap(result.rotationZ, true, runtime.snapStep);
                }
                break;
            }
            case EditorTransformGizmoMode::Scale:
            {
                const float uniformDelta = (dxPixels - dyPixels) / 80.0f;
                if (runtime.handle == EditorTransformGizmoHandle::CenterPlaneXZ)
                {
                    result.scaleX = ClampScale(runtime.startScaleX + uniformDelta);
                    result.scaleY = ClampScale(runtime.startScaleY + uniformDelta);
                    result.scaleZ = ClampScale(runtime.startScaleZ + uniformDelta);
                }
                else if (runtime.handle == EditorTransformGizmoHandle::AxisX)
                {
                    result.scaleX = ClampScale(runtime.startScaleX + dxWorld);
                }
                else if (runtime.handle == EditorTransformGizmoHandle::AxisY)
                {
                    result.scaleY = ClampScale(runtime.startScaleY + dyWorld);
                }
                else if (runtime.handle == EditorTransformGizmoHandle::AxisZ)
                {
                    result.scaleZ = ClampScale(runtime.startScaleZ + dzWorld);
                }

                if (runtime.snapEnabled)
                {
                    result.scaleX = ClampScale(ApplySnap(result.scaleX, true, runtime.snapStep));
                    result.scaleY = ClampScale(ApplySnap(result.scaleY, true, runtime.snapStep));
                    result.scaleZ = ClampScale(ApplySnap(result.scaleZ, true, runtime.snapStep));
                }
                break;
            }
            case EditorTransformGizmoMode::Translate:
            default:
            {
                if (runtime.handle == EditorTransformGizmoHandle::CenterPlaneXZ)
                {
                    result.x = runtime.startX + dxWorld;
                    result.z = runtime.startZ + dzWorld;
                    if (runtime.snapEnabled)
                    {
                        result.x = ApplySnap(result.x, true, runtime.snapStep);
                        result.z = ApplySnap(result.z, true, runtime.snapStep);
                    }
                }
                else if (runtime.handle == EditorTransformGizmoHandle::AxisX || runtime.handle == EditorTransformGizmoHandle::AxisZ)
                {
                    const GizmoAxis2D axis = AxisForHandle(runtime.handle, runtime.space, runtime.startYawDegrees);
                    float amount = dxWorld * axis.x + dzWorld * axis.z;
                    if (runtime.snapEnabled)
                    {
                        amount = ApplySnap(amount, true, runtime.snapStep);
                    }
                    result.x = runtime.startX + axis.x * amount;
                    result.z = runtime.startZ + axis.z * amount;
                }
                else if (runtime.handle == EditorTransformGizmoHandle::AxisY)
                {
                    result.y = runtime.startY + dyWorld;
                    if (runtime.snapEnabled)
                    {
                        result.y = ApplySnap(result.y, true, runtime.snapStep);
                    }
                }
                break;
            }
        }

        result.deltaX = result.x - runtime.startX;
        result.deltaY = result.y - runtime.startY;
        result.deltaZ = result.z - runtime.startZ;
        result.deltaRotationX = result.rotationX - runtime.startRotationX;
        result.deltaRotationY = result.rotationY - runtime.startRotationY;
        result.deltaRotationZ = result.rotationZ - runtime.startRotationZ;
        result.deltaScaleX = result.scaleX - runtime.startScaleX;
        result.deltaScaleY = result.scaleY - runtime.startScaleY;
        result.deltaScaleZ = result.scaleZ - runtime.startScaleZ;
        result.changed = AnyTransformChanged(result, runtime);
        result.status = FormatDragStatus(result);
        return result;
    }

    void EndEditorTransformGizmoDrag(EditorTransformGizmoRuntime& runtime)
    {
        if (!runtime.active)
        {
            return;
        }

        runtime.active = false;
        runtime.handle = EditorTransformGizmoHandle::None;
        ++runtime.revision;
    }

    EditorTransformGizmoDiagnostics RunEditorTransformGizmoDiagnostics()
    {
        EditorTransformGizmoBuildInput input{};
        input.viewport.rect = {10, 20, 420, 260};
        input.viewport.centerX = 0.0f;
        input.viewport.centerZ = 0.0f;
        input.viewport.scale = 40.0f;
        input.target.valid = true;
        input.target.label = "ProbeCube";
        input.target.scaleX = 1.0f;
        input.target.scaleY = 1.0f;
        input.target.scaleZ = 1.0f;
        input.space = EditorTransformGizmoSpace::World;
        input.snapEnabled = false;
        input.snapStep = 0.5f;

        const EditorTransformGizmoSurface surface = BuildEditorTransformGizmoSurface(input);
        EditorTransformGizmoDiagnostics diagnostics{};
        diagnostics.visible = surface.visible;
        diagnostics.handleCount = surface.handles.size();
        for (const EditorTransformGizmoHandleRect& handle : surface.handles)
        {
            if (handle.visible)
            {
                ++diagnostics.visibleHandleCount;
            }
        }

        diagnostics.axisHitOk = HitTestEditorTransformGizmo(surface, surface.originScreenX + 50, surface.originScreenY) == EditorTransformGizmoHandle::AxisX;

        EditorTransformGizmoRuntime runtime{};
        diagnostics.planeDragOk = BeginEditorTransformGizmoDrag(runtime, surface, EditorTransformGizmoHandle::CenterPlaneXZ, surface.originScreenX, surface.originScreenY);
        runtime.viewportScale = input.viewport.scale;
        const EditorTransformGizmoDragResult plane = UpdateEditorTransformGizmoDrag(runtime, surface.originScreenX + 40, surface.originScreenY - 40);
        diagnostics.planeDragOk = diagnostics.planeDragOk && plane.changed && std::fabs(plane.x - 1.0f) < 0.001f && std::fabs(plane.z - 1.0f) < 0.001f;
        EndEditorTransformGizmoDrag(runtime);

        input.snapEnabled = true;
        input.snapStep = 0.5f;
        const EditorTransformGizmoSurface snappedSurface = BuildEditorTransformGizmoSurface(input);
        BeginEditorTransformGizmoDrag(runtime, snappedSurface, EditorTransformGizmoHandle::AxisX, snappedSurface.originScreenX, snappedSurface.originScreenY);
        runtime.viewportScale = input.viewport.scale;
        const EditorTransformGizmoDragResult snapped = UpdateEditorTransformGizmoDrag(runtime, snappedSurface.originScreenX + 29, snappedSurface.originScreenY);
        diagnostics.snapOk = std::fabs(snapped.x - 0.5f) < 0.001f;
        EndEditorTransformGizmoDrag(runtime);

        input.snapEnabled = false;
        const EditorTransformGizmoSurface ySurface = BuildEditorTransformGizmoSurface(input);
        BeginEditorTransformGizmoDrag(runtime, ySurface, EditorTransformGizmoHandle::AxisY, ySurface.originScreenX, ySurface.originScreenY);
        runtime.viewportScale = input.viewport.scale;
        const EditorTransformGizmoDragResult yDrag = UpdateEditorTransformGizmoDrag(runtime, ySurface.originScreenX, ySurface.originScreenY - 40);
        diagnostics.yDragOk = std::fabs(yDrag.y - 1.0f) < 0.001f;
        EndEditorTransformGizmoDrag(runtime);

        input.mode = EditorTransformGizmoMode::Rotate;
        input.snapStep = 15.0f;
        input.snapEnabled = true;
        const EditorTransformGizmoSurface rotateSurface = BuildEditorTransformGizmoSurface(input);
        BeginEditorTransformGizmoDrag(runtime, rotateSurface, EditorTransformGizmoHandle::AxisY, rotateSurface.originScreenX, rotateSurface.originScreenY);
        const EditorTransformGizmoDragResult rotate = UpdateEditorTransformGizmoDrag(runtime, rotateSurface.originScreenX + 31, rotateSurface.originScreenY);
        diagnostics.rotateDragOk = rotate.changed && std::fabs(rotate.rotationY - 15.0f) < 0.001f;
        EndEditorTransformGizmoDrag(runtime);

        input.mode = EditorTransformGizmoMode::Scale;
        input.snapStep = 0.1f;
        input.snapEnabled = true;
        const EditorTransformGizmoSurface scaleSurface = BuildEditorTransformGizmoSurface(input);
        BeginEditorTransformGizmoDrag(runtime, scaleSurface, EditorTransformGizmoHandle::CenterPlaneXZ, scaleSurface.originScreenX, scaleSurface.originScreenY);
        const EditorTransformGizmoDragResult scaleDrag = UpdateEditorTransformGizmoDrag(runtime, scaleSurface.originScreenX + 40, scaleSurface.originScreenY - 40);
        diagnostics.scaleDragOk = scaleDrag.changed
            && std::fabs(scaleDrag.scaleX - 2.0f) < 0.001f
            && std::fabs(scaleDrag.scaleY - 2.0f) < 0.001f
            && std::fabs(scaleDrag.scaleZ - 2.0f) < 0.001f;
        EndEditorTransformGizmoDrag(runtime);

        input.mode = EditorTransformGizmoMode::Translate;
        input.space = EditorTransformGizmoSpace::Local;
        input.snapEnabled = true;
        input.snapStep = 0.5f;
        input.target.rotationY = 90.0f;
        const EditorTransformGizmoSurface localSurface = BuildEditorTransformGizmoSurface(input);
        diagnostics.localHitOk = HitTestEditorTransformGizmo(localSurface, localSurface.originScreenX, localSurface.originScreenY - 50) == EditorTransformGizmoHandle::AxisX;
        BeginEditorTransformGizmoDrag(runtime, localSurface, EditorTransformGizmoHandle::AxisX, localSurface.originScreenX, localSurface.originScreenY);
        const EditorTransformGizmoDragResult localDrag = UpdateEditorTransformGizmoDrag(runtime, localSurface.originScreenX, localSurface.originScreenY - 40);
        diagnostics.localSpaceOk = localDrag.changed && localDrag.space == EditorTransformGizmoSpace::Local && std::fabs(localDrag.x) < 0.001f && std::fabs(localDrag.z - 1.0f) < 0.001f;
        EndEditorTransformGizmoDrag(runtime);

        diagnostics.modeSurfaceOk = rotateSurface.visible && rotateSurface.handles.size() == 4 && scaleSurface.visible && scaleSurface.handles.size() == 4;
        diagnostics.ok = diagnostics.visible
            && diagnostics.handleCount == 4
            && diagnostics.visibleHandleCount == 4
            && diagnostics.axisHitOk
            && diagnostics.planeDragOk
            && diagnostics.snapOk
            && diagnostics.yDragOk
            && diagnostics.rotateDragOk
            && diagnostics.scaleDragOk
            && diagnostics.modeSurfaceOk
            && diagnostics.localSpaceOk
            && diagnostics.localHitOk;
        diagnostics.summary = FormatEditorTransformGizmoDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorTransformGizmoDiagnostics(const EditorTransformGizmoDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "Editor transform gizmo: visible=" << (diagnostics.visible ? "yes" : "no")
            << " handles=" << diagnostics.handleCount
            << " visibleHandles=" << diagnostics.visibleHandleCount
            << " axisHit=" << (diagnostics.axisHitOk ? "ok" : "fail")
            << " planeDrag=" << (diagnostics.planeDragOk ? "ok" : "fail")
            << " snap=" << (diagnostics.snapOk ? "ok" : "fail")
            << " yDrag=" << (diagnostics.yDragOk ? "ok" : "fail")
            << " rotate=" << (diagnostics.rotateDragOk ? "ok" : "fail")
            << " scale=" << (diagnostics.scaleDragOk ? "ok" : "fail")
            << " modes=" << (diagnostics.modeSurfaceOk ? "ok" : "fail")
            << " localDrag=" << (diagnostics.localSpaceOk ? "ok" : "fail")
            << " localHit=" << (diagnostics.localHitOk ? "ok" : "fail");
        return out.str();
    }
}
