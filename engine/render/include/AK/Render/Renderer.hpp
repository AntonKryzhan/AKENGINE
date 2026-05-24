#pragma once

#include <AK/Platform/Window.hpp>
#include <AK/RHI/VulkanRHI.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace AK
{
    enum class RendererBackendPreference : std::uint32_t
    {
        Auto = 0,
        Vulkan = 1,
        SoftwareFallback = 2
    };

    struct RendererDesc
    {
        const char* applicationName = "AK Engine";
        RendererBackendPreference backendPreference = RendererBackendPreference::Auto;
        bool requireVulkanDevice = false;
        bool enableValidation = true;
    };

    struct RendererBackendStatus
    {
        RhiBackend activeBackend = RhiBackend::None;
        bool vulkanPlanned = false;
        bool vulkanLoaderAvailable = false;
        bool usingSoftwareFallback = true;
        std::string summary;
    };

    enum class EditorViewportItemKind : std::uint32_t
    {
        Entity = 0,
        Mesh = 1,
        Camera = 2,
        Light = 3
    };

    struct EditorViewportItem
    {
        std::string name;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool selected = false;
        EditorViewportItemKind kind = EditorViewportItemKind::Entity;
        bool hasBounds = false;
        bool culled = false;
        float minX = 0.0f;
        float minZ = 0.0f;
        float maxX = 0.0f;
        float maxZ = 0.0f;
    };


    struct EditorShellRect
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t width = 0;
        std::int32_t height = 0;
    };

    struct EditorShellPanelDesc
    {
        std::uint32_t id = 0;
        std::string name;
        std::string title;
        std::string kind;
        EditorShellRect tabRect{};
        EditorShellRect bodyRect{};
        bool visible = false;
        bool active = false;
        bool focused = false;
    };

    struct EditorShellSplitterDesc
    {
        EditorShellRect rect{};
        bool vertical = true;
        bool draggable = true;
    };

    struct EditorShellToolbarButtonDesc
    {
        EditorShellRect rect{};
        std::string label;
        bool enabled = true;
        bool active = false;
        bool destructive = false;
    };

    struct EditorShellOverlayDesc
    {
        EditorShellRect rect{};
        std::string label;
        bool visible = true;
        bool interactive = false;
    };

    struct EditorShellPopupItemDesc
    {
        EditorShellRect rect{};
        std::string id;
        std::string label;
        std::string icon;
        bool enabled = true;
        bool checked = false;
        bool destructive = false;
        bool separatorBefore = false;
        bool hasSubmenu = false;
        bool hovered = false;
    };

    struct EditorShellPopupDesc
    {
        EditorShellRect rect{};
        EditorShellRect anchor{};
        std::string id;
        std::string title;
        bool open = false;
        bool popupPanel = false;
        std::vector<EditorShellPopupItemDesc> items;
    };



    struct EditorViewportGizmoHandleRenderDesc
    {
        EditorShellRect rect{};
        std::string id;
        std::string label;
        bool hovered = false;
        bool active = false;
    };

    struct EditorViewportGizmoRenderDesc
    {
        bool visible = false;
        std::string mode;
        std::string status;
        std::int32_t originX = 0;
        std::int32_t originY = 0;
        bool dragging = false;
        std::string activeHandle;
        std::vector<EditorViewportGizmoHandleRenderDesc> handles;
    };

    struct EditorShellLayoutDesc
    {
        EditorShellRect menuBar{};
        EditorShellRect toolbar{};
        EditorShellRect dockSpace{};
        EditorShellRect statusBar{};
        std::vector<EditorShellPanelDesc> panels;
        std::vector<EditorShellSplitterDesc> splitters;
        std::vector<EditorShellToolbarButtonDesc> toolbarButtons;
        std::vector<EditorShellOverlayDesc> overlays;
        std::string statusLeft;
        std::string statusRight;
        std::string hoverLabel;
        bool valid = false;
    };


    struct EditorDiagnosticsMetricRenderDesc
    {
        std::string label;
        std::string value;
        std::string source;
        std::string severity;
        bool pinned = false;
    };

    struct EditorDiagnosticsRowRenderDesc
    {
        std::string icon;
        std::string severity;
        std::string kind;
        std::string source;
        std::string title;
        std::string subtitle;
        std::string message;
        std::string detail;
        std::string badges;
        bool unread = false;
        bool sticky = false;
        bool active = false;
        bool selected = false;
    };



    struct EditorCommandPaletteRowRenderDesc
    {
        std::string icon;
        std::string kind;
        std::string title;
        std::string subtitle;
        std::string rightText;
        std::string disabledReason;
        bool selected = false;
        bool enabled = true;
        bool destructive = false;
    };

    struct EditorCommandPaletteRenderDesc
    {
        bool open = false;
        EditorShellRect surfaceRect{};
        EditorShellRect searchBoxRect{};
        EditorShellRect listRect{};
        EditorShellRect footerRect{};
        std::string query;
        std::string placeholder;
        std::string footerText;
        std::vector<EditorCommandPaletteRowRenderDesc> rows;
        std::uint32_t visibleFirstRow = 0;
        std::uint32_t visibleRowCount = 0;
        std::uint32_t selectedIndex = 0;
        std::int32_t rowHeightPixels = 34;
        std::uint64_t revision = 1;
    };

    struct EditorFrameDesc
    {
        std::string title = "AK Engine";
        std::string sceneName = "Untitled";
        std::string selectedEntityName = "None";
        std::string selectedTransform = "No transform";
        std::string statusLine = "Ready";
        std::vector<std::string> hierarchyItems;
        std::vector<std::string> inspectorLines;
        std::vector<std::string> assetItems;
        std::vector<std::string> consoleLines;
        std::vector<EditorDiagnosticsMetricRenderDesc> diagnosticsMetrics;
        std::vector<EditorDiagnosticsRowRenderDesc> diagnosticsRows;
        std::string diagnosticsStatus;
        std::uint32_t diagnosticsUnreadCount = 0;
        std::uint32_t diagnosticsWarningCount = 0;
        std::uint32_t diagnosticsErrorCount = 0;
        std::uint32_t diagnosticsActiveTaskCount = 0;
        std::vector<EditorViewportItem> viewportItems;
        EditorViewportGizmoRenderDesc transformGizmo{};
        float viewportCenterX = 0.0f;
        float viewportCenterZ = 0.0f;
        float viewportScale = 36.0f;
        std::string viewportMode = "2D";
        std::string viewportProjection = "Orthographic";
        bool viewportPerspective = false;
        float viewportCameraX = 0.0f;
        float viewportCameraY = 8.0f;
        float viewportCameraZ = -10.0f;
        float viewportYawDegrees = 0.0f;
        float viewportPitchDegrees = -35.0f;
        float viewportFovYDegrees = 60.0f;
        bool viewportDragging = false;
        bool viewportPanning = false;
        bool viewportSnapEnabled = false;
        float viewportSnapStep = 0.5f;
        std::uint64_t frameIndex = 0;
        std::uint32_t entityCount = 0;
        std::uint32_t assetCount = 0;
        std::uint32_t meshCount = 0;
        std::uint32_t cameraCount = 0;
        std::uint32_t lightCount = 0;
        std::uint32_t boundsCount = 0;
        std::uint32_t visibleCount = 0;
        std::uint32_t culledCount = 0;
        std::uint32_t undoDepth = 0;
        std::uint32_t redoDepth = 0;
        bool dirty = false;
        std::int32_t hierarchyScrollPixels = 0;
        std::int32_t inspectorScrollPixels = 0;
        std::int32_t projectGridScrollPixels = 0;
        std::int32_t consoleScrollPixels = 0;
        bool useLayoutDrivenShell = false;
        EditorShellLayoutDesc shellLayout{};
        std::vector<EditorShellPopupDesc> popupSurfaces;
        EditorCommandPaletteRenderDesc commandPalette{};
    };

    class Renderer final
    {
    public:
        bool Initialize(const RendererDesc& desc, Window& window);
        void Shutdown();
        void BeginFrame();
        void DrawEditorShell(Window& window, const EditorFrameDesc& desc);
        void EndFrame();
        bool IsInitialized() const;
        const RendererBackendStatus& BackendStatus() const;

    private:
        bool mInitialized = false;
        RendererBackendStatus mBackendStatus{};
    };
}
