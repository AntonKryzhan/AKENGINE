#include <AK/Render/Renderer.hpp>

#include <AK/RHI/VulkanRHI.hpp>

#include <AK/Core/Log.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#endif

namespace AK
{
#if defined(_WIN32)
    namespace
    {
        constexpr int ToolbarHeight = 38;
        constexpr int HierarchyWidth = 260;
        constexpr int InspectorWidth = 340;
        constexpr int AssetBrowserHeight = 170;
        constexpr int ConsoleHeight = 150;
        constexpr int PanelGap = 6;
        constexpr int HeaderHeight = 29;
        constexpr int RowHeight = 22;
        constexpr int UnityMenuHeight = 22;
        constexpr int UnityTabHeight = 23;
        constexpr int UnityPanelPadding = 8;

        COLORREF Rgb(int r, int g, int b)
        {
            return RGB(r, g, b);
        }

        void Fill(HDC dc, const RECT& rect, COLORREF color)
        {
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        }

        void Stroke(HDC dc, const RECT& rect, COLORREF color)
        {
            HPEN pen = CreatePen(PS_SOLID, 1, color);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }

        void Line(HDC dc, int x0, int y0, int x1, int y1, COLORREF color)
        {
            HPEN pen = CreatePen(PS_SOLID, 1, color);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            MoveToEx(dc, x0, y0, nullptr);
            LineTo(dc, x1, y1);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }

        std::wstring Utf8ToWideForGdi(const std::string& value)
        {
            if (value.empty())
            {
                return {};
            }

            const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
            if (required > 0)
            {
                std::wstring wide(static_cast<std::size_t>(required), L'\0');
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), static_cast<int>(value.size()), wide.data(), required);
                return wide;
            }

            const int fallbackRequired = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
            if (fallbackRequired <= 0)
            {
                return L"?";
            }

