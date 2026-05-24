#pragma once

#include <AK/Core/Types.hpp>

#include <array>
#include <string>

namespace AK
{
    enum class EditorThemeProfile
    {
        UnityLikeDark,
        HighContrastDark,
        LightPreview
    };

    enum class EditorThemeColorRole
    {
        WindowBackground,
        MenuBarBackground,
        ToolbarBackground,
        DockBackground,
        PanelBackground,
        PanelHeader,
        PanelBorder,
        TabBackground,
        TabActiveBackground,
        TabHoveredBackground,
        FieldBackground,
        ButtonBackground,
        ButtonHoveredBackground,
        ButtonActiveBackground,
        PopupBackground,
        PopupBorder,
        TextPrimary,
        TextSecondary,
        TextMuted,
        TextDisabled,
        Accent,
        AccentHovered,
        Selection,
        SelectionBorder,
        Warning,
        Error,
        Success,
        GridMinor,
        GridMajor,
        ViewportBackground,
        Count
    };

    enum class EditorThemeMetricRole
    {
        MenuHeight,
        ToolbarHeight,
        StatusHeight,
        DockGap,
        TabHeight,
        RowHeight,
        HeaderHeight,
        FieldHeight,
        ButtonHeight,
        IconSize,
        PaddingSmall,
        Padding,
        PaddingLarge,
        InspectorLabelWidth,
        AssetCellWidth,
        AssetCellHeight,
        PopupRowHeight,
        ScrollbarWidth,
        SplitterHitSize,
        Count
    };

    struct EditorRgba
    {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        u8 a = 255;
    };

    struct EditorThemeMetrics
    {
        i32 menuHeight = 22;
        i32 toolbarHeight = 34;
        i32 statusHeight = 22;
        i32 dockGap = 4;
        i32 tabHeight = 23;
        i32 rowHeight = 22;
        i32 headerHeight = 29;
        i32 fieldHeight = 20;
        i32 buttonHeight = 22;
        i32 iconSize = 16;
        i32 paddingSmall = 4;
        i32 padding = 6;
        i32 paddingLarge = 8;
        i32 inspectorLabelWidth = 78;
        i32 assetCellWidth = 82;
        i32 assetCellHeight = 72;
        i32 popupRowHeight = 22;
        i32 scrollbarWidth = 10;
        i32 splitterHitSize = 8;
    };

    struct EditorThemeTypography
    {
        std::string uiFont = "Segoe UI";
        std::string monoFont = "Consolas";
        i32 baseFontPixels = 12;
        i32 smallFontPixels = 11;
        i32 titleFontPixels = 13;
        bool useClearType = true;
    };

    struct EditorTheme
    {
        EditorThemeProfile profile = EditorThemeProfile::UnityLikeDark;
        std::string id = "unity-like-dark";
        std::string displayName = "Unity-like Dark";
        std::array<EditorRgba, static_cast<std::size_t>(EditorThemeColorRole::Count)> colors{};
        EditorThemeMetrics metrics{};
        EditorThemeTypography typography{};
        u64 revision = 1;
    };

    struct EditorThemeStateColors
    {
        EditorRgba background{};
        EditorRgba border{};
        EditorRgba text{};
        EditorRgba accent{};
    };

    struct EditorThemeDiagnostics
    {
        std::size_t colorCount = 0;
        std::size_t invalidAlphaCount = 0;
        std::size_t invalidMetricCount = 0;
        double primaryTextContrast = 0.0;
        double mutedTextContrast = 0.0;
        bool contrastOk = false;
        bool metricsOk = false;
        bool typographyOk = false;
        bool ok = false;
        std::string summary;
    };

    const char* ToString(EditorThemeProfile profile);
    const char* ToString(EditorThemeColorRole role);
    const char* ToString(EditorThemeMetricRole role);

    EditorRgba MakeEditorRgba(u8 r, u8 g, u8 b, u8 a = 255);
    u32 PackEditorRgba(EditorRgba color);
    EditorRgba GetEditorThemeColor(const EditorTheme& theme, EditorThemeColorRole role);
    i32 GetEditorThemeMetric(const EditorTheme& theme, EditorThemeMetricRole role);
    EditorThemeStateColors GetEditorThemeStateColors(const EditorTheme& theme, bool hovered, bool active, bool focused, bool disabled, bool selected);

    EditorTheme BuildDefaultEditorTheme();
    EditorTheme BuildHighContrastEditorTheme();
    EditorTheme BuildLightPreviewEditorTheme();
    EditorTheme ScaleEditorThemeForDpi(EditorTheme theme, float dpiScale);
    EditorThemeDiagnostics ValidateEditorTheme(const EditorTheme& theme);
    std::string FormatEditorThemeDiagnostics(const EditorThemeDiagnostics& diagnostics);
    std::string FormatEditorThemeSummary(const EditorTheme& theme);
    u64 HashEditorTheme(const EditorTheme& theme);
}
