#include <AK/EditorUI/EditorSceneView.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float MinOrthoScale = 8.0f;
        constexpr float MaxOrthoScale = 160.0f;
        constexpr float MinPitch = -89.0f;
        constexpr float MaxPitch = 89.0f;
        constexpr float MinFovY = 25.0f;
        constexpr float MaxFovY = 120.0f;
        constexpr float MinNearPlane = 0.001f;
        constexpr float MinFarPlane = 1.0f;
        constexpr float MaxFarPlane = 1000000.0f;
        constexpr float MinMoveSpeed = 0.1f;
        constexpr float MaxMoveSpeed = 1000.0f;
        constexpr float MinFrameDistance = 3.0f;
        constexpr float MaxFrameDistance = 10000.0f;
        constexpr float MaxCameraCoordinate = 1000000000.0f;
        constexpr float FramePadding = 1.15f;
        constexpr float Epsilon = 0.00001f;

        struct Vec3f
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        float DegreesToRadians(float degrees)
        {
            return degrees * Pi / 180.0f;
        }

        bool IsFinite(float value)
        {
            return std::isfinite(value);
        }

        bool IsFinite(Vec3f value)
        {
            return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
        }

        float Length(Vec3f v)
        {
            return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }

        float Distance(Vec3f a, Vec3f b)
        {
            return Length({a.x - b.x, a.y - b.y, a.z - b.z});
        }

        Vec3f Normalize(Vec3f v, Vec3f fallback)
        {
            const float length = Length(v);
            if (!std::isfinite(length) || length <= Epsilon)
            {
                return fallback;
            }
            return {v.x / length, v.y / length, v.z / length};
        }

        Vec3f Cross(Vec3f a, Vec3f b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        Vec3f Add(Vec3f a, Vec3f b)
        {
            return {a.x + b.x, a.y + b.y, a.z + b.z};
        }

        Vec3f Multiply(Vec3f v, float scale)
        {
            return {v.x * scale, v.y * scale, v.z * scale};
        }

        Vec3f ForwardFromAngles(float yawDegrees, float pitchDegrees)
        {
            const float yaw = DegreesToRadians(yawDegrees);
            const float pitch = DegreesToRadians(std::clamp(pitchDegrees, MinPitch, MaxPitch));
            const float cp = std::cos(pitch);
            return Normalize({std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp}, {0.0f, 0.0f, 1.0f});
        }

        Vec3f RightFromYaw(float yawDegrees)
        {
            const float yaw = DegreesToRadians(yawDegrees);
            return Normalize({std::cos(yaw), 0.0f, -std::sin(yaw)}, {1.0f, 0.0f, 0.0f});
        }

        Vec3f UpFromAxes(Vec3f forward, Vec3f right)
        {
            return Normalize(Cross(forward, right), {0.0f, 1.0f, 0.0f});
        }

        bool RectUsable(EditorRect rect)
        {
            return rect.width > 0 && rect.height > 0;
        }

        i32 RectRight(EditorRect rect)
        {
            const long long value = static_cast<long long>(rect.x) + static_cast<long long>(rect.width);
            return static_cast<i32>(std::clamp(value,
                                               static_cast<long long>(std::numeric_limits<i32>::min()),
                                               static_cast<long long>(std::numeric_limits<i32>::max())));
        }

        i32 RectBottom(EditorRect rect)
        {
            const long long value = static_cast<long long>(rect.y) + static_cast<long long>(rect.height);
            return static_cast<i32>(std::clamp(value,
                                               static_cast<long long>(std::numeric_limits<i32>::min()),
                                               static_cast<long long>(std::numeric_limits<i32>::max())));
        }

        i32 OffsetCoordinate(i32 value, i32 offset)
        {
            const long long result = static_cast<long long>(value) + static_cast<long long>(offset);
            return static_cast<i32>(std::clamp(result,
                                               static_cast<long long>(std::numeric_limits<i32>::min()),
                                               static_cast<long long>(std::numeric_limits<i32>::max())));
        }

        bool RectContains(EditorRect rect, i32 x, i32 y)
        {
            return x >= rect.x && y >= rect.y && x < RectRight(rect) && y < RectBottom(rect);
        }

        float NormalizeAngleDegrees(float degrees)
        {
            if (!IsFinite(degrees))
            {
                return 0.0f;
            }

            float result = std::fmod(degrees, 360.0f);
            if (result > 180.0f)
            {
                result -= 360.0f;
            }
            else if (result < -180.0f)
            {
                result += 360.0f;
            }
            return result;
        }

        bool Nearly(float value, float expected, float tolerance)
        {
            return std::fabs(value - expected) <= tolerance;
        }

        float SanitizeScalar(float value, float fallback, float minValue, float maxValue, bool& changed)
        {
            float result = value;
            if (!IsFinite(result))
            {
                result = fallback;
            }
            result = std::clamp(result, minValue, maxValue);
            if (result != value)
            {
                changed = true;
            }
            return result;
        }

        bool CameraRayInputsFinite(const EditorSceneViewCamera& camera)
        {
            return IsFinite(camera.centerX)
                && IsFinite(camera.focusY)
                && IsFinite(camera.centerZ)
                && IsFinite(camera.orthographicScale)
                && IsFinite(camera.positionX)
                && IsFinite(camera.positionY)
                && IsFinite(camera.positionZ)
                && IsFinite(camera.yawDegrees)
                && IsFinite(camera.pitchDegrees)
                && IsFinite(camera.fovYDegrees)
                && IsFinite(camera.nearPlane)
                && IsFinite(camera.farPlane);
        }

        bool BoundsCoordinateUsable(float value)
        {
            return IsFinite(value) && value >= -MaxCameraCoordinate && value <= MaxCameraCoordinate;
        }

        bool BoundsUsable(const EditorSceneViewBounds& bounds)
        {
            return bounds.valid
                && BoundsCoordinateUsable(bounds.minX)
                && BoundsCoordinateUsable(bounds.minY)
                && BoundsCoordinateUsable(bounds.minZ)
                && BoundsCoordinateUsable(bounds.maxX)
                && BoundsCoordinateUsable(bounds.maxY)
                && BoundsCoordinateUsable(bounds.maxZ)
                && bounds.minX <= bounds.maxX
                && bounds.minY <= bounds.maxY
                && bounds.minZ <= bounds.maxZ;
        }

        Vec3f BoundsCenter(const EditorSceneViewBounds& bounds)
        {
            return {
                (bounds.minX + bounds.maxX) * 0.5f,
                (bounds.minY + bounds.maxY) * 0.5f,
                (bounds.minZ + bounds.maxZ) * 0.5f
            };
        }

        Vec3f BoundsExtents(const EditorSceneViewBounds& bounds)
        {
            return {
                std::max(Epsilon, (bounds.maxX - bounds.minX) * 0.5f),
                std::max(Epsilon, (bounds.maxY - bounds.minY) * 0.5f),
                std::max(Epsilon, (bounds.maxZ - bounds.minZ) * 0.5f)
            };
        }

        Vec3f CameraFocusPoint(const EditorSceneViewCamera& camera)
        {
            return {camera.centerX, camera.focusY, camera.centerZ};
        }

        Vec3f CameraPosition(const EditorSceneViewCamera& camera)
        {
            return {camera.positionX, camera.positionY, camera.positionZ};
        }

        float CameraDistanceToFocus(const EditorSceneViewCamera& camera)
        {
            float distance = Distance(CameraPosition(camera), CameraFocusPoint(camera));
            if (!IsFinite(distance) || distance < MinFrameDistance)
            {
                distance = 10.0f;
            }
            return std::clamp(distance, MinFrameDistance, MaxFrameDistance);
        }

        void PositionCameraAtFocus(EditorSceneViewCamera& camera, Vec3f focus, float distance)
        {
            const Vec3f forward = ForwardFromAngles(camera.yawDegrees, camera.pitchDegrees);
            camera.centerX = focus.x;
            camera.focusY = focus.y;
            camera.centerZ = focus.z;
            camera.positionX = focus.x - forward.x * distance;
            camera.positionY = focus.y - forward.y * distance;
            camera.positionZ = focus.z - forward.z * distance;
        }

        void OffsetCameraAndFocus(EditorSceneViewCamera& camera, Vec3f offset)
        {
            if (!IsFinite(offset))
            {
                return;
            }

            camera.centerX += offset.x;
            camera.focusY += offset.y;
            camera.centerZ += offset.z;
            camera.positionX += offset.x;
            camera.positionY += offset.y;
            camera.positionZ += offset.z;
        }

        std::string FormatCameraStatus(const EditorSceneViewCamera& camera, const char* prefix)
        {
            std::ostringstream out;
            out << prefix << ' ' << ToString(camera.mode) << '/' << ToString(camera.projection);
            if (camera.mode == EditorSceneViewMode::Mode2D)
            {
                out << " center=(" << camera.centerX << ',' << camera.centerZ << ") scale=" << static_cast<int>(camera.orthographicScale);
            }
            else
            {
                out << " pos=(" << camera.positionX << ',' << camera.positionY << ',' << camera.positionZ << ") yaw=" << camera.yawDegrees << " pitch=" << camera.pitchDegrees;
            }
            return out.str();
        }

        CommandId CommandForOverlayControl(EditorSceneViewOverlayControl control)
        {
            switch (control)
            {
                case EditorSceneViewOverlayControl::Mode2D: return CommandId::ViewScene2D;
                case EditorSceneViewOverlayControl::Mode3D: return CommandId::ViewScene3D;
                case EditorSceneViewOverlayControl::GridSnap: return CommandId::ToggleGridSnap;
                case EditorSceneViewOverlayControl::ToolTranslate: return CommandId::ToolTranslate;
                case EditorSceneViewOverlayControl::ToolRotate: return CommandId::ToolRotate;
                case EditorSceneViewOverlayControl::ToolScale: return CommandId::ToolScale;
                case EditorSceneViewOverlayControl::SpaceWorld: return CommandId::ToolSpaceWorld;
                case EditorSceneViewOverlayControl::SpaceLocal: return CommandId::ToolSpaceLocal;
                case EditorSceneViewOverlayControl::FocusSelection: return CommandId::FocusSelection;
                case EditorSceneViewOverlayControl::FrameAll: return CommandId::ViewFrameAll;
                case EditorSceneViewOverlayControl::ResetView: return CommandId::ResetViewport;
                case EditorSceneViewOverlayControl::OrientationAxisX: return CommandId::ViewAxisRight;
                case EditorSceneViewOverlayControl::OrientationAxisY: return CommandId::ViewAxisTop;
                case EditorSceneViewOverlayControl::OrientationAxisZ: return CommandId::ViewAxisFront;
                case EditorSceneViewOverlayControl::None:
                default:
                    return CommandId::CloseEditor;
            }
        }

        bool IsOverlayControlActive(EditorSceneViewOverlayControl control, const EditorSceneViewOverlayBuildInput& input, const EditorSceneViewCamera& camera)
        {
            switch (control)
            {
                case EditorSceneViewOverlayControl::Mode2D:
                    return camera.mode == EditorSceneViewMode::Mode2D;
                case EditorSceneViewOverlayControl::Mode3D:
                    return camera.mode == EditorSceneViewMode::Mode3D;
                case EditorSceneViewOverlayControl::GridSnap:
                    return input.snapEnabled;
                case EditorSceneViewOverlayControl::ToolTranslate:
                    return input.activeTool == CommandId::ToolTranslate;
                case EditorSceneViewOverlayControl::ToolRotate:
                    return input.activeTool == CommandId::ToolRotate;
                case EditorSceneViewOverlayControl::ToolScale:
                    return input.activeTool == CommandId::ToolScale;
                case EditorSceneViewOverlayControl::SpaceWorld:
                    return input.activeSpace == CommandId::ToolSpaceWorld;
                case EditorSceneViewOverlayControl::SpaceLocal:
                    return input.activeSpace == CommandId::ToolSpaceLocal;
                case EditorSceneViewOverlayControl::OrientationAxisX:
                    return camera.mode == EditorSceneViewMode::Mode3D
                        && Nearly(NormalizeAngleDegrees(camera.yawDegrees), -90.0f, 0.25f)
                        && Nearly(camera.pitchDegrees, 0.0f, 0.25f);
                case EditorSceneViewOverlayControl::OrientationAxisY:
                    return camera.mode == EditorSceneViewMode::Mode3D && camera.pitchDegrees < -88.0f;
                case EditorSceneViewOverlayControl::OrientationAxisZ:
                    return camera.mode == EditorSceneViewMode::Mode3D
                        && Nearly(NormalizeAngleDegrees(camera.yawDegrees), 0.0f, 0.25f)
                        && Nearly(camera.pitchDegrees, 0.0f, 0.25f);
                default:
                    return false;
            }
        }

        void AddOverlayControl(EditorSceneViewOverlaySurface& surface, EditorSceneViewOverlayControl control, EditorRect rect, const char* label, bool enabled, bool active)
        {
            EditorSceneViewOverlayControlDesc desc{};
            desc.control = control;
            desc.rect = rect;
            desc.label = label ? label : "";
            desc.command = CommandForOverlayControl(control);
            desc.visible = RectUsable(rect);
            desc.enabled = enabled;
            desc.active = active;
            surface.controls.push_back(std::move(desc));
        }

        const EditorSceneViewOverlayControlDesc* FindOverlayControl(const EditorSceneViewOverlaySurface& surface, EditorSceneViewOverlayControl control)
        {
            for (const EditorSceneViewOverlayControlDesc& desc : surface.controls)
            {
                if (desc.control == control)
                {
                    return &desc;
                }
            }
            return nullptr;
        }
    }

    const char* ToString(EditorSceneViewMode mode)
    {
        switch (mode)
        {
            case EditorSceneViewMode::Mode2D: return "2D";
            case EditorSceneViewMode::Mode3D: return "3D";
        }
        return "2D";
    }

    const char* ToString(EditorSceneViewProjection projection)
    {
        switch (projection)
        {
            case EditorSceneViewProjection::Orthographic: return "Orthographic";
            case EditorSceneViewProjection::Perspective: return "Perspective";
        }
        return "Orthographic";
    }

    const char* ToString(EditorSceneViewAxis axis)
    {
        switch (axis)
        {
            case EditorSceneViewAxis::Top: return "Top";
            case EditorSceneViewAxis::Front: return "Front";
            case EditorSceneViewAxis::Right: return "Right";
        }
        return "Top";
    }

    const char* ToString(EditorSceneViewOverlayControl control)
    {
        switch (control)
        {
            case EditorSceneViewOverlayControl::Mode2D: return "Mode2D";
            case EditorSceneViewOverlayControl::Mode3D: return "Mode3D";
            case EditorSceneViewOverlayControl::GridSnap: return "GridSnap";
            case EditorSceneViewOverlayControl::ToolTranslate: return "ToolTranslate";
            case EditorSceneViewOverlayControl::ToolRotate: return "ToolRotate";
            case EditorSceneViewOverlayControl::ToolScale: return "ToolScale";
            case EditorSceneViewOverlayControl::SpaceWorld: return "SpaceWorld";
            case EditorSceneViewOverlayControl::SpaceLocal: return "SpaceLocal";
            case EditorSceneViewOverlayControl::FocusSelection: return "FocusSelection";
            case EditorSceneViewOverlayControl::FrameAll: return "FrameAll";
            case EditorSceneViewOverlayControl::ResetView: return "ResetView";
            case EditorSceneViewOverlayControl::OrientationAxisX: return "OrientationAxisX";
            case EditorSceneViewOverlayControl::OrientationAxisY: return "OrientationAxisY";
            case EditorSceneViewOverlayControl::OrientationAxisZ: return "OrientationAxisZ";
            case EditorSceneViewOverlayControl::None:
            default:
                return "None";
        }
    }

    EditorSceneViewCamera MakeDefaultEditorSceneViewCamera()
    {
        EditorSceneViewCamera camera{};
        camera.mode = EditorSceneViewMode::Mode2D;
        camera.projection = EditorSceneViewProjection::Orthographic;
        camera.centerX = 0.0f;
        camera.focusY = 0.0f;
        camera.centerZ = 0.0f;
        camera.orthographicScale = 36.0f;
        camera.positionX = 0.0f;
        camera.positionY = 8.0f;
        camera.positionZ = -10.0f;
        camera.yawDegrees = 0.0f;
        camera.pitchDegrees = -35.0f;
        camera.fovYDegrees = 60.0f;
        camera.nearPlane = 0.05f;
        camera.farPlane = 10000.0f;
        camera.moveSpeed = 8.0f;
        camera.revision = 1;
        return camera;
    }

    bool SanitizeEditorSceneViewCamera(EditorSceneViewCamera& camera)
    {
        bool changed = false;

        const EditorSceneViewProjection expectedProjection = camera.mode == EditorSceneViewMode::Mode3D
            ? EditorSceneViewProjection::Perspective
            : EditorSceneViewProjection::Orthographic;
        if (camera.projection != expectedProjection)
        {
            camera.projection = expectedProjection;
            changed = true;
        }

        camera.centerX = SanitizeScalar(camera.centerX, 0.0f, -MaxCameraCoordinate, MaxCameraCoordinate, changed);
        camera.focusY = SanitizeScalar(camera.focusY, 0.0f, -MaxCameraCoordinate, MaxCameraCoordinate, changed);
        camera.centerZ = SanitizeScalar(camera.centerZ, 0.0f, -MaxCameraCoordinate, MaxCameraCoordinate, changed);
        camera.orthographicScale = SanitizeScalar(camera.orthographicScale, 36.0f, MinOrthoScale, MaxOrthoScale, changed);
        camera.positionX = SanitizeScalar(camera.positionX, 0.0f, -MaxCameraCoordinate, MaxCameraCoordinate, changed);
        camera.positionY = SanitizeScalar(camera.positionY, 8.0f, -MaxCameraCoordinate, MaxCameraCoordinate, changed);
        camera.positionZ = SanitizeScalar(camera.positionZ, -10.0f, -MaxCameraCoordinate, MaxCameraCoordinate, changed);
        camera.yawDegrees = SanitizeScalar(camera.yawDegrees, 0.0f, -3600.0f, 3600.0f, changed);
        camera.pitchDegrees = SanitizeScalar(camera.pitchDegrees, -35.0f, MinPitch, MaxPitch, changed);
        camera.fovYDegrees = SanitizeScalar(camera.fovYDegrees, 60.0f, MinFovY, MaxFovY, changed);
        camera.nearPlane = SanitizeScalar(camera.nearPlane, 0.05f, MinNearPlane, MaxFarPlane - MinFarPlane, changed);
        camera.farPlane = SanitizeScalar(camera.farPlane, 10000.0f, MinFarPlane, MaxFarPlane, changed);
        camera.moveSpeed = SanitizeScalar(camera.moveSpeed, 8.0f, MinMoveSpeed, MaxMoveSpeed, changed);
        if (camera.farPlane < camera.nearPlane + MinFarPlane)
        {
            camera.farPlane = std::min(MaxFarPlane, camera.nearPlane + MinFarPlane);
            changed = true;
        }

        if (changed)
        {
            ++camera.revision;
        }
        return changed;
    }

    void SetEditorSceneViewMode(EditorSceneViewCamera& camera, EditorSceneViewMode mode)
    {
        SanitizeEditorSceneViewCamera(camera);
        if (camera.mode == mode)
        {
            return;
        }

        EditorSceneViewGroundPoint previousGroundCenter{};
        if (mode == EditorSceneViewMode::Mode2D && camera.mode == EditorSceneViewMode::Mode3D)
        {
            previousGroundCenter = IntersectEditorSceneViewGroundPlane(BuildEditorSceneViewRay(camera, {0, 0, 2, 2}, 1, 1), 0.0f);
        }

        camera.mode = mode;
        camera.projection = mode == EditorSceneViewMode::Mode3D ? EditorSceneViewProjection::Perspective : EditorSceneViewProjection::Orthographic;
        if (mode == EditorSceneViewMode::Mode3D)
        {
            const Vec3f focus = CameraFocusPoint(camera);
            const float distance = CameraDistanceToFocus(camera);
            camera.pitchDegrees = std::clamp(camera.pitchDegrees, -70.0f, -12.0f);
            PositionCameraAtFocus(camera, focus, distance);
        }
        else
        {
            if (previousGroundCenter.valid)
            {
                camera.centerX = previousGroundCenter.x;
                camera.focusY = previousGroundCenter.y;
                camera.centerZ = previousGroundCenter.z;
            }
            camera.orthographicScale = std::clamp(camera.orthographicScale, MinOrthoScale, MaxOrthoScale);
        }
        ++camera.revision;
    }

    void ResetEditorSceneViewCamera(EditorSceneViewCamera& camera, EditorSceneViewMode mode)
    {
        const u64 revision = camera.revision + 1;
        camera = MakeDefaultEditorSceneViewCamera();
        camera.revision = revision;
        if (mode == EditorSceneViewMode::Mode3D)
        {
            camera.mode = EditorSceneViewMode::Mode3D;
            camera.projection = EditorSceneViewProjection::Perspective;
            camera.positionX = camera.centerX;
            camera.positionY = 8.0f;
            camera.positionZ = camera.centerZ - 10.0f;
            camera.pitchDegrees = -35.0f;
            PositionCameraAtFocus(camera, CameraFocusPoint(camera), CameraDistanceToFocus(camera));
        }
    }

    void FocusEditorSceneViewCamera(EditorSceneViewCamera& camera, float x, float y, float z)
    {
        if (!BoundsCoordinateUsable(x) || !BoundsCoordinateUsable(y) || !BoundsCoordinateUsable(z))
        {
            return;
        }
        SanitizeEditorSceneViewCamera(camera);
        const Vec3f previousFocus = CameraFocusPoint(camera);
        const Vec3f previousPosition = CameraPosition(camera);
        camera.centerX = x;
        camera.focusY = y;
        camera.centerZ = z;
        if (camera.mode == EditorSceneViewMode::Mode3D)
        {
            const float distance = std::clamp(Distance(CameraPosition(camera), {x, y, z}), MinFrameDistance, MaxFrameDistance);
            PositionCameraAtFocus(camera, {x, y, z}, std::max(10.0f, distance));
        }
        if (!Nearly(camera.centerX, previousFocus.x, Epsilon)
            || !Nearly(camera.focusY, previousFocus.y, Epsilon)
            || !Nearly(camera.centerZ, previousFocus.z, Epsilon)
            || !Nearly(camera.positionX, previousPosition.x, Epsilon)
            || !Nearly(camera.positionY, previousPosition.y, Epsilon)
            || !Nearly(camera.positionZ, previousPosition.z, Epsilon))
        {
            ++camera.revision;
        }
    }

    bool FrameEditorSceneViewCamera(EditorSceneViewCamera& camera, const EditorSceneViewBounds& bounds, EditorRect viewport)
    {
        if (!BoundsUsable(bounds) || !RectUsable(viewport))
        {
            return false;
        }

        SanitizeEditorSceneViewCamera(camera);
        const Vec3f center = BoundsCenter(bounds);
        const Vec3f extents = BoundsExtents(bounds);
        camera.centerX = center.x;
        camera.focusY = center.y;
        camera.centerZ = center.z;

        if (camera.mode == EditorSceneViewMode::Mode2D)
        {
            const float width = static_cast<float>(std::max<i32>(1, viewport.width));
            const float height = static_cast<float>(std::max<i32>(1, viewport.height));
            const float sizeX = std::max(1.0f, extents.x * 2.0f * FramePadding);
            const float sizeZ = std::max(1.0f, extents.z * 2.0f * FramePadding);
            camera.orthographicScale = std::clamp(std::min(width / sizeX, height / sizeZ), MinOrthoScale, MaxOrthoScale);
        }
        else
        {
            const float radius = std::max(1.0f, Length(extents));
            const float fov = DegreesToRadians(std::clamp(camera.fovYDegrees, MinFovY, MaxFovY));
            const float distance = std::clamp((radius / std::tan(fov * 0.5f)) * FramePadding, MinFrameDistance, MaxFrameDistance);
            PositionCameraAtFocus(camera, center, distance);
        }

        ++camera.revision;
        return true;
    }

    void SnapEditorSceneViewCameraToAxis(EditorSceneViewCamera& camera, EditorSceneViewAxis axis)
    {
        SanitizeEditorSceneViewCamera(camera);
        const Vec3f focus = CameraFocusPoint(camera);
        float distance = Distance(CameraPosition(camera), focus);
        if (!IsFinite(distance) || distance < MinFrameDistance)
        {
            distance = 10.0f;
        }
        distance = std::clamp(distance, MinFrameDistance, MaxFrameDistance);

        camera.mode = EditorSceneViewMode::Mode3D;
        camera.projection = EditorSceneViewProjection::Perspective;
        switch (axis)
        {
            case EditorSceneViewAxis::Front:
                camera.yawDegrees = 0.0f;
                camera.pitchDegrees = 0.0f;
                break;
            case EditorSceneViewAxis::Right:
                camera.yawDegrees = -90.0f;
                camera.pitchDegrees = 0.0f;
                break;
            case EditorSceneViewAxis::Top:
            default:
                camera.yawDegrees = 0.0f;
                camera.pitchDegrees = -89.0f;
                break;
        }
        PositionCameraAtFocus(camera, focus, distance);
        ++camera.revision;
    }

    EditorSceneViewRay BuildEditorSceneViewRay(const EditorSceneViewCamera& camera, EditorRect viewport, i32 screenX, i32 screenY)
    {
        EditorSceneViewRay ray{};
        if (!RectUsable(viewport) || !CameraRayInputsFinite(camera))
        {
            return ray;
        }

        EditorSceneViewCamera safeCamera = camera;
        SanitizeEditorSceneViewCamera(safeCamera);
        const float width = static_cast<float>(std::max<i32>(1, viewport.width));
        const float height = static_cast<float>(std::max<i32>(1, viewport.height));
        const float centerX = static_cast<float>(viewport.x) + width * 0.5f;
        const float centerY = static_cast<float>(viewport.y) + height * 0.5f;
        const float sx = (static_cast<float>(screenX) - centerX) / width;
        const float sy = (static_cast<float>(screenY) - centerY) / height;

        ray.valid = true;
        if (safeCamera.mode == EditorSceneViewMode::Mode2D || safeCamera.projection == EditorSceneViewProjection::Orthographic)
        {
            const float scale = std::clamp(safeCamera.orthographicScale, MinOrthoScale, MaxOrthoScale);
            ray.originX = safeCamera.centerX + (static_cast<float>(screenX) - centerX) / scale;
            ray.originY = MaxCameraCoordinate;
            ray.originZ = safeCamera.centerZ - (static_cast<float>(screenY) - centerY) / scale;
            ray.dirX = 0.0f;
            ray.dirY = -1.0f;
            ray.dirZ = 0.0f;
            return ray;
        }

        const Vec3f forward = ForwardFromAngles(safeCamera.yawDegrees, safeCamera.pitchDegrees);
        const Vec3f right = RightFromYaw(safeCamera.yawDegrees);
        const Vec3f up = UpFromAxes(forward, right);
        const float fov = DegreesToRadians(std::clamp(safeCamera.fovYDegrees, MinFovY, MaxFovY));
        const float aspect = width / height;
        const float tanHalfFov = std::tan(fov * 0.5f);
        const Vec3f dir = Normalize({
            forward.x + right.x * (sx * 2.0f * aspect * tanHalfFov) - up.x * (sy * 2.0f * tanHalfFov),
            forward.y + right.y * (sx * 2.0f * aspect * tanHalfFov) - up.y * (sy * 2.0f * tanHalfFov),
            forward.z + right.z * (sx * 2.0f * aspect * tanHalfFov) - up.z * (sy * 2.0f * tanHalfFov)
        }, forward);

        ray.originX = safeCamera.positionX;
        ray.originY = safeCamera.positionY;
        ray.originZ = safeCamera.positionZ;
        ray.dirX = dir.x;
        ray.dirY = dir.y;
        ray.dirZ = dir.z;
        if (!IsFinite(Vec3f{ray.originX, ray.originY, ray.originZ}) || !IsFinite(Vec3f{ray.dirX, ray.dirY, ray.dirZ}))
        {
            return {};
        }
        return ray;
    }

    EditorSceneViewGroundPoint IntersectEditorSceneViewGroundPlane(const EditorSceneViewRay& ray, float planeY)
    {
        EditorSceneViewGroundPoint point{};
        if (!ray.valid || std::fabs(ray.dirY) <= Epsilon)
        {
            return point;
        }

        const float t = (planeY - ray.originY) / ray.dirY;
        if (!std::isfinite(t) || t < 0.0f)
        {
            return point;
        }

        point.valid = true;
        point.x = ray.originX + ray.dirX * t;
        point.y = planeY;
        point.z = ray.originZ + ray.dirZ * t;
        point.distance = t;
        if (!IsFinite(Vec3f{point.x, point.y, point.z}) || !IsFinite(point.distance))
        {
            return {};
        }
        return point;
    }

    EditorSceneViewGroundPoint ScreenToEditorSceneViewGroundPoint(const EditorSceneViewCamera& camera, EditorRect viewport, i32 screenX, i32 screenY, float planeY)
    {
        return IntersectEditorSceneViewGroundPlane(BuildEditorSceneViewRay(camera, viewport, screenX, screenY), planeY);
    }

    EditorSceneViewNavigationResult ApplyEditorSceneViewNavigation(EditorSceneViewCamera& camera, const EditorSceneViewNavigationInput& input)
    {
        EditorSceneViewNavigationResult result{};
        SanitizeEditorSceneViewCamera(camera);
        if (input.focusRequested)
        {
            const u64 previousRevision = camera.revision;
            FocusEditorSceneViewCamera(camera, input.focusX, input.focusY, input.focusZ);
            if (camera.revision != previousRevision)
            {
                result.changed = true;
                result.status = FormatCameraStatus(camera, "Scene view focused");
            }
        }

        if (camera.mode == EditorSceneViewMode::Mode2D)
        {
            if (input.pan2D && (input.mouseDeltaX != 0 || input.mouseDeltaY != 0) && RectUsable(input.viewport))
            {
                const float scale = std::clamp(camera.orthographicScale, MinOrthoScale, MaxOrthoScale);
                camera.centerX -= static_cast<float>(input.mouseDeltaX) / scale;
                camera.centerZ += static_cast<float>(input.mouseDeltaY) / scale;
                ++camera.revision;
                result.changed = true;
                result.status = FormatCameraStatus(camera, "Scene view panning");
            }
            if (input.mouseWheelDelta != 0 && RectUsable(input.viewport))
            {
                const EditorSceneViewGroundPoint before = ScreenToEditorSceneViewGroundPoint(camera, input.viewport, input.mouseX, input.mouseY, 0.0f);
                const float zoomSteps = static_cast<float>(input.mouseWheelDelta) / 120.0f;
                const float nextScale = std::clamp(camera.orthographicScale * std::pow(1.12f, zoomSteps), MinOrthoScale, MaxOrthoScale);
                if (nextScale != camera.orthographicScale)
                {
                    camera.orthographicScale = nextScale;
                    const EditorSceneViewGroundPoint after = ScreenToEditorSceneViewGroundPoint(camera, input.viewport, input.mouseX, input.mouseY, 0.0f);
                    if (before.valid && after.valid)
                    {
                        camera.centerX += before.x - after.x;
                        camera.centerZ += before.z - after.z;
                    }
                    ++camera.revision;
                    result.changed = true;
                    result.status = FormatCameraStatus(camera, "Scene view zoom");
                }
            }
            return result;
        }

        if (input.pan2D && (input.mouseDeltaX != 0 || input.mouseDeltaY != 0) && RectUsable(input.viewport))
        {
            const float height = static_cast<float>(std::max<i32>(1, input.viewport.height));
            const float distance = CameraDistanceToFocus(camera);
            const float fov = DegreesToRadians(std::clamp(camera.fovYDegrees, MinFovY, MaxFovY));
            const float unitsPerPixel = std::max(0.0001f, (std::tan(fov * 0.5f) * 2.0f * distance) / height);
            const Vec3f forward = ForwardFromAngles(camera.yawDegrees, camera.pitchDegrees);
            const Vec3f right = RightFromYaw(camera.yawDegrees);
            const Vec3f up = UpFromAxes(forward, right);
            const Vec3f offset = Add(Multiply(right, -static_cast<float>(input.mouseDeltaX) * unitsPerPixel),
                                     Multiply(up, static_cast<float>(input.mouseDeltaY) * unitsPerPixel));
            OffsetCameraAndFocus(camera, offset);
            ++camera.revision;
            result.changed = true;
            result.status = FormatCameraStatus(camera, "Scene view panning");
        }

        if (input.orbit3D && (input.mouseDeltaX != 0 || input.mouseDeltaY != 0))
        {
            const Vec3f focus = CameraFocusPoint(camera);
            const float distance = CameraDistanceToFocus(camera);
            camera.yawDegrees += static_cast<float>(input.mouseDeltaX) * 0.25f;
            camera.pitchDegrees = std::clamp(camera.pitchDegrees - static_cast<float>(input.mouseDeltaY) * 0.25f, -85.0f, -5.0f);
            PositionCameraAtFocus(camera, focus, distance);
            ++camera.revision;
            result.changed = true;
            result.status = FormatCameraStatus(camera, "Scene view orbit");
        }

        if (input.mouseWheelDelta != 0 || input.dolly3D)
        {
            const Vec3f forward = ForwardFromAngles(camera.yawDegrees, camera.pitchDegrees);
            const float wheelSteps = static_cast<float>(input.mouseWheelDelta) / 120.0f;
            const double deltaSeconds = std::isfinite(input.deltaSeconds) ? input.deltaSeconds : 1.0 / 60.0;
            float distance = (input.dolly3D ? 1.0f : wheelSteps) * std::max(0.1f, camera.moveSpeed) * static_cast<float>(std::max(1.0 / 120.0, deltaSeconds * 6.0));
            const float focusDistance = CameraDistanceToFocus(camera);
            distance = std::clamp(distance, focusDistance - MaxFrameDistance, focusDistance - MinFrameDistance);
            if (std::fabs(distance) > Epsilon)
            {
                camera.positionX += forward.x * distance;
                camera.positionY += forward.y * distance;
                camera.positionZ += forward.z * distance;
                ++camera.revision;
                result.changed = true;
                result.status = FormatCameraStatus(camera, "Scene view dolly");
            }
        }

        return result;
    }

    EditorSceneViewOverlaySurface BuildEditorSceneViewOverlaySurface(const EditorSceneViewOverlayBuildInput& input)
    {
        EditorSceneViewOverlaySurface surface{};
        surface.viewport = input.viewport;
        if (!RectUsable(input.viewport))
        {
            surface.status = "Scene View overlay hidden: invalid viewport";
            return surface;
        }

        EditorSceneViewCamera camera = input.camera;
        SanitizeEditorSceneViewCamera(camera);

        surface.visible = true;
        const i32 x = OffsetCoordinate(input.viewport.x, 10);
        const i32 y = OffsetCoordinate(input.viewport.y, 10);
        const i32 rowHeight = 24;
        const i32 gap = 6;

        AddOverlayControl(surface, EditorSceneViewOverlayControl::Mode2D, {x, y, 42, rowHeight}, "2D", true, IsOverlayControlActive(EditorSceneViewOverlayControl::Mode2D, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::Mode3D, {OffsetCoordinate(x, 42 + gap), y, 42, rowHeight}, "3D", true, IsOverlayControlActive(EditorSceneViewOverlayControl::Mode3D, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::GridSnap, {OffsetCoordinate(x, 84 + gap * 2), y, 54, rowHeight}, "Snap", true, IsOverlayControlActive(EditorSceneViewOverlayControl::GridSnap, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::FocusSelection, {OffsetCoordinate(x, 138 + gap * 3), y, 58, rowHeight}, "Focus", input.hasSelection, false);
        AddOverlayControl(surface, EditorSceneViewOverlayControl::FrameAll, {OffsetCoordinate(x, 196 + gap * 4), y, 58, rowHeight}, "Frame", true, false);
        AddOverlayControl(surface, EditorSceneViewOverlayControl::ResetView, {OffsetCoordinate(x, 254 + gap * 5), y, 58, rowHeight}, "Reset", true, false);

        const i32 toolsY = OffsetCoordinate(y, rowHeight + gap);
        AddOverlayControl(surface, EditorSceneViewOverlayControl::ToolTranslate, {x, toolsY, 52, rowHeight}, "Move", true, IsOverlayControlActive(EditorSceneViewOverlayControl::ToolTranslate, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::ToolRotate, {OffsetCoordinate(x, 52 + gap), toolsY, 44, rowHeight}, "Rot", true, IsOverlayControlActive(EditorSceneViewOverlayControl::ToolRotate, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::ToolScale, {OffsetCoordinate(x, 96 + gap * 2), toolsY, 56, rowHeight}, "Scale", true, IsOverlayControlActive(EditorSceneViewOverlayControl::ToolScale, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::SpaceWorld, {OffsetCoordinate(x, 152 + gap * 3), toolsY, 62, rowHeight}, "World", true, IsOverlayControlActive(EditorSceneViewOverlayControl::SpaceWorld, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::SpaceLocal, {OffsetCoordinate(x, 214 + gap * 4), toolsY, 58, rowHeight}, "Local", true, IsOverlayControlActive(EditorSceneViewOverlayControl::SpaceLocal, input, camera));

        const i32 orientationSize = 104;
        const i32 orientationX = std::max(OffsetCoordinate(input.viewport.x, 10), OffsetCoordinate(RectRight(input.viewport), -orientationSize - 12));
        const i32 orientationY = OffsetCoordinate(input.viewport.y, 10);
        surface.orientationRect = {orientationX, orientationY, orientationSize, orientationSize};
        AddOverlayControl(surface, EditorSceneViewOverlayControl::OrientationAxisX, {OffsetCoordinate(orientationX, 72), OffsetCoordinate(orientationY, 40), 24, 24}, "X", true, IsOverlayControlActive(EditorSceneViewOverlayControl::OrientationAxisX, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::OrientationAxisY, {OffsetCoordinate(orientationX, 40), OffsetCoordinate(orientationY, 8), 24, 24}, "Y", true, IsOverlayControlActive(EditorSceneViewOverlayControl::OrientationAxisY, input, camera));
        AddOverlayControl(surface, EditorSceneViewOverlayControl::OrientationAxisZ, {OffsetCoordinate(orientationX, 16), OffsetCoordinate(orientationY, 72), 24, 24}, "Z", true, IsOverlayControlActive(EditorSceneViewOverlayControl::OrientationAxisZ, input, camera));

        std::ostringstream out;
        out << "Scene View overlay " << ToString(camera.mode)
            << " grid=" << (input.gridVisible ? "on" : "off")
            << " snap=" << (input.snapEnabled ? "on" : "off")
            << " tool=" << ToString(input.activeTool)
            << " space=" << ToString(input.activeSpace)
            << " selection=" << (input.hasSelection ? (input.selectedLabel.empty() ? "selection" : input.selectedLabel) : "none");
        surface.status = out.str();
        return surface;
    }

    EditorSceneViewOverlayHit HitTestEditorSceneViewOverlay(const EditorSceneViewOverlaySurface& surface, i32 screenX, i32 screenY)
    {
        EditorSceneViewOverlayHit hit{};
        if (!surface.visible)
        {
            return hit;
        }

        for (std::size_t index = 0; index < surface.controls.size(); ++index)
        {
            const EditorSceneViewOverlayControlDesc& desc = surface.controls[index];
            if (desc.visible && RectContains(desc.rect, screenX, screenY))
            {
                hit.control = desc.control;
                hit.command = desc.command;
                hit.itemIndex = index;
                hit.actionable = desc.enabled;
                hit.active = desc.active;
                hit.label = desc.label;
                return hit;
            }
        }
        return hit;
    }

    EditorSceneViewDiagnostics RunEditorSceneViewDiagnostics()
    {
        EditorSceneViewDiagnostics diagnostics{};
        EditorSceneViewCamera camera = MakeDefaultEditorSceneViewCamera();
        const EditorRect viewport{0, 0, 800, 600};

        const EditorSceneViewGroundPoint center2D = ScreenToEditorSceneViewGroundPoint(camera, viewport, 400, 300, 0.0f);
        diagnostics.ray2DOk = center2D.valid && std::fabs(center2D.x) < 0.001f && std::fabs(center2D.z) < 0.001f;

        EditorSceneViewNavigationInput zoom{};
        zoom.viewport = viewport;
        zoom.mouseX = 160;
        zoom.mouseY = 190;
        zoom.mouseWheelDelta = 120;
        const EditorSceneViewGroundPoint beforeZoom = ScreenToEditorSceneViewGroundPoint(camera, viewport, zoom.mouseX, zoom.mouseY, 0.0f);
        const EditorSceneViewNavigationResult zoomResult = ApplyEditorSceneViewNavigation(camera, zoom);
        const EditorSceneViewGroundPoint afterZoom = ScreenToEditorSceneViewGroundPoint(camera, viewport, zoom.mouseX, zoom.mouseY, 0.0f);
        diagnostics.pickingStabilityOk = zoomResult.changed
            && beforeZoom.valid
            && afterZoom.valid
            && std::fabs(beforeZoom.x - afterZoom.x) < 0.001f
            && std::fabs(beforeZoom.z - afterZoom.z) < 0.001f;

        const EditorSceneViewRay invalidRay = BuildEditorSceneViewRay(camera, {0, 0, 0, 600}, 0, 0);
        diagnostics.invalidInputOk = !invalidRay.valid && !IntersectEditorSceneViewGroundPlane(invalidRay, 0.0f).valid;

        SetEditorSceneViewMode(camera, EditorSceneViewMode::Mode3D);
        diagnostics.modeSwitchOk = camera.mode == EditorSceneViewMode::Mode3D && camera.projection == EditorSceneViewProjection::Perspective;
        const EditorSceneViewRay ray3D = BuildEditorSceneViewRay(camera, viewport, 400, 300);
        const EditorSceneViewGroundPoint center3D = IntersectEditorSceneViewGroundPlane(ray3D, camera.focusY);
        diagnostics.ray3DOk = ray3D.valid && ray3D.dirY < -0.01f;
        diagnostics.groundHitOk = center3D.valid
            && std::fabs(center3D.x - camera.centerX) < 0.001f
            && std::fabs(center3D.z - camera.centerZ) < 0.001f;

        EditorSceneViewNavigationInput navigation{};
        navigation.viewport = viewport;
        navigation.orbit3D = true;
        navigation.mouseDeltaX = 40;
        navigation.mouseDeltaY = -20;
        navigation.deltaSeconds = 1.0 / 60.0;
        const float previousYaw = camera.yawDegrees;
        const EditorSceneViewNavigationResult navigationResult = ApplyEditorSceneViewNavigation(camera, navigation);
        const EditorSceneViewGroundPoint orbitFocus = ScreenToEditorSceneViewGroundPoint(camera, viewport, 400, 300, camera.focusY);
        diagnostics.navigationOk = navigationResult.changed
            && camera.yawDegrees != previousYaw
            && orbitFocus.valid
            && std::fabs(orbitFocus.x - camera.centerX) < 0.001f
            && std::fabs(orbitFocus.z - camera.centerZ) < 0.001f;

        EditorSceneViewNavigationInput pan3D{};
        pan3D.viewport = viewport;
        pan3D.pan2D = true;
        pan3D.mouseDeltaX = 36;
        pan3D.mouseDeltaY = -24;
        const float previousFocusX = camera.centerX;
        const float previousFocusY = camera.focusY;
        const float previousFocusZ = camera.centerZ;
        const EditorSceneViewNavigationResult pan3DResult = ApplyEditorSceneViewNavigation(camera, pan3D);
        const EditorSceneViewGroundPoint panFocus = ScreenToEditorSceneViewGroundPoint(camera, viewport, 400, 300, camera.focusY);
        diagnostics.navigationOk = diagnostics.navigationOk
            && pan3DResult.changed
            && (std::fabs(camera.centerX - previousFocusX) > 0.001f || std::fabs(camera.focusY - previousFocusY) > 0.001f || std::fabs(camera.centerZ - previousFocusZ) > 0.001f)
            && panFocus.valid
            && std::fabs(panFocus.x - camera.centerX) < 0.001f
            && std::fabs(panFocus.z - camera.centerZ) < 0.001f;

        FocusEditorSceneViewCamera(camera, 5.0f, 2.0f, -3.0f);
        const EditorSceneViewGroundPoint focusedPoint = ScreenToEditorSceneViewGroundPoint(camera, viewport, 400, 300, 2.0f);
        diagnostics.focusOk = std::fabs(camera.centerX - 5.0f) < 0.001f
            && std::fabs(camera.focusY - 2.0f) < 0.001f
            && std::fabs(camera.centerZ + 3.0f) < 0.001f
            && focusedPoint.valid
            && std::fabs(focusedPoint.x - 5.0f) < 0.001f
            && std::fabs(focusedPoint.z + 3.0f) < 0.001f;

        const u64 previousRevision = camera.revision;
        ResetEditorSceneViewCamera(camera, EditorSceneViewMode::Mode3D);
        diagnostics.resetOk = camera.mode == EditorSceneViewMode::Mode3D
            && camera.projection == EditorSceneViewProjection::Perspective
            && camera.revision > previousRevision
            && camera.pitchDegrees < 0.0f;

        const EditorSceneViewBounds bounds{true, -4.0f, -1.0f, -2.0f, 6.0f, 3.0f, 8.0f};
        diagnostics.frameOk = FrameEditorSceneViewCamera(camera, bounds, viewport)
            && std::fabs(camera.centerX - 1.0f) < 0.001f
            && std::fabs(camera.focusY - 1.0f) < 0.001f
            && std::fabs(camera.centerZ - 3.0f) < 0.001f;

        SnapEditorSceneViewCameraToAxis(camera, EditorSceneViewAxis::Top);
        const EditorSceneViewRay topRay = BuildEditorSceneViewRay(camera, viewport, 400, 300);
        const bool topOk = topRay.valid && topRay.dirY < -0.99f;
        SnapEditorSceneViewCameraToAxis(camera, EditorSceneViewAxis::Front);
        const EditorSceneViewRay frontRay = BuildEditorSceneViewRay(camera, viewport, 400, 300);
        const bool frontOk = frontRay.valid && std::fabs(frontRay.dirY) < 0.001f && frontRay.dirZ > 0.99f;
        SnapEditorSceneViewCameraToAxis(camera, EditorSceneViewAxis::Right);
        const EditorSceneViewRay rightRay = BuildEditorSceneViewRay(camera, viewport, 400, 300);
        const bool rightOk = rightRay.valid && std::fabs(rightRay.dirY) < 0.001f && rightRay.dirX < -0.99f;
        diagnostics.axisSnapOk = topOk && frontOk && rightOk && camera.mode == EditorSceneViewMode::Mode3D;

        EditorSceneViewOverlayBuildInput overlayInput{};
        overlayInput.viewport = viewport;
        overlayInput.camera = camera;
        overlayInput.activeTool = CommandId::ToolRotate;
        overlayInput.activeSpace = CommandId::ToolSpaceLocal;
        overlayInput.gridVisible = true;
        overlayInput.snapEnabled = true;
        overlayInput.hasSelection = true;
        overlayInput.selectedLabel = "ProbeCube";
        const EditorSceneViewOverlaySurface overlay = BuildEditorSceneViewOverlaySurface(overlayInput);
        const EditorSceneViewOverlayControlDesc* axisX = FindOverlayControl(overlay, EditorSceneViewOverlayControl::OrientationAxisX);
        const EditorSceneViewOverlayControlDesc* focus = FindOverlayControl(overlay, EditorSceneViewOverlayControl::FocusSelection);
        const EditorSceneViewOverlayControlDesc* rotate = FindOverlayControl(overlay, EditorSceneViewOverlayControl::ToolRotate);
        const EditorSceneViewOverlayControlDesc* local = FindOverlayControl(overlay, EditorSceneViewOverlayControl::SpaceLocal);
        const EditorSceneViewOverlayControlDesc* snap = FindOverlayControl(overlay, EditorSceneViewOverlayControl::GridSnap);
        diagnostics.overlaySurfaceOk = overlay.visible
            && RectUsable(overlay.orientationRect)
            && overlay.controls.size() == 14
            && overlay.status.find("ProbeCube") != std::string::npos
            && rotate
            && rotate->active
            && local
            && local->active
            && snap
            && snap->active;
        if (axisX)
        {
            const EditorSceneViewOverlayHit axisHit = HitTestEditorSceneViewOverlay(overlay, axisX->rect.x + axisX->rect.width / 2, axisX->rect.y + axisX->rect.height / 2);
            diagnostics.overlayHitOk = axisHit.control == EditorSceneViewOverlayControl::OrientationAxisX
                && axisHit.command == CommandId::ViewAxisRight
                && axisHit.actionable
                && axisHit.active;
        }
        if (focus)
        {
            const EditorSceneViewOverlayHit focusHit = HitTestEditorSceneViewOverlay(overlay, focus->rect.x + focus->rect.width / 2, focus->rect.y + focus->rect.height / 2);
            diagnostics.overlayCommandOk = focusHit.control == EditorSceneViewOverlayControl::FocusSelection
                && focusHit.command == CommandId::FocusSelection
                && focusHit.actionable
                && !focusHit.active;
        }

        diagnostics.ok = diagnostics.modeSwitchOk
            && diagnostics.ray2DOk
            && diagnostics.ray3DOk
            && diagnostics.groundHitOk
            && diagnostics.navigationOk
            && diagnostics.focusOk
            && diagnostics.resetOk
            && diagnostics.frameOk
            && diagnostics.axisSnapOk
            && diagnostics.pickingStabilityOk
            && diagnostics.invalidInputOk
            && diagnostics.overlaySurfaceOk
            && diagnostics.overlayHitOk
            && diagnostics.overlayCommandOk;
        diagnostics.summary = FormatEditorSceneViewDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorSceneViewDiagnostics(const EditorSceneViewDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-scene-view modeSwitch=" << diagnostics.modeSwitchOk
            << " ray2D=" << diagnostics.ray2DOk
            << " ray3D=" << diagnostics.ray3DOk
            << " ground=" << diagnostics.groundHitOk
            << " navigation=" << diagnostics.navigationOk
            << " focus=" << diagnostics.focusOk
            << " reset=" << diagnostics.resetOk
            << " frame=" << diagnostics.frameOk
            << " axis=" << diagnostics.axisSnapOk
            << " stablePick=" << diagnostics.pickingStabilityOk
            << " invalidInput=" << diagnostics.invalidInputOk
            << " overlay=" << diagnostics.overlaySurfaceOk
            << " overlayHit=" << diagnostics.overlayHitOk
            << " overlayCommand=" << diagnostics.overlayCommandOk
            << " ok=" << diagnostics.ok;
        return out.str();
    }
}