            std::wstring wide(static_cast<std::size_t>(fallbackRequired), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), wide.data(), fallbackRequired);
            return wide;
        }

        void Text(HDC dc, RECT rect, const std::string& value, COLORREF color, UINT format = DT_LEFT | DT_SINGLELINE | DT_VCENTER)
        {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, color);
            const std::wstring wide = Utf8ToWideForGdi(value);
            DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &rect, format | DT_END_ELLIPSIS);
        }

        void TextDisabled(HDC dc, RECT rect, const std::string& value, UINT format = DT_LEFT | DT_SINGLELINE | DT_VCENTER)
        {
            Text(dc, rect, value, Rgb(116, 119, 128), format);
        }

        void SoftBorder(HDC dc, const RECT& rect)
        {
            Stroke(dc, rect, Rgb(43, 45, 52));
            Stroke(dc, {rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1}, Rgb(30, 31, 36));
        }

        void DrawSubtleSeparator(HDC dc, int x, int top, int bottom)
        {
            Line(dc, x, top, x, bottom, Rgb(26, 27, 31));
            Line(dc, x + 1, top, x + 1, bottom, Rgb(56, 58, 66));
        }

        void Label(HDC dc, RECT rect, const std::string& value)
        {
            Text(dc, rect, value, Rgb(224, 224, 224));
        }

        void Header(HDC dc, RECT rect, const std::string& value)
        {
            Fill(dc, rect, Rgb(38, 38, 42));
            Text(dc, {rect.left + 10, rect.top, rect.right - 8, rect.bottom}, value, Rgb(245, 245, 245));
        }

        void Panel(HDC dc, RECT rect, const std::string& title)
        {
            Fill(dc, rect, Rgb(30, 30, 34));
            Stroke(dc, rect, Rgb(64, 64, 70));
            Header(dc, {rect.left + 1, rect.top + 1, rect.right - 1, rect.top + HeaderHeight}, title);
        }

        void Button(HDC dc, RECT rect, const std::string& value)
        {
            Fill(dc, rect, Rgb(48, 50, 56));
            Stroke(dc, rect, Rgb(86, 90, 104));
            Text(dc, {rect.left + 8, rect.top, rect.right - 8, rect.bottom}, value, Rgb(232, 232, 236), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        void TextList(HDC dc, RECT rect, const std::vector<std::string>& lines, int maxLines)
        {
            const int firstLineTop = rect.top + HeaderHeight + 8;
            const int count = std::min<int>(static_cast<int>(lines.size()), maxLines);

            for (int i = 0; i < count; ++i)
            {
                const std::string& line = lines[static_cast<std::size_t>(i)];
                const bool selected = line.rfind("> ", 0) == 0;
                RECT lineRect{rect.left + 8, firstLineTop + i * RowHeight, rect.right - 8, firstLineTop + (i + 1) * RowHeight};

                if (selected)
                {
                    Fill(dc, lineRect, Rgb(54, 63, 82));
                }

                Text(dc, {lineRect.left + 2, lineRect.top, lineRect.right, lineRect.bottom}, line, selected ? Rgb(245, 245, 255) : Rgb(208, 208, 208));
            }

            if (lines.empty())
            {
                Text(dc, {rect.left + 10, firstLineTop, rect.right - 8, firstLineTop + RowHeight}, "Empty", Rgb(128, 128, 128));
            }
        }

        std::string CountText(const EditorFrameDesc& desc)
        {
            std::ostringstream out;
            out << "entities=" << desc.entityCount
                << " | mesh=" << desc.meshCount
                << " | cam=" << desc.cameraCount
                << " | light=" << desc.lightCount
                << " | bounds=" << desc.boundsCount
                << " | vis=" << desc.visibleCount
                << " | culled=" << desc.culledCount
                << " | assets=" << desc.assetCount
                << " | undo=" << desc.undoDepth
                << " | redo=" << desc.redoDepth
                << " | frame=" << desc.frameIndex;
            if (desc.dirty)
            {
                out << " | modified";
            }
            return out.str();
        }

        void DrawToolbar(HDC dc, RECT toolbar, const EditorFrameDesc& desc)
        {
            Fill(dc, toolbar, Rgb(34, 34, 38));
            Stroke(dc, {toolbar.left, toolbar.bottom - 1, toolbar.right, toolbar.bottom}, Rgb(67, 67, 72));

            Text(dc, {toolbar.left + 12, toolbar.top, toolbar.right - 16, toolbar.top + 18},
                 desc.title + "    " + CountText(desc), Rgb(236, 236, 236));

            int x = toolbar.left + 12;
            const int y = toolbar.top + 20;
            const int h = 16;
            Button(dc, {x, y, x + 58, y + h}, "New");
            x += 64;
            Button(dc, {x, y, x + 58, y + h}, "Mesh");
            x += 64;
            Button(dc, {x, y, x + 74, y + h}, "Camera");
            x += 80;
            Button(dc, {x, y, x + 58, y + h}, "Light");
            x += 64;
            Button(dc, {x, y, x + 68, y + h}, "Delete");
            x += 74;
            Button(dc, {x, y, x + 58, y + h}, "Save");
            x += 64;
            Button(dc, {x, y, x + 58, y + h}, "Load");
            x += 64;
            Button(dc, {x, y, x + 74, y + h}, "Rescan");
            x += 88;

            Text(dc, {x, toolbar.top + 19, toolbar.right - 16, toolbar.bottom},
                 "RHI v6.2 | Vulkan runtime | reversed-Z | camera-relative | RenderGraph | Ctrl+Z/Y",
                 Rgb(170, 170, 178));
        }


        struct ViewportProjectedPoint
        {
            bool visible = false;
            int x = 0;
            int y = 0;
            float depth = 0.0f;
        };

        float DegreesToRadians(float degrees)
        {
            return degrees * 3.14159265358979323846f / 180.0f;
        }

        ViewportProjectedPoint ProjectViewportPoint3D(RECT clip, const EditorFrameDesc& desc, float worldX, float worldY, float worldZ)
        {
            ViewportProjectedPoint projected{};
            const int height = std::max<int>(1, static_cast<int>(clip.bottom - clip.top));
            const float centerX = static_cast<float>((clip.left + clip.right) / 2);
            const float centerY = static_cast<float>((clip.top + clip.bottom) / 2);
            const float yaw = DegreesToRadians(desc.viewportYawDegrees);
            const float pitch = DegreesToRadians(desc.viewportPitchDegrees);
            const float cp = std::cos(pitch);
            const float forwardX = std::sin(yaw) * cp;
            const float forwardY = std::sin(pitch);
            const float forwardZ = std::cos(yaw) * cp;
            const float rightX = std::cos(yaw);
            const float rightY = 0.0f;
            const float rightZ = -std::sin(yaw);
            const float upX = forwardY * rightZ - forwardZ * rightY;
            const float upY = forwardZ * rightX - forwardX * rightZ;
            const float upZ = forwardX * rightY - forwardY * rightX;
            const float dx = worldX - desc.viewportCameraX;
            const float dy = worldY - desc.viewportCameraY;
            const float dz = worldZ - desc.viewportCameraZ;
            const float cameraX = dx * rightX + dy * rightY + dz * rightZ;
            const float cameraY = dx * upX + dy * upY + dz * upZ;
            const float cameraZ = dx * forwardX + dy * forwardY + dz * forwardZ;
            if (cameraZ <= 0.05f)
            {
                return projected;
            }
            const float fov = std::clamp(desc.viewportFovYDegrees, 25.0f, 120.0f);
            const float focal = (static_cast<float>(height) * 0.5f) / std::tan(DegreesToRadians(fov) * 0.5f);
            projected.x = static_cast<int>(std::round(centerX + cameraX / cameraZ * focal));
            projected.y = static_cast<int>(std::round(centerY - cameraY / cameraZ * focal));
            projected.depth = cameraZ;
            projected.visible = projected.x >= clip.left - 64 && projected.x <= clip.right + 64 && projected.y >= clip.top - 64 && projected.y <= clip.bottom + 64;
            return projected;
        }

        void DrawViewportGrid3D(HDC dc, RECT viewport, const EditorFrameDesc& desc, RECT clip)
        {
            (void)viewport;
            Fill(dc, clip, Rgb(18, 19, 23));
            Stroke(dc, clip, Rgb(55, 57, 65));

            const int gridRadius = 24;
            const int centerGX = static_cast<int>(std::round(desc.viewportCenterX));
            const int centerGZ = static_cast<int>(std::round(desc.viewportCenterZ));
            for (int gx = centerGX - gridRadius; gx <= centerGX + gridRadius; ++gx)
            {
                ViewportProjectedPoint a = ProjectViewportPoint3D(clip, desc, static_cast<float>(gx), 0.0f, static_cast<float>(centerGZ - gridRadius));
                ViewportProjectedPoint b = ProjectViewportPoint3D(clip, desc, static_cast<float>(gx), 0.0f, static_cast<float>(centerGZ + gridRadius));
                if (a.visible || b.visible)
                {
                    const bool major = gx % 5 == 0;
                    Line(dc, a.x, a.y, b.x, b.y, gx == 0 ? Rgb(80, 96, 130) : (major ? Rgb(48, 52, 62) : Rgb(34, 36, 43)));
                }
            }
            for (int gz = centerGZ - gridRadius; gz <= centerGZ + gridRadius; ++gz)
            {
                ViewportProjectedPoint a = ProjectViewportPoint3D(clip, desc, static_cast<float>(centerGX - gridRadius), 0.0f, static_cast<float>(gz));
                ViewportProjectedPoint b = ProjectViewportPoint3D(clip, desc, static_cast<float>(centerGX + gridRadius), 0.0f, static_cast<float>(gz));
                if (a.visible || b.visible)
                {
                    const bool major = gz % 5 == 0;
                    Line(dc, a.x, a.y, b.x, b.y, gz == 0 ? Rgb(80, 96, 130) : (major ? Rgb(48, 52, 62) : Rgb(34, 36, 43)));
                }
            }

            RECT horizon{clip.left + 8, clip.top + 8, clip.right - 8, clip.top + 30};
            Fill(dc, horizon, Rgb(28, 30, 36));
            Stroke(dc, horizon, Rgb(58, 62, 74));
            std::ostringstream overlayText;
            overlayText << "3D perspective | yaw " << static_cast<int>(desc.viewportYawDegrees)
                        << " pitch " << static_cast<int>(desc.viewportPitchDegrees)
                        << " | snap " << (desc.viewportSnapEnabled ? "ON" : "OFF");
            Text(dc, {horizon.left + 8, horizon.top, horizon.right - 8, horizon.bottom}, overlayText.str(), Rgb(188, 194, 210));

            RECT info{clip.left + 8, clip.bottom - 24, clip.right - 8, clip.bottom - 4};
            std::ostringstream out;
            out << "cam X=" << desc.viewportCameraX << " Y=" << desc.viewportCameraY << " Z=" << desc.viewportCameraZ << " FOV=" << static_cast<int>(desc.viewportFovYDegrees);
            if (desc.viewportPanning)
            {
                out << " | orbit/pan";
            }
            Text(dc, info, out.str(), Rgb(142, 148, 162), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }

        void DrawViewportGrid(HDC dc, RECT viewport, const EditorFrameDesc& desc)
        {
            const int left = viewport.left + 12;
            const int top = viewport.top + HeaderHeight + 12;
            const int right = viewport.right - 12;
            const int bottom = viewport.bottom - 12;

            if (desc.viewportPerspective || desc.viewportMode == "3D")
            {
                DrawViewportGrid3D(dc, viewport, desc, {left, top, right, bottom});
                return;
            }

            Fill(dc, {left, top, right, bottom}, Rgb(20, 21, 25));
            Stroke(dc, {left, top, right, bottom}, Rgb(55, 57, 65));

            const int centerX = (left + right) / 2;
            const int centerY = (top + bottom) / 2;
            const float scale = std::clamp(desc.viewportScale, 8.0f, 160.0f);
            const float worldLeft = desc.viewportCenterX + static_cast<float>(left - centerX) / scale;
            const float worldRight = desc.viewportCenterX + static_cast<float>(right - centerX) / scale;
            const float worldTop = desc.viewportCenterZ - static_cast<float>(top - centerY) / scale;
            const float worldBottom = desc.viewportCenterZ - static_cast<float>(bottom - centerY) / scale;

            const int firstX = static_cast<int>(std::floor(worldLeft));
            const int lastX = static_cast<int>(std::ceil(worldRight));
            for (int gx = firstX; gx <= lastX; ++gx)
            {
                const int x = centerX + static_cast<int>(std::round((static_cast<float>(gx) - desc.viewportCenterX) * scale));
                const bool major = gx % 5 == 0;
                Line(dc, x, top, x, bottom, gx == 0 ? Rgb(72, 86, 112) : (major ? Rgb(42, 45, 54) : Rgb(32, 34, 40)));
            }

            const int firstZ = static_cast<int>(std::floor(worldBottom));
            const int lastZ = static_cast<int>(std::ceil(worldTop));
            for (int gz = firstZ; gz <= lastZ; ++gz)
            {
                const int y = centerY - static_cast<int>(std::round((static_cast<float>(gz) - desc.viewportCenterZ) * scale));
                const bool major = gz % 5 == 0;
                Line(dc, left, y, right, y, gz == 0 ? Rgb(72, 86, 112) : (major ? Rgb(42, 45, 54) : Rgb(32, 34, 40)));
            }

            RECT overlay{left + 8, top + 8, left + 360, top + 30};
            Fill(dc, overlay, Rgb(28, 30, 36));
            Stroke(dc, overlay, Rgb(58, 62, 74));

            std::ostringstream overlayText;
            overlayText << desc.viewportMode << " " << desc.viewportProjection << " | XZ top-down | snap " << (desc.viewportSnapEnabled ? "ON" : "OFF");
            if (desc.viewportSnapEnabled)
            {
                overlayText << " " << desc.viewportSnapStep << "u";
            }
            Text(dc, {overlay.left + 8, overlay.top, overlay.right - 8, overlay.bottom}, overlayText.str(), Rgb(188, 194, 210));

            RECT info{left + 8, bottom - 24, right - 8, bottom - 4};
            std::ostringstream out;
            out << "view X=" << desc.viewportCenterX << " Z=" << desc.viewportCenterZ << " zoom=" << static_cast<int>(scale) << "px/u";
            if (desc.viewportDragging)
            {
                out << " | dragging entity";
            }
            if (desc.viewportPanning)
            {
                out << " | panning";
            }
            Text(dc, info, out.str(), Rgb(142, 148, 162), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }

        const char* ViewportKindTag(EditorViewportItemKind kind)
        {
            switch (kind)
            {
                case EditorViewportItemKind::Mesh:
                    return "[M]";
                case EditorViewportItemKind::Camera:
                    return "[C]";
                case EditorViewportItemKind::Light:
                    return "[L]";
                default:
                    return "[E]";
            }
        }

        COLORREF ViewportKindFill(EditorViewportItemKind kind, bool selected)
        {
            if (selected)
            {
                return Rgb(96, 136, 220);
            }

            switch (kind)
            {
                case EditorViewportItemKind::Mesh:
                    return Rgb(126, 134, 146);
                case EditorViewportItemKind::Camera:
                    return Rgb(88, 128, 164);
                case EditorViewportItemKind::Light:
                    return Rgb(160, 142, 84);
                default:
                    return Rgb(118, 118, 128);
            }
        }

        void DrawViewportItems(HDC dc, RECT viewport, const EditorFrameDesc& desc)
        {
            const int left = viewport.left + 12;
            const int top = viewport.top + HeaderHeight + 12;
            const int right = viewport.right - 12;
            const int bottom = viewport.bottom - 12;
            const int centerX = (left + right) / 2;
            const int centerY = (top + bottom) / 2;
            const float scale = std::clamp(desc.viewportScale, 8.0f, 160.0f);

            for (const EditorViewportItem& item : desc.viewportItems)
            {
                int x = centerX + static_cast<int>(std::round((item.x - desc.viewportCenterX) * scale));
                int y = centerY - static_cast<int>(std::round((item.z - desc.viewportCenterZ) * scale));
                if (desc.viewportPerspective || desc.viewportMode == "3D")
                {
                    const ViewportProjectedPoint projected = ProjectViewportPoint3D({left, top, right, bottom}, desc, item.x, item.y, item.z);
                    if (!projected.visible)
                    {
                        continue;
                    }
                    x = projected.x;
                    y = projected.y;
                }

                if (x < left || x > right || y < top || y > bottom)
                {
                    continue;
                }

                if (item.hasBounds && !(desc.viewportPerspective || desc.viewportMode == "3D"))
                {
                    const int bx0 = centerX + static_cast<int>(std::round((item.minX - desc.viewportCenterX) * scale));
                    const int bx1 = centerX + static_cast<int>(std::round((item.maxX - desc.viewportCenterX) * scale));
                    const int by0 = centerY - static_cast<int>(std::round((item.maxZ - desc.viewportCenterZ) * scale));
                    const int by1 = centerY - static_cast<int>(std::round((item.minZ - desc.viewportCenterZ) * scale));
                    RECT boundsRect{std::min(bx0, bx1), std::min(by0, by1), std::max(bx0, bx1), std::max(by0, by1)};
                    Stroke(dc, boundsRect, item.culled ? Rgb(74, 54, 54) : (item.selected ? Rgb(84, 120, 190) : Rgb(62, 70, 82)));
                }

                const int radius = item.selected ? 8 : 5;
                RECT marker{x - radius, y - radius, x + radius, y + radius};
                Fill(dc, marker, ViewportKindFill(item.kind, item.selected));
                Stroke(dc, marker, item.selected ? Rgb(205, 220, 255) : Rgb(180, 180, 188));

                if (item.kind == EditorViewportItemKind::Camera)
                {
                    Line(dc, x, y, x + 16, y - 10, Rgb(152, 184, 214));
                    Line(dc, x, y, x + 16, y + 10, Rgb(152, 184, 214));
                    Line(dc, x + 16, y - 10, x + 16, y + 10, Rgb(152, 184, 214));
                }
                else if (item.kind == EditorViewportItemKind::Light)
                {
                    Line(dc, x - 12, y, x + 12, y, Rgb(202, 184, 116));
                    Line(dc, x, y - 12, x, y + 12, Rgb(202, 184, 116));
                }

                if (item.selected)
                {
                    Line(dc, x - 18, y, x + 18, y, Rgb(130, 170, 255));
                    Line(dc, x, y - 18, x, y + 18, Rgb(130, 170, 255));
                    Stroke(dc, {x - 15, y - 15, x + 15, y + 15}, Rgb(205, 220, 255));
                }

                const std::string label = std::string(ViewportKindTag(item.kind)) + " " + item.name + (item.culled ? " (culled)" : "");
                Text(dc, {x + 10, y - 10, x + 240, y + 12}, label, item.selected ? Rgb(235, 240, 255) : Rgb(190, 190, 198));
            }
        }

        RECT ToWinRect(const EditorShellRect& rect)
        {
            return {rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
        }

        RECT Inset(RECT rect, int amount)
        {
            return {rect.left + amount, rect.top + amount, rect.right - amount, rect.bottom - amount};
        }

        bool RectIsUsable(RECT rect)
        {
            return rect.right > rect.left && rect.bottom > rect.top;
        }

        int RectWidth(RECT rect)
        {
            return static_cast<int>(rect.right - rect.left);
        }

        int RectHeight(RECT rect)
        {
            return static_cast<int>(rect.bottom - rect.top);
        }

        int ClampScrollOffsetPixels(int offsetPixels, int contentPixels, int viewportPixels)
        {
            const int maxOffset = std::max<int>(0, contentPixels - viewportPixels);
            return std::clamp(offsetPixels, 0, maxOffset);
        }

        class ScopedClip final
        {
        public:
            ScopedClip(HDC dc, RECT rect)
                : mDc(dc)
            {
                if (mDc && RectIsUsable(rect))
                {
                    mSaved = SaveDC(mDc);
                    IntersectClipRect(mDc, rect.left, rect.top, rect.right, rect.bottom);
                }
            }

            ~ScopedClip()
            {
                if (mDc && mSaved > 0)
                {
                    RestoreDC(mDc, mSaved);
                }
            }

            ScopedClip(const ScopedClip&) = delete;
            ScopedClip& operator=(const ScopedClip&) = delete;

        private:
            HDC mDc = nullptr;
            int mSaved = 0;
        };

        void DrawVerticalScrollbar(HDC dc, RECT track, int contentPixels, int viewportPixels, int offsetPixels)
        {
            if (!RectIsUsable(track) || contentPixels <= viewportPixels || viewportPixels <= 0)
            {
                return;
            }

            Fill(dc, track, Rgb(34, 35, 40));
            Stroke(dc, track, Rgb(47, 50, 58));
            const int trackHeight = std::max<int>(1, RectHeight(track));
            const int thumbHeight = std::clamp(static_cast<int>((static_cast<double>(viewportPixels) / static_cast<double>(std::max<int>(1, contentPixels))) * static_cast<double>(trackHeight)), 22, trackHeight);
            const int maxOffset = std::max<int>(1, contentPixels - viewportPixels);
            const int travel = std::max<int>(0, trackHeight - thumbHeight);
            const int top = track.top + static_cast<int>((static_cast<double>(ClampScrollOffsetPixels(offsetPixels, contentPixels, viewportPixels)) / static_cast<double>(maxOffset)) * static_cast<double>(travel));
            RECT thumb{track.left + 2, top + 1, track.right - 2, top + thumbHeight - 1};
            Fill(dc, thumb, Rgb(78, 82, 94));
            Stroke(dc, thumb, Rgb(100, 105, 120));
        }

        void TextListBodyScrolled(HDC dc, RECT rect, const std::vector<std::string>& lines, int scrollPixels)
        {
            if (!RectIsUsable(rect))
            {
                return;
            }

            const int contentPixels = static_cast<int>(lines.size()) * RowHeight;
            const int viewportPixels = std::max<int>(0, RectHeight(rect) - 8);
            const int scroll = ClampScrollOffsetPixels(scrollPixels, contentPixels, viewportPixels);
            RECT clip{rect.left, rect.top + 4, rect.right - (contentPixels > viewportPixels ? 12 : 0), rect.bottom - 4};
            ScopedClip clipScope(dc, clip);

            const int firstIndex = std::max<int>(0, scroll / RowHeight);
            const int offsetY = -(scroll % RowHeight);
            const int visibleRows = std::max<int>(0, (RectHeight(clip) + RowHeight - 1) / RowHeight + 1);
            const int count = std::min<int>(static_cast<int>(lines.size()) - firstIndex, visibleRows);
            const int firstLineTop = clip.top + offsetY;

            for (int i = 0; i < count; ++i)
            {
                const int index = firstIndex + i;
                if (index < 0 || index >= static_cast<int>(lines.size()))
                {
                    continue;
                }
                const std::string& line = lines[static_cast<std::size_t>(index)];
                const bool selected = line.rfind("> ", 0) == 0;
                RECT lineRect{clip.left + 6, firstLineTop + i * RowHeight, clip.right - 6, firstLineTop + (i + 1) * RowHeight};
                if (selected)
                {
                    Fill(dc, lineRect, Rgb(55, 77, 117));
                    Line(dc, lineRect.left, lineRect.top, lineRect.left, lineRect.bottom, Rgb(94, 140, 225));
                }
                Text(dc, {lineRect.left + 7, lineRect.top, lineRect.right - 4, lineRect.bottom}, line, selected ? Rgb(248, 250, 255) : Rgb(194, 197, 205));
            }

            if (lines.empty())
            {
                TextDisabled(dc, {clip.left + 12, clip.top, clip.right - 8, clip.top + RowHeight}, "Empty");
            }

            DrawVerticalScrollbar(dc, {rect.right - 11, rect.top + 5, rect.right - 3, rect.bottom - 5}, contentPixels, viewportPixels, scroll);
        }

        void TextListBody(HDC dc, RECT rect, const std::vector<std::string>& lines, int maxLines)
        {
            (void)maxLines;
            TextListBodyScrolled(dc, rect, lines, 0);
        }


        void DrawLayoutMenuBar(HDC dc, RECT rect, const EditorFrameDesc& desc)
        {
            Fill(dc, rect, Rgb(31, 31, 35));
            Line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, Rgb(18, 18, 21));

            int x = rect.left + 10;
            const char* menus[] = {"File", "Edit", "Assets", "GameObject", "Component", "Window", "Help"};
            const int widths[] = {46, 44, 58, 94, 92, 68, 48};
            for (int i = 0; i < 7; ++i)
            {
                RECT item{x, rect.top, x + widths[i], rect.bottom};
                Text(dc, item, menus[i], Rgb(214, 216, 222));
                x += widths[i];
            }

            Text(dc, {rect.right - 520, rect.top, rect.right - 12, rect.bottom},
                 desc.sceneName.empty() ? "AK Engine" : (desc.sceneName + " - AK Engine"),
                 Rgb(138, 142, 152), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }


        void DrawLayoutToolbar(HDC dc, RECT rect, const EditorFrameDesc& desc)
        {
            Fill(dc, rect, Rgb(44, 45, 50));
            Line(dc, rect.left, rect.top, rect.right, rect.top, Rgb(62, 64, 70));
            Line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, Rgb(20, 21, 24));

            int previousRight = rect.left + 8;
            for (const EditorShellToolbarButtonDesc& button : desc.shellLayout.toolbarButtons)
            {
                RECT buttonRect = ToWinRect(button.rect);
                if (!RectIsUsable(buttonRect))
                {
                    continue;
                }

                if (buttonRect.left - previousRight > 10)
                {
                    DrawSubtleSeparator(dc, buttonRect.left - 7, rect.top + 7, rect.bottom - 7);
                }
                previousRight = buttonRect.right;

                const bool dropdownButton = button.label == "Create" || button.label == "Mesh" || button.label == "Camera" || button.label == "Light";
                const COLORREF fill = button.active ? Rgb(72, 96, 142) : (button.enabled ? Rgb(54, 56, 63) : Rgb(42, 43, 48));
                const COLORREF stroke = button.destructive ? Rgb(121, 70, 70) : (button.active ? Rgb(98, 132, 198) : Rgb(74, 78, 88));
                Fill(dc, buttonRect, fill);
                Stroke(dc, buttonRect, stroke);
                RECT labelRect = Inset(buttonRect, 5);
                if (dropdownButton)
                {
                    labelRect.right -= 10;
                }
                Text(dc, labelRect, button.label, button.enabled ? Rgb(231, 232, 236) : Rgb(118, 121, 130), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                if (dropdownButton)
                {
                    Text(dc, {buttonRect.right - 13, buttonRect.top, buttonRect.right - 3, buttonRect.bottom}, "v", Rgb(146, 152, 164), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }
            }

            RECT search{rect.right - 310, rect.top + 7, rect.right - 14, rect.bottom - 7};
            const int playW = 46;
            const int playH = std::max<int>(20, static_cast<int>(rect.bottom - rect.top) - 12);
            const int playY = rect.top + (rect.bottom - rect.top - playH) / 2;
            int center = (rect.left + rect.right) / 2;
            const int minCenter = previousRight + playW * 2 + 18;
            center = std::max<int>(center, minCenter);
            if (center + playW * 2 + 10 < search.left)
            {
                RECT play{center - playW - 2, playY, center - 2, playY + playH};
                RECT pause{center + 2, playY, center + 2 + playW, playY + playH};
                RECT step{center + 6 + playW, playY, center + 6 + playW * 2, playY + playH};
                Fill(dc, play, Rgb(52, 55, 63)); Stroke(dc, play, Rgb(82, 86, 98)); Text(dc, play, "Play", Rgb(220, 224, 232), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                Fill(dc, pause, Rgb(52, 55, 63)); Stroke(dc, pause, Rgb(82, 86, 98)); Text(dc, pause, "Pause", Rgb(220, 224, 232), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                Fill(dc, step, Rgb(52, 55, 63)); Stroke(dc, step, Rgb(82, 86, 98)); Text(dc, step, "Step", Rgb(220, 224, 232), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }

            Fill(dc, search, Rgb(37, 38, 43));
            Stroke(dc, search, Rgb(67, 70, 80));
            Text(dc, {search.left + 12, search.top, search.right - 10, search.bottom}, "Search commands, assets, entities...", Rgb(132, 136, 146));
        }


        void DrawLayoutStatusBar(HDC dc, RECT rect, const EditorFrameDesc& desc)
        {
            Fill(dc, rect, Rgb(32, 34, 38));
            Line(dc, rect.left, rect.top, rect.right, rect.top, Rgb(54, 57, 64));
            const std::string left = desc.shellLayout.statusLeft.empty() ? desc.statusLine : desc.shellLayout.statusLeft;
            const std::string right = desc.shellLayout.statusRight.empty() ? CountText(desc) : desc.shellLayout.statusRight;
            Text(dc, {rect.left + 9, rect.top, rect.right / 2, rect.bottom}, left, Rgb(207, 211, 219));
            Text(dc, {rect.right / 2, rect.top, rect.right - 10, rect.bottom}, right, Rgb(135, 141, 154), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }


        void DrawLayoutTabs(HDC dc, const EditorFrameDesc& desc)
        {
            for (const EditorShellPanelDesc& panel : desc.shellLayout.panels)
            {
                if (!panel.visible)
                {
                    continue;
                }

                RECT tab = ToWinRect(panel.tabRect);
                if (!RectIsUsable(tab))
                {
                    continue;
                }

                const COLORREF fill = panel.active ? Rgb(46, 47, 53) : Rgb(35, 36, 41);
                const COLORREF textColor = panel.active ? Rgb(230, 232, 238) : Rgb(142, 147, 158);
                Fill(dc, tab, fill);
                Line(dc, tab.left, tab.top, tab.right, tab.top, Rgb(57, 59, 66));
                Line(dc, tab.left, tab.bottom - 1, tab.right, tab.bottom - 1, panel.active ? Rgb(46, 47, 53) : Rgb(25, 26, 30));
                Line(dc, tab.right - 1, tab.top + 4, tab.right - 1, tab.bottom - 5, Rgb(24, 25, 29));
                Text(dc, {tab.left + 10, tab.top, tab.right - 10, tab.bottom}, panel.title.empty() ? panel.name : panel.title, textColor);
                if (panel.focused)
                {
                    Line(dc, tab.left + 4, tab.bottom - 2, tab.right - 4, tab.bottom - 2, Rgb(73, 132, 228));
                }
            }
        }


        const EditorShellPanelDesc* FindShellPanel(const EditorFrameDesc& desc, const char* name)
        {
            for (const EditorShellPanelDesc& panel : desc.shellLayout.panels)
            {
                if (panel.name == name)
                {
                    return &panel;
                }
            }
            return nullptr;
        }

        int BodyLineCapacity(RECT body)
        {
            const int height = static_cast<int>(body.bottom - body.top);
            return std::max(1, (height - 16) / RowHeight);
        }

        void DrawPanelBodyBackground(HDC dc, RECT body, bool focused, bool viewport = false)
        {
            Fill(dc, body, viewport ? Rgb(18, 19, 23) : Rgb(46, 47, 53));
            Stroke(dc, body, Rgb(38, 40, 46));
            Line(dc, body.left + 1, body.top, body.right - 1, body.top, Rgb(57, 59, 67));
            if (focused)
            {
                Line(dc, body.left + 1, body.top + 1, body.left + 1, body.bottom - 2, Rgb(73, 132, 228));
            }
        }

        void DrawPropertyHeader(HDC dc, RECT body, const std::string& title, const std::string& subtitle)
        {
            RECT header{body.left, body.top, body.right, body.top + 34};
            Fill(dc, header, Rgb(52, 53, 59));
            Line(dc, header.left, header.bottom - 1, header.right, header.bottom - 1, Rgb(35, 36, 42));
            Text(dc, {header.left + 10, header.top + 2, header.right - 10, header.top + 18}, title, Rgb(230, 232, 238));
            Text(dc, {header.left + 10, header.top + 17, header.right - 10, header.bottom}, subtitle, Rgb(138, 143, 155));
        }

        void DrawUnityOverlay(HDC dc, const EditorShellOverlayDesc& overlay)
        {
            RECT overlayRect = ToWinRect(overlay.rect);
            if (!RectIsUsable(overlayRect))
            {
                return;
            }

            const bool transformTools = overlay.label == "Transform Tools";
            const bool orientationGizmo = overlay.label == "Orientation Gizmo";
            const bool frameStats = overlay.label == "Frame Stats";

            if (transformTools)
            {
                RECT column = overlayRect;
                column.right = column.left + std::min<int>(38, static_cast<int>(column.right - column.left));
                Fill(dc, column, Rgb(38, 40, 47));
                Stroke(dc, column, Rgb(72, 76, 88));
                const char* tools[] = {"Q", "W", "E", "R"};
                for (int i = 0; i < 4; ++i)
                {
                    RECT item{column.left + 5, column.top + 6 + i * 31, column.right - 5, column.top + 31 + i * 31};
                    Fill(dc, item, i == 1 ? Rgb(60, 86, 138) : Rgb(49, 52, 60));
                    Stroke(dc, item, i == 1 ? Rgb(98, 143, 225) : Rgb(78, 82, 94));
                    Text(dc, item, tools[i], Rgb(231, 234, 240), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }
                return;
            }

            if (orientationGizmo)
            {
                Fill(dc, overlayRect, Rgb(35, 38, 46));
                Stroke(dc, overlayRect, Rgb(70, 75, 88));
                const int cx = (overlayRect.left + overlayRect.right) / 2;
                const int cy = (overlayRect.top + overlayRect.bottom) / 2;
                Line(dc, cx, cy, cx + 36, cy, Rgb(210, 90, 90));
                Line(dc, cx, cy, cx, cy - 36, Rgb(110, 205, 120));
                Line(dc, cx, cy, cx - 25, cy + 25, Rgb(100, 140, 230));
                Text(dc, {cx + 39, cy - 8, cx + 54, cy + 8}, "X", Rgb(230, 130, 130));
                Text(dc, {cx - 8, cy - 56, cx + 8, cy - 38}, "Y", Rgb(150, 235, 160), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                Text(dc, {cx - 42, cy + 25, cx - 24, cy + 43}, "Z", Rgb(130, 165, 245), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                return;
            }

            if (frameStats)
            {
                Fill(dc, overlayRect, Rgb(34, 36, 43));
                Stroke(dc, overlayRect, Rgb(64, 68, 80));
                Text(dc, {overlayRect.left + 8, overlayRect.top + 4, overlayRect.right - 8, overlayRect.top + 22}, "Stats", Rgb(205, 210, 220));
                Text(dc, {overlayRect.left + 8, overlayRect.top + 23, overlayRect.right - 8, overlayRect.bottom - 4}, "frame / draw / memory", Rgb(132, 138, 152));
                return;
            }

            Fill(dc, overlayRect, overlay.interactive ? Rgb(43, 46, 55) : Rgb(38, 40, 47));
            Stroke(dc, overlayRect, overlay.interactive ? Rgb(78, 93, 128) : Rgb(64, 68, 80));
            Text(dc, Inset(overlayRect, 8), overlay.label, Rgb(194, 200, 214));
        }


        std::string LowerAscii(std::string value)
        {
            for (char& c : value)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return value;
        }

        std::string TrimLeftAscii(std::string value)
        {
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            {
                value.erase(value.begin());
            }
            return value;
        }

        std::string CleanHierarchyLabel(std::string value)
        {
            value = TrimLeftAscii(value);
            if (value.rfind("> ", 0) == 0)
            {
                value.erase(0, 2);
            }
            value = TrimLeftAscii(value);
            if (value.size() > 4 && value[0] == '[' && value[2] == ']' && value[3] == ' ')
            {
                value.erase(0, 4);
            }
            return value;
        }

        std::string CompactAssetLabel(std::string value)
        {
            const std::size_t slash = value.find_last_of("/\\");
            if (slash != std::string::npos && slash + 1 < value.size())
            {
                value = value.substr(slash + 1);
            }
            const std::size_t doubleSpace = value.find("  ");
            if (doubleSpace != std::string::npos && doubleSpace + 2 < value.size())
            {
                const std::string tail = value.substr(doubleSpace + 2);
                const std::size_t tailSlash = tail.find_last_of("/\\");
                if (tailSlash != std::string::npos && tailSlash + 1 < tail.size())
                {
                    value = tail.substr(tailSlash + 1);
                }
            }
            return value.empty() ? "Asset" : value;
        }

        enum class GdiIconKind
        {
            Entity,
            Mesh,
            Camera,
            Light,
            Folder,
            Asset,
            Material,
            Shader,
            Transform,
            Bounds,
            Console,
            Search,
            Plus,
            Eye,
            Lock
        };

        GdiIconKind GuessHierarchyGdiIcon(const std::string& raw)
        {
            const std::string lower = LowerAscii(raw);
            if (lower.find("[m]") != std::string::npos || lower.find("mesh") != std::string::npos || lower.find("cube") != std::string::npos)
            {
                return GdiIconKind::Mesh;
            }
            if (lower.find("[c]") != std::string::npos || lower.find("camera") != std::string::npos)
            {
                return GdiIconKind::Camera;
            }
            if (lower.find("[l]") != std::string::npos || lower.find("light") != std::string::npos)
            {
                return GdiIconKind::Light;
            }
            return GdiIconKind::Entity;
        }

        GdiIconKind GuessAssetGdiIcon(const std::string& raw)
        {
            const std::string lower = LowerAscii(raw);
            if (lower.find("shader") != std::string::npos || lower.find(".slang") != std::string::npos || lower.find(".hlsl") != std::string::npos)
            {
                return GdiIconKind::Shader;
            }
            if (lower.find("material") != std::string::npos || lower.find("mat") != std::string::npos)
            {
                return GdiIconKind::Material;
            }
            if (lower.find("mesh") != std::string::npos || lower.find("cube") != std::string::npos || lower.find(".obj") != std::string::npos || lower.find(".fbx") != std::string::npos)
            {
                return GdiIconKind::Mesh;
            }
            if (lower.find("assets") != std::string::npos && lower.find(".") == std::string::npos)
            {
                return GdiIconKind::Folder;
            }
            return GdiIconKind::Asset;
        }

        COLORREF IconColor(GdiIconKind kind)
        {
            switch (kind)
            {
                case GdiIconKind::Mesh: return Rgb(118, 142, 185);
                case GdiIconKind::Camera: return Rgb(88, 148, 192);
                case GdiIconKind::Light: return Rgb(206, 174, 92);
                case GdiIconKind::Folder: return Rgb(205, 156, 72);
                case GdiIconKind::Material: return Rgb(126, 162, 126);
                case GdiIconKind::Shader: return Rgb(165, 126, 188);
                case GdiIconKind::Transform: return Rgb(94, 136, 215);
                case GdiIconKind::Bounds: return Rgb(118, 118, 134);
                case GdiIconKind::Console: return Rgb(142, 144, 152);
                case GdiIconKind::Plus: return Rgb(114, 178, 115);
                case GdiIconKind::Eye: return Rgb(126, 154, 195);
                case GdiIconKind::Lock: return Rgb(176, 143, 80);
                case GdiIconKind::Search: return Rgb(126, 132, 148);
                case GdiIconKind::Asset: return Rgb(132, 142, 158);
                default: return Rgb(126, 130, 145);
            }
        }

        void DrawGdiIcon(HDC dc, RECT rect, GdiIconKind kind)
        {
            const COLORREF color = IconColor(kind);
            RECT icon{rect.left, rect.top, rect.right, rect.bottom};
            if (kind == GdiIconKind::Folder)
            {
                RECT tab{icon.left + 1, icon.top + 3, icon.left + 10, icon.top + 8};
                RECT body{icon.left + 1, icon.top + 7, icon.right - 1, icon.bottom - 2};
                Fill(dc, tab, Rgb(170, 122, 56));
                Fill(dc, body, color);
                Stroke(dc, {icon.left + 1, icon.top + 3, icon.right - 1, icon.bottom - 2}, Rgb(114, 86, 48));
                return;
            }
            if (kind == GdiIconKind::Camera)
            {
                Fill(dc, {icon.left + 2, icon.top + 4, icon.right - 4, icon.bottom - 3}, color);
                POINT pts[3] = {{icon.right - 4, icon.top + 6}, {icon.right, icon.top + 3}, {icon.right, icon.bottom - 3}};
                HBRUSH brush = CreateSolidBrush(color);
                HGDIOBJ oldBrush = SelectObject(dc, brush);
                Polygon(dc, pts, 3);
                SelectObject(dc, oldBrush);
                DeleteObject(brush);
                Stroke(dc, icon, Rgb(48, 62, 78));
                return;
            }
            if (kind == GdiIconKind::Light)
            {
                HBRUSH brush = CreateSolidBrush(color);
                HGDIOBJ oldBrush = SelectObject(dc, brush);
                Ellipse(dc, icon.left + 3, icon.top + 3, icon.right - 3, icon.bottom - 3);
                SelectObject(dc, oldBrush);
                DeleteObject(brush);
                Line(dc, icon.left + 1, (icon.top + icon.bottom) / 2, icon.right - 1, (icon.top + icon.bottom) / 2, color);
                Line(dc, (icon.left + icon.right) / 2, icon.top + 1, (icon.left + icon.right) / 2, icon.bottom - 1, color);
                return;
            }
            if (kind == GdiIconKind::Search)
            {
                HBRUSH brush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
                HPEN pen = CreatePen(PS_SOLID, 2, color);
                HGDIOBJ oldPen = SelectObject(dc, pen);
                HGDIOBJ oldBrush = SelectObject(dc, brush);
                Ellipse(dc, icon.left + 3, icon.top + 3, icon.left + 12, icon.top + 12);
                MoveToEx(dc, icon.left + 11, icon.top + 11, nullptr);
                LineTo(dc, icon.right - 2, icon.bottom - 2);
                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
                return;
            }
            Fill(dc, icon, color);
            Stroke(dc, icon, Rgb(58, 61, 70));
            const char* glyph = "E";
            switch (kind)
            {
                case GdiIconKind::Mesh: glyph = "M"; break;
                case GdiIconKind::Material: glyph = "O"; break;
                case GdiIconKind::Shader: glyph = "{}"; break;
                case GdiIconKind::Transform: glyph = "T"; break;
                case GdiIconKind::Bounds: glyph = "B"; break;
                case GdiIconKind::Asset: glyph = "A"; break;
                case GdiIconKind::Plus: glyph = "+"; break;
                case GdiIconKind::Eye: glyph = "v"; break;
                case GdiIconKind::Lock: glyph = "L"; break;
                default: break;
            }
            Text(dc, icon, glyph, Rgb(22, 24, 28), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        void DrawSearchBox(HDC dc, RECT rect, const std::string& placeholder)
        {
            Fill(dc, rect, Rgb(35, 36, 41));
            Stroke(dc, rect, Rgb(66, 69, 78));
            DrawGdiIcon(dc, {rect.left + 7, rect.top + 4, rect.left + 21, rect.bottom - 4}, GdiIconKind::Search);
            Text(dc, {rect.left + 28, rect.top, rect.right - 8, rect.bottom}, placeholder, Rgb(120, 125, 136));
        }

        void DrawUnityMiniButton(HDC dc, RECT rect, const std::string& label, bool active = false)
        {
            Fill(dc, rect, active ? Rgb(64, 87, 132) : Rgb(48, 50, 57));
            Stroke(dc, rect, active ? Rgb(100, 143, 220) : Rgb(72, 75, 84));
            Text(dc, rect, label, active ? Rgb(245, 248, 255) : Rgb(205, 209, 218), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        void DrawDropdownButton(HDC dc, RECT rect, const std::string& label)
        {
            Fill(dc, rect, Rgb(45, 47, 54));
            Stroke(dc, rect, Rgb(72, 76, 86));
            Text(dc, {rect.left + 8, rect.top, rect.right - 18, rect.bottom}, label, Rgb(210, 214, 222));
            Text(dc, {rect.right - 16, rect.top, rect.right - 4, rect.bottom}, "v", Rgb(144, 150, 162), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        void DrawHierarchyTreeRow(HDC dc, RECT row, const std::string& rawLine, int depth)
        {
            const bool selected = rawLine.rfind("> ", 0) == 0;
            const std::string label = CleanHierarchyLabel(rawLine);
            if (selected)
            {
                Fill(dc, row, Rgb(55, 76, 112));
                Line(dc, row.left, row.top, row.left, row.bottom, Rgb(91, 140, 226));
            }
            else
            {
                Fill(dc, row, Rgb(46, 47, 53));
            }
            const int x = row.left + 7 + depth * 15;
            Text(dc, {x, row.top, x + 12, row.bottom}, depth == 0 ? "v" : "", Rgb(135, 141, 154), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            DrawGdiIcon(dc, {x + 16, row.top + 3, x + 31, row.bottom - 3}, GuessHierarchyGdiIcon(rawLine));
            Text(dc, {x + 38, row.top, row.right - 42, row.bottom}, label, selected ? Rgb(244, 247, 255) : Rgb(201, 205, 214));
            RECT eye{row.right - 38, row.top + 3, row.right - 24, row.bottom - 3};
            DrawGdiIcon(dc, eye, GdiIconKind::Eye);
        }

        void DrawHierarchyPanelContent(HDC dc, RECT body, const EditorFrameDesc& desc)
        {
            RECT header{body.left, body.top, body.right, body.top + 30};
            Fill(dc, header, Rgb(48, 49, 55));
            DrawUnityMiniButton(dc, {body.left + 7, body.top + 5, body.left + 31, body.top + 25}, "+");
            DrawSearchBox(dc, {body.left + 38, body.top + 5, body.right - 58, body.top + 25}, "Search");
            DrawUnityMiniButton(dc, {body.right - 52, body.top + 5, body.right - 30, body.top + 25}, "L");
            DrawUnityMiniButton(dc, {body.right - 26, body.top + 5, body.right - 6, body.top + 25}, "...");

            RECT sceneRow{body.left, body.top + 32, body.right - 12, body.top + 32 + RowHeight};
            DrawHierarchyTreeRow(dc, sceneRow, "[S] Sandbox", 0);

            RECT listClip{body.left, sceneRow.bottom, body.right - 12, body.bottom - 4};
            const int viewportPixels = std::max<int>(0, RectHeight(listClip));
            const int contentPixels = static_cast<int>(desc.hierarchyItems.size()) * RowHeight;
            const int scroll = ClampScrollOffsetPixels(desc.hierarchyScrollPixels, contentPixels, viewportPixels);
            const int firstIndex = std::max<int>(0, scroll / RowHeight);
            const int offsetY = -(scroll % RowHeight);
            const int visibleRows = std::max<int>(0, (viewportPixels + RowHeight - 1) / RowHeight + 1);
            const int count = std::min<int>(static_cast<int>(desc.hierarchyItems.size()) - firstIndex, visibleRows);

            ScopedClip clip(dc, listClip);
            for (int i = 0; i < count; ++i)
            {
                const int index = firstIndex + i;
                RECT row{listClip.left, listClip.top + offsetY + i * RowHeight, listClip.right, listClip.top + offsetY + (i + 1) * RowHeight};
                DrawHierarchyTreeRow(dc, row, desc.hierarchyItems[static_cast<std::size_t>(index)], 1);
            }
            DrawVerticalScrollbar(dc, {body.right - 10, listClip.top + 1, body.right - 3, listClip.bottom - 1}, contentPixels, viewportPixels, scroll);
        }

        void DrawInspectorObjectHeader(HDC dc, RECT body, const EditorFrameDesc& desc)
        {
            RECT header{body.left, body.top, body.right, body.top + 68};
            Fill(dc, header, Rgb(51, 52, 58));
            Line(dc, header.left, header.bottom - 1, header.right, header.bottom - 1, Rgb(34, 35, 40));
            DrawGdiIcon(dc, {header.left + 10, header.top + 10, header.left + 30, header.top + 30}, GdiIconKind::Entity);
            RECT enabled{header.left + 38, header.top + 12, header.left + 52, header.top + 26};
            Fill(dc, enabled, Rgb(63, 66, 74));
            Stroke(dc, enabled, Rgb(92, 96, 108));
            Text(dc, enabled, "x", Rgb(218, 222, 230), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            RECT nameField{header.left + 58, header.top + 8, header.right - 84, header.top + 31};
            Fill(dc, nameField, Rgb(38, 39, 44));
            Stroke(dc, nameField, Rgb(68, 72, 82));
            Text(dc, {nameField.left + 8, nameField.top, nameField.right - 8, nameField.bottom}, desc.selectedEntityName.empty() ? "None" : desc.selectedEntityName, Rgb(228, 231, 238));
            RECT staticBox{header.right - 76, header.top + 10, header.right - 12, header.top + 29};
            Fill(dc, staticBox, Rgb(45, 47, 54));
            Stroke(dc, staticBox, Rgb(68, 72, 82));
            Text(dc, staticBox, "Static", Rgb(185, 190, 202), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            Text(dc, {header.left + 12, header.top + 39, header.left + 44, header.top + 59}, "Tag", Rgb(151, 157, 170));
            DrawDropdownButton(dc, {header.left + 48, header.top + 37, header.left + 140, header.top + 61}, "Untagged");
            Text(dc, {header.left + 154, header.top + 39, header.left + 194, header.top + 59}, "Layer", Rgb(151, 157, 170));
            DrawDropdownButton(dc, {header.left + 196, header.top + 37, header.right - 12, header.top + 61}, "Default");
        }

        void DrawComponentHeaderRow(HDC dc, RECT rect, GdiIconKind icon, const std::string& title, bool expanded = true)
        {
            Fill(dc, rect, Rgb(57, 58, 64));
            Stroke(dc, rect, Rgb(76, 79, 90));
            Text(dc, {rect.left + 6, rect.top, rect.left + 20, rect.bottom}, expanded ? "v" : ">", Rgb(162, 168, 180), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            RECT toggle{rect.left + 24, rect.top + 6, rect.left + 36, rect.bottom - 6};
            Fill(dc, toggle, Rgb(71, 74, 83));
            Stroke(dc, toggle, Rgb(102, 108, 122));
            Text(dc, toggle, "x", Rgb(223, 226, 234), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            DrawGdiIcon(dc, {rect.left + 43, rect.top + 4, rect.left + 58, rect.bottom - 4}, icon);
            Text(dc, {rect.left + 64, rect.top, rect.right - 52, rect.bottom}, title, Rgb(228, 231, 238));
            DrawUnityMiniButton(dc, {rect.right - 46, rect.top + 4, rect.right - 25, rect.bottom - 4}, "?");
            DrawUnityMiniButton(dc, {rect.right - 23, rect.top + 4, rect.right - 5, rect.bottom - 4}, "...");
        }

        void DrawFieldBox(HDC dc, RECT rect, const std::string& value)
        {
            Fill(dc, rect, Rgb(37, 38, 43));
            Stroke(dc, rect, Rgb(64, 67, 76));
            Text(dc, {rect.left + 6, rect.top, rect.right - 4, rect.bottom}, value, Rgb(215, 219, 228));
        }

        void DrawVec3InspectorRow(HDC dc, RECT row, const std::string& label, const std::string& x, const std::string& y, const std::string& z)
        {
            Text(dc, {row.left, row.top, row.left + 68, row.bottom}, label, Rgb(168, 174, 188));
            const int fieldLeft = row.left + 70;
            const int fieldWidth = std::max<int>(34, (static_cast<int>(row.right - fieldLeft) - 10) / 3);
            RECT rx{fieldLeft, row.top + 1, fieldLeft + fieldWidth, row.bottom - 1};
            RECT ry{rx.right + 5, row.top + 1, rx.right + 5 + fieldWidth, row.bottom - 1};
            RECT rz{ry.right + 5, row.top + 1, row.right, row.bottom - 1};
            Text(dc, {rx.left + 4, rx.top, rx.left + 17, rx.bottom}, "X", Rgb(196, 118, 118)); DrawFieldBox(dc, {rx.left + 16, rx.top, rx.right, rx.bottom}, x);
            Text(dc, {ry.left + 4, ry.top, ry.left + 17, ry.bottom}, "Y", Rgb(128, 196, 135)); DrawFieldBox(dc, {ry.left + 16, ry.top, ry.right, ry.bottom}, y);
            Text(dc, {rz.left + 4, rz.top, rz.left + 17, rz.bottom}, "Z", Rgb(122, 153, 222)); DrawFieldBox(dc, {rz.left + 16, rz.top, rz.right, rz.bottom}, z);
        }

        void DrawInspectorPanelContent(HDC dc, RECT body, const EditorFrameDesc& desc)
        {
            DrawInspectorObjectHeader(dc, body, desc);
            RECT contentClip{body.left, body.top + 72, body.right - 12, body.bottom - 38};
            const int estimatedContentPixels = std::max<int>(360, 118 + static_cast<int>(desc.inspectorLines.size()) * 22);
            const int viewportPixels = std::max<int>(0, RectHeight(contentClip));
            const int scroll = ClampScrollOffsetPixels(desc.inspectorScrollPixels, estimatedContentPixels, viewportPixels);

            ScopedClip clip(dc, contentClip);
            int y = contentClip.top + 6 - scroll;
            RECT comp{body.left + 7, y, contentClip.right - 7, y + 25};
            DrawComponentHeaderRow(dc, comp, GdiIconKind::Transform, "Transform");
            y += 31;
            DrawVec3InspectorRow(dc, {body.left + 14, y, contentClip.right - 14, y + 21}, "Position", "0.00", "0.00", "0.00");
            y += 24;
            DrawVec3InspectorRow(dc, {body.left + 14, y, contentClip.right - 14, y + 21}, "Rotation", "0.00", "0.00", "0.00");
            y += 24;
            DrawVec3InspectorRow(dc, {body.left + 14, y, contentClip.right - 14, y + 21}, "Scale", "1.00", "1.00", "1.00");
            y += 34;

            bool drewMesh = false;
            bool drewCamera = false;
            bool drewLight = false;
            bool drewBounds = false;
            auto visibleBand = [&](int top, int height)
            {
                return top + height >= contentClip.top && top <= contentClip.bottom;
            };

            for (const std::string& line : desc.inspectorLines)
            {
                if (!drewMesh && line.find("MeshComponent") != std::string::npos)
                {
                    if (visibleBand(y, 58))
                    {
                        DrawComponentHeaderRow(dc, {body.left + 7, y, contentClip.right - 7, y + 25}, GdiIconKind::Mesh, "Mesh Renderer");
                        Text(dc, {body.left + 14, y + 31, body.left + 86, y + 52}, "Mesh", Rgb(168, 174, 188));
                        DrawFieldBox(dc, {body.left + 88, y + 31, contentClip.right - 14, y + 52}, "builtin:cube");
                    }
                    y += 65;
                    drewMesh = true;
                }
                if (!drewCamera && line.find("CameraComponent") != std::string::npos)
                {
                    if (visibleBand(y, 58))
                    {
                        DrawComponentHeaderRow(dc, {body.left + 7, y, contentClip.right - 7, y + 25}, GdiIconKind::Camera, "Camera");
                        Text(dc, {body.left + 14, y + 31, body.left + 86, y + 52}, "FOV", Rgb(168, 174, 188));
                        DrawFieldBox(dc, {body.left + 88, y + 31, contentClip.right - 14, y + 52}, "60");
                    }
                    y += 65;
                    drewCamera = true;
                }
                if (!drewLight && line.find("LightComponent") != std::string::npos)
                {
                    if (visibleBand(y, 58))
                    {
                        DrawComponentHeaderRow(dc, {body.left + 7, y, contentClip.right - 7, y + 25}, GdiIconKind::Light, "Light");
                        Text(dc, {body.left + 14, y + 31, body.left + 86, y + 52}, "Intensity", Rgb(168, 174, 188));
                        DrawFieldBox(dc, {body.left + 88, y + 31, contentClip.right - 14, y + 52}, "1.00");
                    }
                    y += 65;
                    drewLight = true;
                }
                if (!drewBounds && line.find("BoundsComponent") != std::string::npos)
                {
                    if (visibleBand(y, 58))
                    {
                        DrawComponentHeaderRow(dc, {body.left + 7, y, contentClip.right - 7, y + 25}, GdiIconKind::Bounds, "Bounds");
                        Text(dc, {body.left + 14, y + 31, body.left + 86, y + 52}, "Status", Rgb(168, 174, 188));
                        DrawFieldBox(dc, {body.left + 88, y + 31, contentClip.right - 14, y + 52}, "valid / visible");
                    }
                    y += 65;
                    drewBounds = true;
                }
            }

            DrawVerticalScrollbar(dc, {body.right - 10, contentClip.top + 1, body.right - 3, contentClip.bottom - 1}, estimatedContentPixels, viewportPixels, scroll);
            DrawUnityMiniButton(dc, {body.left + 18, body.bottom - 30, body.right - 18, body.bottom - 7}, "+ Add Component");
        }

        void DrawSceneToolbarContent(HDC dc, RECT body, const EditorFrameDesc& desc)
        {
            RECT toolbar{body.left + 8, body.top + 6, body.right - 8, body.top + 31};
            Fill(dc, toolbar, Rgb(32, 34, 40));
            Stroke(dc, toolbar, Rgb(62, 66, 78));
            int x = toolbar.left + 6;
            const int y0 = toolbar.top + 3;
            const int y1 = toolbar.bottom - 3;
            DrawDropdownButton(dc, {x, y0, x + 72, y1}, "Scene"); x += 78;
            if (x + 58 < toolbar.right - 90) { DrawDropdownButton(dc, {x, y0, x + 54, y1}, desc.viewportMode); x += 60; }
            if (x + 90 < toolbar.right - 90) { DrawDropdownButton(dc, {x, y0, x + 86, y1}, desc.viewportSnapEnabled ? "Snap On" : "Snap Off"); x += 92; }
            if (x + 104 < toolbar.right - 90) { DrawDropdownButton(dc, {x, y0, x + 100, y1}, desc.viewportProjection); x += 106; }
            if (x + 90 < toolbar.right - 90) { DrawDropdownButton(dc, {x, y0, x + 86, y1}, "Shaded"); }
            DrawDropdownButton(dc, {toolbar.right - 86, y0, toolbar.right - 8, y1}, "Gizmos");
        }

        void DrawAssetGridItem(HDC dc, RECT cell, const std::string& raw, bool selected)
        {
            if (selected)
            {
                Fill(dc, cell, Rgb(52, 72, 108));
                Stroke(dc, cell, Rgb(93, 139, 222));
            }
            const std::string label = CompactAssetLabel(raw);
            RECT iconRect{cell.left + (cell.right - cell.left) / 2 - 18, cell.top + 8, cell.left + (cell.right - cell.left) / 2 + 18, cell.top + 44};
            DrawGdiIcon(dc, iconRect, GuessAssetGdiIcon(raw));
            Text(dc, {cell.left + 4, iconRect.bottom + 4, cell.right - 4, cell.bottom - 3}, label, selected ? Rgb(245, 248, 255) : Rgb(194, 199, 210), DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
        }

        void DrawProjectBrowserContent(HDC dc, RECT body, const EditorFrameDesc& desc)
        {
            RECT header{body.left, body.top, body.right, body.top + 31};
            Fill(dc, header, Rgb(48, 49, 55));
            DrawSearchBox(dc, {body.left + 8, body.top + 5, body.left + 220, body.top + 25}, "Search Project");
            DrawDropdownButton(dc, {body.right - 178, body.top + 5, body.right - 100, body.top + 25}, "All");
            DrawUnityMiniButton(dc, {body.right - 94, body.top + 5, body.right - 66, body.top + 25}, "List");
            DrawUnityMiniButton(dc, {body.right - 60, body.top + 5, body.right - 32, body.top + 25}, "Grid", true);
            DrawUnityMiniButton(dc, {body.right - 26, body.top + 5, body.right - 6, body.top + 25}, "+");

            const int treeWidth = std::min<int>(190, std::max<int>(130, static_cast<int>(body.right - body.left) / 4));
            RECT tree{body.left, body.top + 31, body.left + treeWidth, body.bottom};
            RECT grid{tree.right + 1, body.top + 31, body.right, body.bottom};
            Fill(dc, tree, Rgb(42, 43, 49));
            Fill(dc, grid, Rgb(46, 47, 53));
            Line(dc, tree.right, tree.top, tree.right, tree.bottom, Rgb(31, 32, 37));
            {
                ScopedClip treeClip(dc, tree);
                DrawHierarchyTreeRow(dc, {tree.left, tree.top + 5, tree.right, tree.top + 5 + RowHeight}, "[F] Favorites", 0);
                DrawHierarchyTreeRow(dc, {tree.left, tree.top + 5 + RowHeight, tree.right, tree.top + 5 + RowHeight * 2}, "> [F] Assets", 0);
                DrawHierarchyTreeRow(dc, {tree.left, tree.top + 5 + RowHeight * 2, tree.right, tree.top + 5 + RowHeight * 3}, "[F] Packages", 0);
            }

            RECT breadcrumb{grid.left + 10, grid.top + 6, grid.right - 16, grid.top + 26};
            Text(dc, breadcrumb, "Assets", Rgb(172, 178, 190));
            const int cellW = 84;
            const int cellH = 74;
            RECT gridClip{grid.left + 10, grid.top + 30, grid.right - 13, grid.bottom - 4};
            const int cols = std::max<int>(1, (static_cast<int>(gridClip.right - gridClip.left)) / cellW);
            const int totalRows = static_cast<int>((desc.assetItems.size() + static_cast<std::size_t>(cols) - 1) / static_cast<std::size_t>(cols));
            const int contentPixels = totalRows * cellH;
            const int viewportPixels = std::max<int>(0, RectHeight(gridClip));
            const int scroll = ClampScrollOffsetPixels(desc.projectGridScrollPixels, contentPixels, viewportPixels);
            const int firstRow = std::max<int>(0, scroll / cellH);
            const int firstIndex = firstRow * cols;
            const int offsetY = -(scroll % cellH);
            const int visibleRows = std::max<int>(0, (viewportPixels + cellH - 1) / cellH + 1);
            const int maxCells = visibleRows * cols;
            const int count = std::min<int>(static_cast<int>(desc.assetItems.size()) - firstIndex, maxCells);
            {
                ScopedClip gridClipScope(dc, gridClip);
                for (int i = 0; i < count; ++i)
                {
                    const int itemIndex = firstIndex + i;
                    const int local = itemIndex - firstRow * cols;
                    const int col = local % cols;
                    const int row = local / cols;
                    RECT cell{gridClip.left + col * cellW, gridClip.top + offsetY + row * cellH, gridClip.left + col * cellW + cellW - 8, gridClip.top + offsetY + row * cellH + cellH - 6};
                    DrawAssetGridItem(dc, cell, desc.assetItems[static_cast<std::size_t>(itemIndex)], itemIndex == 0);
                }
                if (desc.assetItems.empty())
                {
                    TextDisabled(dc, {grid.left + 16, grid.top + 34, grid.right - 16, grid.top + 58}, "No imported assets yet");
                }
            }
            DrawVerticalScrollbar(dc, {grid.right - 10, gridClip.top, grid.right - 3, gridClip.bottom}, contentPixels, viewportPixels, scroll);
        }


        COLORREF DiagnosticsSeverityColor(const std::string& severity)
        {
            if (severity == "error")
            {
                return Rgb(232, 92, 92);
            }
            if (severity == "warning")
            {
                return Rgb(232, 176, 72);
            }
            if (severity == "success")
            {
                return Rgb(98, 202, 126);
            }
            if (severity == "trace")
            {
                return Rgb(116, 122, 134);
            }
            return Rgb(174, 183, 198);
        }

        COLORREF DiagnosticsSeverityBackColor(const std::string& severity)
        {
            if (severity == "error")
            {
                return Rgb(72, 38, 42);
            }
            if (severity == "warning")
            {
                return Rgb(70, 56, 34);
            }
            if (severity == "success")
            {
                return Rgb(37, 62, 44);
            }
            if (severity == "trace")
            {
                return Rgb(42, 43, 48);
            }
            return Rgb(44, 47, 54);
        }

        void DrawDiagnosticsChip(HDC dc, RECT rect, const std::string& label, std::uint32_t count, bool active, bool warning, bool error)
        {
            const COLORREF back = error ? Rgb(72, 38, 42) : (warning ? Rgb(70, 56, 34) : (active ? Rgb(52, 60, 76) : Rgb(43, 45, 51)));
            const COLORREF border = active ? Rgb(94, 126, 178) : Rgb(66, 69, 78);
            Fill(dc, rect, back);
            Stroke(dc, rect, border);
            std::ostringstream text;
            text << label << " " << count;
            Text(dc, {rect.left + 7, rect.top, rect.right - 7, rect.bottom}, text.str(), error ? Rgb(255, 176, 176) : (warning ? Rgb(255, 212, 128) : Rgb(214, 220, 232)), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        void DrawDiagnosticsMetricTile(HDC dc, RECT rect, const EditorDiagnosticsMetricRenderDesc& metric)
        {
            Fill(dc, rect, DiagnosticsSeverityBackColor(metric.severity));
            Stroke(dc, rect, metric.pinned ? Rgb(92, 122, 176) : Rgb(62, 65, 74));
            Text(dc, {rect.left + 8, rect.top + 4, rect.right - 8, rect.top + 20}, metric.label, Rgb(182, 190, 204), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            Text(dc, {rect.left + 8, rect.top + 20, rect.right - 8, rect.bottom - 4}, metric.value, DiagnosticsSeverityColor(metric.severity), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }

        void DrawDiagnosticsRow(HDC dc, RECT rect, const EditorDiagnosticsRowRenderDesc& row)
        {
            const bool prominent = row.unread || row.selected || row.active;
            Fill(dc, rect, prominent ? Rgb(48, 54, 68) : Rgb(37, 38, 44));
            Line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, Rgb(28, 29, 34));
            RECT iconRect{rect.left + 7, rect.top + 8, rect.left + 29, rect.top + 30};
            Fill(dc, iconRect, DiagnosticsSeverityBackColor(row.severity));
            Stroke(dc, iconRect, DiagnosticsSeverityColor(row.severity));
            Text(dc, iconRect, row.icon, DiagnosticsSeverityColor(row.severity), DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            RECT titleRect{rect.left + 38, rect.top + 4, rect.right - 96, rect.top + 22};
            RECT subRect{rect.left + 38, rect.top + 22, rect.right - 96, rect.top + 39};
            Text(dc, titleRect, row.title.empty() ? row.message : row.title, row.unread ? Rgb(245, 248, 255) : Rgb(218, 222, 230));
            Text(dc, subRect, row.subtitle.empty() ? row.source : row.subtitle, Rgb(144, 151, 164));
            if (!row.detail.empty() && RectHeight(rect) > 46)
            {
                Text(dc, {rect.left + 38, rect.top + 38, rect.right - 96, rect.bottom - 4}, row.detail, Rgb(124, 132, 146));
            }

            if (!row.badges.empty())
            {
                Text(dc, {rect.right - 90, rect.top + 5, rect.right - 8, rect.top + 23}, row.badges, Rgb(160, 168, 184), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
            }
            Text(dc, {rect.right - 90, rect.bottom - 22, rect.right - 8, rect.bottom - 5}, row.kind, Rgb(112, 119, 132), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }



        COLORREF GizmoHandleFill(const EditorViewportGizmoHandleRenderDesc& handle)
        {
            if (handle.active)
            {
                return Rgb(82, 118, 186);
            }
            if (handle.hovered)
            {
                return Rgb(64, 82, 122);
            }
            if (handle.label.find('X') != std::string::npos)
            {
                return Rgb(146, 70, 66);
            }
            if (handle.label.find('Y') != std::string::npos)
            {
                return Rgb(70, 132, 82);
            }
            if (handle.label.find('Z') != std::string::npos)
            {
                return Rgb(74, 100, 154);
            }
            return Rgb(72, 78, 90);
        }

        void DrawViewportGizmo(HDC dc, RECT viewport, const EditorFrameDesc& desc)
        {
            const EditorViewportGizmoRenderDesc& gizmo = desc.transformGizmo;
            if (!gizmo.visible)
            {
                return;
            }

            const int clipLeft = viewport.left + 12;
            const int clipTop = viewport.top + HeaderHeight + 12;
            const int clipRight = viewport.right - 12;
            const int clipBottom = viewport.bottom - 12;
            if (clipRight <= clipLeft || clipBottom <= clipTop)
            {
                return;
            }

            ScopedClip clip(dc, {clipLeft, clipTop, clipRight, clipBottom});
            const int ox = gizmo.originX;
            const int oy = gizmo.originY;

            for (const EditorViewportGizmoHandleRenderDesc& handle : gizmo.handles)
            {
                RECT rect{handle.rect.x, handle.rect.y, handle.rect.x + handle.rect.width, handle.rect.y + handle.rect.height};
                if (!RectIsUsable(rect))
                {
                    continue;
                }

                const int hx = (rect.left + rect.right) / 2;
                const int hy = (rect.top + rect.bottom) / 2;
                COLORREF axisColor = GizmoHandleFill(handle);
                if (handle.label == "XZ")
                {
                    Stroke(dc, {ox - 11, oy - 11, ox + 11, oy + 11}, handle.active ? Rgb(218, 232, 255) : Rgb(176, 188, 210));
                }
                else
                {
                    Line(dc, ox, oy, hx, hy, axisColor);
                    if (handle.label == "X")
                    {
                        Line(dc, hx - 6, hy - 5, hx, hy, axisColor);
                        Line(dc, hx - 6, hy + 5, hx, hy, axisColor);
                    }
                    else if (handle.label == "Z")
                    {
                        Line(dc, hx - 5, hy + 6, hx, hy, axisColor);
                        Line(dc, hx + 5, hy + 6, hx, hy, axisColor);
                    }
                    else if (handle.label == "Y")
                    {
                        Line(dc, hx - 5, hy + 5, hx, hy, axisColor);
                        Line(dc, hx + 5, hy + 5, hx, hy, axisColor);
                    }
                }
            }

            for (const EditorViewportGizmoHandleRenderDesc& handle : gizmo.handles)
            {
                RECT rect{handle.rect.x, handle.rect.y, handle.rect.x + handle.rect.width, handle.rect.y + handle.rect.height};
                if (!RectIsUsable(rect))
                {
                    continue;
                }

                Fill(dc, rect, GizmoHandleFill(handle));
                Stroke(dc, rect, handle.active ? Rgb(226, 238, 255) : (handle.hovered ? Rgb(196, 210, 238) : Rgb(42, 45, 54)));
                Text(dc, rect, handle.label, Rgb(242, 245, 250), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }

            RECT label{ox + 16, oy + 16, ox + 280, oy + 38};
            Text(dc, label, gizmo.dragging ? ("Gizmo drag: " + gizmo.activeHandle) : ("Gizmo " + gizmo.mode), Rgb(204, 214, 236));
        }


        void DrawDiagnosticsPanelContent(HDC dc, RECT body, const EditorFrameDesc& desc)
        {
            Fill(dc, body, Rgb(34, 35, 40));
            const int width = std::max<int>(0, static_cast<int>(body.right - body.left));
            RECT top{body.left + 8, body.top + 6, body.right - 8, body.top + 28};
            DrawSearchBox(dc, {top.left, top.top, std::min<int>(top.left + 220, top.right - 310), top.bottom}, "Search diagnostics");

            int chipX = std::max<int>(top.left + 230, top.right - 520);
            DrawDiagnosticsChip(dc, {chipX, top.top, chipX + 76, top.bottom}, "Unread", desc.diagnosticsUnreadCount, desc.diagnosticsUnreadCount > 0, desc.diagnosticsUnreadCount > 0, false);
            chipX += 82;
            DrawDiagnosticsChip(dc, {chipX, top.top, chipX + 84, top.bottom}, "Warn", desc.diagnosticsWarningCount, desc.diagnosticsWarningCount > 0, desc.diagnosticsWarningCount > 0, false);
            chipX += 90;
            DrawDiagnosticsChip(dc, {chipX, top.top, chipX + 82, top.bottom}, "Error", desc.diagnosticsErrorCount, desc.diagnosticsErrorCount > 0, false, desc.diagnosticsErrorCount > 0);
            chipX += 88;
            DrawDiagnosticsChip(dc, {chipX, top.top, chipX + 78, top.bottom}, "Tasks", desc.diagnosticsActiveTaskCount, desc.diagnosticsActiveTaskCount > 0, false, false);

            RECT metricsRect{body.left + 8, body.top + 34, body.right - 8, body.top + 76};
            const int metricCount = std::min<int>(static_cast<int>(desc.diagnosticsMetrics.size()), std::max<int>(1, width / 128));
            const int metricGap = 6;
            const int metricW = metricCount > 0 ? std::max<int>(96, (RectWidth(metricsRect) - metricGap * (metricCount - 1)) / metricCount) : 0;
            for (int i = 0; i < metricCount; ++i)
            {
                RECT tile{metricsRect.left + i * (metricW + metricGap), metricsRect.top, metricsRect.left + i * (metricW + metricGap) + metricW, metricsRect.bottom};
                DrawDiagnosticsMetricTile(dc, tile, desc.diagnosticsMetrics[static_cast<std::size_t>(i)]);
            }

            RECT list{body.left + 8, metricsRect.bottom + 8, body.right - 11, body.bottom - 24};
            RECT status{body.left + 8, body.bottom - 22, body.right - 8, body.bottom - 3};
            Text(dc, status, desc.diagnosticsStatus.empty() ? "Diagnostics runtime view" : desc.diagnosticsStatus, Rgb(132, 140, 154));

            const int rowH = 48;
            const int contentPixels = static_cast<int>(desc.diagnosticsRows.size()) * rowH;
            const int viewportPixels = std::max<int>(0, RectHeight(list));
            const int scroll = ClampScrollOffsetPixels(desc.consoleScrollPixels, contentPixels, viewportPixels);
            const int firstRow = std::max<int>(0, scroll / rowH);
            const int yOffset = -(scroll % rowH);
            const int visibleRows = std::max<int>(0, (viewportPixels + rowH - 1) / rowH + 1);
            const int endRow = std::min<int>(static_cast<int>(desc.diagnosticsRows.size()), firstRow + visibleRows);
            {
                ScopedClip clip(dc, list);
                if (desc.diagnosticsRows.empty())
                {
                    TextDisabled(dc, {list.left + 8, list.top + 8, list.right - 8, list.top + 32}, "No diagnostics rows yet");
                }
                for (int i = firstRow; i < endRow; ++i)
                {
                    const int local = i - firstRow;
                    RECT rowRect{list.left, list.top + yOffset + local * rowH, list.right - 4, list.top + yOffset + (local + 1) * rowH - 2};
                    DrawDiagnosticsRow(dc, rowRect, desc.diagnosticsRows[static_cast<std::size_t>(i)]);
                }
            }
            DrawVerticalScrollbar(dc, {body.right - 9, list.top, body.right - 3, list.bottom}, contentPixels, viewportPixels, scroll);
        }

        void DrawLayoutPanelBody(HDC dc, const EditorShellPanelDesc& panel, const EditorFrameDesc& desc)
        {
            if (!panel.visible || !panel.active)
            {
                return;
            }

            RECT body = ToWinRect(panel.bodyRect);
            if (!RectIsUsable(body))
            {
                return;
            }

            const bool isViewport = panel.name == "scene.viewport" || panel.name == "game.viewport";
            DrawPanelBodyBackground(dc, body, panel.focused, isViewport);

            if (panel.name == "scene.hierarchy")
            {
                DrawHierarchyPanelContent(dc, body, desc);
            }
            else if (panel.name == "inspector")
            {
                DrawInspectorPanelContent(dc, body, desc);
            }
            else if (panel.name == "scene.viewport")
            {
                DrawSceneToolbarContent(dc, body, desc);
                RECT legacyViewport{body.left, body.top + 1, body.right, body.bottom};
                DrawViewportGrid(dc, legacyViewport, desc);
                DrawViewportItems(dc, legacyViewport, desc);
                DrawViewportGizmo(dc, legacyViewport, desc);
                for (const EditorShellOverlayDesc& overlay : desc.shellLayout.overlays)
                {
                    if (overlay.visible)
                    {
                        DrawUnityOverlay(dc, overlay);
                    }
                }
            }
            else if (panel.name == "game.viewport")
            {
                RECT inner = Inset(body, 10);
                Fill(dc, inner, Rgb(16, 17, 20));
                Stroke(dc, inner, Rgb(54, 57, 66));
                Text(dc, Inset(inner, 18), "Game View placeholder: runtime camera output will bind here after Vulkan/ImGui shell.", Rgb(162, 168, 182));
            }
            else if (panel.name == "assets.browser")
            {
                DrawProjectBrowserContent(dc, body, desc);
            }
            else if (panel.name == "console")
            {
                TextListBodyScrolled(dc, body, desc.consoleLines, desc.consoleScrollPixels);
            }
            else if (panel.name == "diagnostics")
            {
                DrawDiagnosticsPanelContent(dc, body, desc);
            }
            else if (panel.name == "world.partition")
            {
                std::vector<std::string> lines;
                lines.push_back("World Partition");
                lines.push_back("Visible objects: " + std::to_string(desc.visibleCount));
                lines.push_back("Culled objects: " + std::to_string(desc.culledCount));
                lines.push_back("Bounds: " + std::to_string(desc.boundsCount));
                TextListBody(dc, body, lines, BodyLineCapacity(body));
            }
            else if (panel.name == "render.graph")
            {
                std::vector<std::string> lines;
                lines.push_back("RenderGraph debugger placeholder");
                lines.push_back("FrameGraph compiler/execution foundations are ready.");
                lines.push_back("Future: pass graph, barriers, transient resources, timing.");
                TextListBody(dc, body, lines, BodyLineCapacity(body));
            }
            else
            {
                std::vector<std::string> lines;
                lines.push_back(panel.title.empty() ? panel.name : panel.title);
                lines.push_back("Panel is registered in EditorUI and docked in the layout tree.");
                lines.push_back("Renderer body integration is pending for this panel kind.");
                TextListBody(dc, body, lines, BodyLineCapacity(body));
            }
        }



        GdiIconKind PopupIconFromString(const std::string& icon)
        {
            const std::string lower = LowerAscii(icon);
            if (lower == "mesh") return GdiIconKind::Mesh;
            if (lower == "camera") return GdiIconKind::Camera;
            if (lower == "light") return GdiIconKind::Light;
            if (lower == "folder") return GdiIconKind::Folder;
            if (lower == "material") return GdiIconKind::Material;
            if (lower == "shader") return GdiIconKind::Shader;
            if (lower == "transform") return GdiIconKind::Transform;
            if (lower == "bounds") return GdiIconKind::Bounds;
            if (lower == "console") return GdiIconKind::Console;
            if (lower == "search") return GdiIconKind::Search;
            if (lower == "plus") return GdiIconKind::Plus;
            if (lower == "eye") return GdiIconKind::Eye;
            if (lower == "lock") return GdiIconKind::Lock;
            if (lower == "entity" || lower == "scene") return GdiIconKind::Entity;
            return GdiIconKind::Asset;
        }

        void DrawPopupDropShadow(HDC dc, RECT rect)
        {
            Fill(dc, {rect.left + 4, rect.top + 4, rect.right + 4, rect.bottom + 4}, Rgb(16, 17, 20));
            Fill(dc, {rect.left + 2, rect.top + 2, rect.right + 2, rect.bottom + 2}, Rgb(22, 23, 27));
        }

        void DrawLayoutPopupSurface(HDC dc, const EditorShellPopupDesc& popup)
        {
            if (!popup.open)
            {
                return;
            }

            RECT rect = ToWinRect(popup.rect);
            if (!RectIsUsable(rect))
            {
                return;
            }

            DrawPopupDropShadow(dc, rect);
            Fill(dc, rect, popup.popupPanel ? Rgb(44, 45, 51) : Rgb(47, 48, 54));
            Stroke(dc, rect, Rgb(77, 81, 92));
            Line(dc, rect.left + 1, rect.top + 1, rect.right - 1, rect.top + 1, Rgb(62, 65, 75));

            if (popup.popupPanel && !popup.title.empty())
            {
                RECT titleRect{rect.left + 10, rect.top + 5, rect.right - 10, rect.top + 26};
                Text(dc, titleRect, popup.title, Rgb(221, 224, 232), DT_SINGLELINE | DT_VCENTER);
                Line(dc, rect.left + 6, rect.top + 29, rect.right - 6, rect.top + 29, Rgb(34, 35, 40));
            }

            for (const EditorShellPopupItemDesc& item : popup.items)
            {
                RECT row = ToWinRect(item.rect);
                if (!RectIsUsable(row))
                {
                    continue;
                }
                if (item.separatorBefore)
                {
                    Line(dc, row.left + 4, row.top - 3, row.right - 4, row.top - 3, Rgb(34, 35, 40));
                }

                if (item.hovered && item.enabled)
                {
                    Fill(dc, row, Rgb(62, 83, 124));
                    Line(dc, row.left, row.top, row.left, row.bottom, Rgb(89, 139, 221));
                }
                else if (!item.enabled)
                {
                    Fill(dc, row, Rgb(42, 43, 48));
                }

                if (item.checked)
                {
                    Text(dc, {row.left + 4, row.top, row.left + 20, row.bottom}, "x", Rgb(168, 194, 240), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }
                else if (!item.icon.empty())
                {
                    DrawGdiIcon(dc, {row.left + 5, row.top + 3, row.left + 18, row.bottom - 3}, PopupIconFromString(item.icon));
                }

                const COLORREF textColor = !item.enabled ? Rgb(111, 115, 124) : (item.destructive ? Rgb(238, 160, 150) : Rgb(224, 227, 234));
                Text(dc, {row.left + 24, row.top, row.right - 22, row.bottom}, item.label, textColor, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                if (item.hasSubmenu)
                {
                    Text(dc, {row.right - 18, row.top, row.right - 4, row.bottom}, ">", item.enabled ? Rgb(168, 173, 185) : Rgb(101, 104, 112), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }
            }
        }



        COLORREF CommandPaletteKindColor(const std::string& kind)
        {
            if (kind == "command") return Rgb(116, 158, 230);
            if (kind == "panel") return Rgb(124, 180, 148);
            if (kind == "asset") return Rgb(214, 166, 86);
            if (kind == "entity") return Rgb(168, 176, 196);
            return Rgb(148, 154, 168);
        }

        void DrawCommandPaletteRow(HDC dc, RECT rect, const EditorCommandPaletteRowRenderDesc& row)
        {
            const bool selected = row.selected;
            const COLORREF back = selected ? Rgb(58, 80, 122) : Rgb(42, 43, 49);
            Fill(dc, rect, back);
            Line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, Rgb(31, 32, 37));
            if (selected)
            {
                Line(dc, rect.left, rect.top, rect.left, rect.bottom - 1, Rgb(91, 142, 229));
            }

            RECT iconRect{rect.left + 10, rect.top + 7, rect.left + 31, rect.top + 28};
            Fill(dc, iconRect, row.enabled ? Rgb(50, 53, 62) : Rgb(37, 38, 43));
            Stroke(dc, iconRect, CommandPaletteKindColor(row.kind));
            if (!row.icon.empty())
            {
                DrawGdiIcon(dc, {iconRect.left + 4, iconRect.top + 4, iconRect.right - 4, iconRect.bottom - 4}, PopupIconFromString(row.icon));
            }

            const COLORREF titleColor = !row.enabled ? Rgb(113, 116, 126) : (row.destructive ? Rgb(246, 165, 154) : (selected ? Rgb(248, 250, 255) : Rgb(225, 229, 237)));
            const COLORREF subColor = !row.enabled ? Rgb(88, 91, 99) : (selected ? Rgb(178, 194, 226) : Rgb(143, 150, 164));
            Text(dc, {rect.left + 40, rect.top + 4, rect.right - 130, rect.top + 22}, row.title, titleColor);
            const std::string subtitle = !row.enabled && !row.disabledReason.empty() ? row.disabledReason : row.subtitle;
            Text(dc, {rect.left + 40, rect.top + 22, rect.right - 130, rect.bottom - 3}, subtitle, subColor);

            if (!row.rightText.empty())
            {
                RECT shortcut{rect.right - 120, rect.top + 8, rect.right - 12, rect.top + 27};
                Fill(dc, shortcut, selected ? Rgb(46, 58, 84) : Rgb(35, 36, 41));
                Stroke(dc, shortcut, selected ? Rgb(86, 112, 164) : Rgb(58, 61, 70));
                Text(dc, shortcut, row.rightText, selected ? Rgb(211, 222, 246) : Rgb(143, 150, 164), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }
            else
            {
                Text(dc, {rect.right - 110, rect.top, rect.right - 12, rect.bottom}, row.kind, CommandPaletteKindColor(row.kind), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
            }
        }

        void DrawCommandPaletteSurface(HDC dc, const EditorCommandPaletteRenderDesc& palette)
        {
            if (!palette.open)
            {
                return;
            }

            RECT rect = ToWinRect(palette.surfaceRect);
            if (!RectIsUsable(rect))
            {
                return;
            }

            DrawPopupDropShadow(dc, rect);
            Fill(dc, rect, Rgb(39, 40, 46));
            Stroke(dc, rect, Rgb(92, 100, 118));
            Line(dc, rect.left + 1, rect.top + 1, rect.right - 1, rect.top + 1, Rgb(65, 68, 78));

            RECT search = ToWinRect(palette.searchBoxRect);
            Fill(dc, search, Rgb(30, 31, 36));
            Stroke(dc, search, Rgb(91, 125, 184));
            Text(dc, {search.left + 11, search.top, search.left + 31, search.bottom}, ">", Rgb(126, 162, 226), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            const std::string queryText = palette.query.empty() ? palette.placeholder : palette.query;
            Text(dc, {search.left + 34, search.top, search.right - 12, search.bottom}, queryText, palette.query.empty() ? Rgb(117, 122, 134) : Rgb(236, 239, 246));

            RECT list = ToWinRect(palette.listRect);
            Fill(dc, list, Rgb(35, 36, 41));
            {
                ScopedClip listClip(dc, list);
                const int first = static_cast<int>(palette.visibleFirstRow);
                const int count = std::min<int>(static_cast<int>(palette.visibleRowCount) + 1, static_cast<int>(palette.rows.size()) - first);
                for (int i = 0; i < count; ++i)
                {
                    const int index = first + i;
                    if (index < 0 || index >= static_cast<int>(palette.rows.size()))
                    {
                        continue;
                    }
                    RECT rowRect{list.left, list.top + i * palette.rowHeightPixels, list.right, list.top + (i + 1) * palette.rowHeightPixels};
                    DrawCommandPaletteRow(dc, rowRect, palette.rows[static_cast<std::size_t>(index)]);
                }
                if (palette.rows.empty())
                {
                    TextDisabled(dc, {list.left + 12, list.top + 10, list.right - 12, list.top + 34}, "No matching commands, panels, assets or entities");
                }
            }

            RECT footer = ToWinRect(palette.footerRect);
            Line(dc, footer.left, footer.top - 3, footer.right, footer.top - 3, Rgb(28, 29, 34));
            Text(dc, footer, palette.footerText, Rgb(134, 141, 156), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }


        void DrawLayoutPopups(HDC dc, const EditorFrameDesc& desc)
        {
            for (const EditorShellPopupDesc& popup : desc.popupSurfaces)
            {
                DrawLayoutPopupSurface(dc, popup);
            }
        }

        void DrawLayoutDrivenShell(HDC dc, const EditorFrameDesc& desc)
        {
            const EditorShellLayoutDesc& shell = desc.shellLayout;
            Fill(dc, ToWinRect(shell.dockSpace), Rgb(28, 29, 34));
            DrawLayoutMenuBar(dc, ToWinRect(shell.menuBar), desc);
            DrawLayoutToolbar(dc, ToWinRect(shell.toolbar), desc);

            for (const EditorShellPanelDesc& panel : shell.panels)
            {
                if (!panel.visible || !panel.active)
                {
                    continue;
                }
                RECT body = ToWinRect(panel.bodyRect);
                RECT tab = ToWinRect(panel.tabRect);
                if (!RectIsUsable(body) || !RectIsUsable(tab))
                {
                    continue;
                }
                RECT frame{body.left, tab.top, body.right, body.bottom};
                Fill(dc, frame, Rgb(38, 39, 45));
                SoftBorder(dc, frame);
            }

            DrawLayoutTabs(dc, desc);
            for (const EditorShellPanelDesc& panel : shell.panels)
            {
                DrawLayoutPanelBody(dc, panel, desc);
            }

            for (const EditorShellSplitterDesc& splitter : shell.splitters)
            {
                RECT rect = ToWinRect(splitter.rect);
                if (!RectIsUsable(rect))
                {
                    continue;
                }
                Fill(dc, rect, Rgb(27, 28, 33));
            }

            DrawLayoutStatusBar(dc, ToWinRect(shell.statusBar), desc);
            DrawLayoutPopups(dc, desc);
            DrawCommandPaletteSurface(dc, desc.commandPalette);
        }


    }
#endif

    bool Renderer::Initialize(const RendererDesc& desc, Window& window)
    {
        if (mInitialized)
        {
            return true;
        }

        VulkanRhiConfig rhiConfig = MakeDefaultVulkanRhiConfig(desc.applicationName ? desc.applicationName : "AK Engine");
        rhiConfig.enableValidation = desc.enableValidation;
        rhiConfig.window = MakeNativeWindowSurfaceDesc(window.NativeHandle(), window.Width(), window.Height());

        const VulkanRhiProbe rhiProbe = BuildVulkanRhiProbe(rhiConfig, true);
        mBackendStatus.vulkanPlanned = rhiProbe.plan.valid;
        mBackendStatus.vulkanLoaderAvailable = rhiProbe.loader.loaded && rhiProbe.loader.vkGetInstanceProcAddrResolved;

        const bool preferVulkan = desc.backendPreference == RendererBackendPreference::Auto
            || desc.backendPreference == RendererBackendPreference::Vulkan;
        const bool canUseVulkanBootstrap = preferVulkan && rhiProbe.ok && mBackendStatus.vulkanLoaderAvailable;

        if (canUseVulkanBootstrap)
        {
            mBackendStatus.activeBackend = RhiBackend::Vulkan;
            mBackendStatus.usingSoftwareFallback = false;
            mBackendStatus.summary = std::string("Vulkan RHI bootstrap ready: ") + rhiProbe.summary;
        }
        else
        {
            if (desc.requireVulkanDevice && desc.backendPreference == RendererBackendPreference::Vulkan)
            {
                LogError(std::string("Renderer Vulkan backend required but unavailable: ") + rhiProbe.summary);
                return false;
            }

            mBackendStatus.activeBackend = RhiBackend::None;
            mBackendStatus.usingSoftwareFallback = true;
            mBackendStatus.summary = std::string("GDI/editor fallback while Vulkan RHI is planned: ") + rhiProbe.summary;
        }

        LogInfo(std::string("Renderer initialized for ") + (desc.applicationName ? desc.applicationName : "AK Engine") + " | " + mBackendStatus.summary);
        mInitialized = true;
        return true;
    }

    void Renderer::Shutdown()
    {
        if (!mInitialized)
        {
            return;
        }

        LogInfo("Renderer shutdown");
        mBackendStatus = {};
        mInitialized = false;
    }

    void Renderer::BeginFrame()
    {
    }

    void Renderer::DrawEditorShell(Window& window, const EditorFrameDesc& desc)
    {
        if (!mInitialized)
        {
            return;
        }

#if defined(_WIN32)
        HWND hwnd = static_cast<HWND>(window.NativeHandle());
        if (!hwnd)
        {
            return;
        }

        RECT client{};
        GetClientRect(hwnd, &client);

        HDC windowDc = GetDC(hwnd);
        if (!windowDc)
        {
            return;
        }

        const int clientWidth = client.right - client.left;
        const int clientHeight = client.bottom - client.top;
        if (clientWidth <= 0 || clientHeight <= 0)
        {
            ReleaseDC(hwnd, windowDc);
            return;
        }

        HDC dc = CreateCompatibleDC(windowDc);
        if (!dc)
        {
            ReleaseDC(hwnd, windowDc);
            return;
        }

        HBITMAP backBuffer = CreateCompatibleBitmap(windowDc, clientWidth, clientHeight);
        if (!backBuffer)
        {
            DeleteDC(dc);
            ReleaseDC(hwnd, windowDc);
            return;
        }

        HGDIOBJ oldBitmap = SelectObject(dc, backBuffer);
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ oldFont = SelectObject(dc, font);

        Fill(dc, client, Rgb(24, 24, 28));

        if (desc.useLayoutDrivenShell && desc.shellLayout.valid)
        {
            DrawLayoutDrivenShell(dc, desc);
        }
        else
        {
            RECT toolbar{client.left, client.top, client.right, client.top + ToolbarHeight};
            DrawToolbar(dc, toolbar, desc);

            const int top = toolbar.bottom + PanelGap;
            const int bottom = client.bottom - PanelGap;
            const int left = client.left + PanelGap;
            const int right = client.right - PanelGap;
            const int lowerTop = std::max<int>(top + 180, bottom - AssetBrowserHeight - ConsoleHeight - PanelGap);
            const int middleBottom = lowerTop - PanelGap;

            RECT hierarchy{left, top, left + HierarchyWidth, middleBottom};
            RECT inspector{right - InspectorWidth, top, right, middleBottom};
            RECT viewport{hierarchy.right + PanelGap, top, inspector.left - PanelGap, middleBottom};
            RECT assets{left, lowerTop, right, lowerTop + AssetBrowserHeight};
            RECT console{left, assets.bottom + PanelGap, right, bottom};

            Panel(dc, hierarchy, "Scene Hierarchy");
            TextList(dc, hierarchy, desc.hierarchyItems, 18);

            Panel(dc, inspector, "Inspector");
            Label(dc, {inspector.left + 10, inspector.top + 40, inspector.right - 10, inspector.top + 62}, "Selected: " + desc.selectedEntityName);
            int inspectorY = inspector.top + 70;
            for (const std::string& line : desc.inspectorLines)
            {
                Label(dc, {inspector.left + 10, inspectorY, inspector.right - 10, inspectorY + 22}, line);
                inspectorY += 24;
                if (inspectorY > inspector.bottom - 52)
                {
                    break;
                }
            }
            Label(dc, {inspector.left + 10, inspector.bottom - 34, inspector.right - 10, inspector.bottom - 12}, desc.statusLine);

            Panel(dc, viewport, "Viewport");
            DrawViewportGrid(dc, viewport, desc);
            DrawViewportItems(dc, viewport, desc);
            DrawViewportGizmo(dc, viewport, desc);

            Panel(dc, assets, "Asset Browser");
            TextList(dc, assets, desc.assetItems, 5);

            Panel(dc, console, "Console");
            TextList(dc, console, desc.consoleLines, 5);
        }

        BitBlt(windowDc, 0, 0, clientWidth, clientHeight, dc, 0, 0, SRCCOPY);

        SelectObject(dc, oldFont);
        SelectObject(dc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(dc);
        ReleaseDC(hwnd, windowDc);
#else
        (void)window;
        (void)desc;
#endif
    }

    void Renderer::EndFrame()
    {
    }

    bool Renderer::IsInitialized() const
    {
        return mInitialized;
    }

    const RendererBackendStatus& Renderer::BackendStatus() const
    {
        return mBackendStatus;
    }
}
