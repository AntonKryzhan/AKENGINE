#include <AK/EditorUI/EditorTheme.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace AK
{
    namespace
    {
        double ChannelToLinear(u8 value)
        {
            const double c = static_cast<double>(value) / 255.0;
            if (c <= 0.04045)
            {
                return c / 12.92;
            }
            return std::pow((c + 0.055) / 1.055, 2.4);
        }

        double RelativeLuminance(EditorRgba color)
        {
            return 0.2126 * ChannelToLinear(color.r) + 0.7152 * ChannelToLinear(color.g) + 0.0722 * ChannelToLinear(color.b);
        }

        double ContrastRatio(EditorRgba a, EditorRgba b)
        {
            const double la = RelativeLuminance(a);
            const double lb = RelativeLuminance(b);
            const double hi = std::max(la, lb);
            const double lo = std::min(la, lb);
            return (hi + 0.05) / (lo + 0.05);
        }

        void Set(EditorTheme& theme, EditorThemeColorRole role, EditorRgba color)
        {
            theme.colors[static_cast<std::size_t>(role)] = color;
        }

        bool MetricPositive(i32 value)
        {
            return value > 0 && value < 512;
        }
    }

    const char* ToString(EditorThemeProfile profile)
    {
        switch (profile)
        {
            case EditorThemeProfile::UnityLikeDark: return "unity-like-dark";
            case EditorThemeProfile::HighContrastDark: return "high-contrast-dark";
            case EditorThemeProfile::LightPreview: return "light-preview";
        }
        return "unknown";
    }

    const char* ToString(EditorThemeColorRole role)
    {
        switch (role)
        {
            case EditorThemeColorRole::WindowBackground: return "WindowBackground";
            case EditorThemeColorRole::MenuBarBackground: return "MenuBarBackground";
            case EditorThemeColorRole::ToolbarBackground: return "ToolbarBackground";
            case EditorThemeColorRole::DockBackground: return "DockBackground";
            case EditorThemeColorRole::PanelBackground: return "PanelBackground";
            case EditorThemeColorRole::PanelHeader: return "PanelHeader";
            case EditorThemeColorRole::PanelBorder: return "PanelBorder";
            case EditorThemeColorRole::TabBackground: return "TabBackground";
            case EditorThemeColorRole::TabActiveBackground: return "TabActiveBackground";
            case EditorThemeColorRole::TabHoveredBackground: return "TabHoveredBackground";
            case EditorThemeColorRole::FieldBackground: return "FieldBackground";
            case EditorThemeColorRole::ButtonBackground: return "ButtonBackground";
            case EditorThemeColorRole::ButtonHoveredBackground: return "ButtonHoveredBackground";
            case EditorThemeColorRole::ButtonActiveBackground: return "ButtonActiveBackground";
            case EditorThemeColorRole::PopupBackground: return "PopupBackground";
            case EditorThemeColorRole::PopupBorder: return "PopupBorder";
            case EditorThemeColorRole::TextPrimary: return "TextPrimary";
            case EditorThemeColorRole::TextSecondary: return "TextSecondary";
            case EditorThemeColorRole::TextMuted: return "TextMuted";
            case EditorThemeColorRole::TextDisabled: return "TextDisabled";
            case EditorThemeColorRole::Accent: return "Accent";
            case EditorThemeColorRole::AccentHovered: return "AccentHovered";
            case EditorThemeColorRole::Selection: return "Selection";
            case EditorThemeColorRole::SelectionBorder: return "SelectionBorder";
            case EditorThemeColorRole::Warning: return "Warning";
            case EditorThemeColorRole::Error: return "Error";
            case EditorThemeColorRole::Success: return "Success";
            case EditorThemeColorRole::GridMinor: return "GridMinor";
            case EditorThemeColorRole::GridMajor: return "GridMajor";
            case EditorThemeColorRole::ViewportBackground: return "ViewportBackground";
            case EditorThemeColorRole::Count: return "Count";
        }
        return "Unknown";
    }

    const char* ToString(EditorThemeMetricRole role)
    {
        switch (role)
        {
            case EditorThemeMetricRole::MenuHeight: return "MenuHeight";
            case EditorThemeMetricRole::ToolbarHeight: return "ToolbarHeight";
            case EditorThemeMetricRole::StatusHeight: return "StatusHeight";
            case EditorThemeMetricRole::DockGap: return "DockGap";
            case EditorThemeMetricRole::TabHeight: return "TabHeight";
            case EditorThemeMetricRole::RowHeight: return "RowHeight";
            case EditorThemeMetricRole::HeaderHeight: return "HeaderHeight";
            case EditorThemeMetricRole::FieldHeight: return "FieldHeight";
            case EditorThemeMetricRole::ButtonHeight: return "ButtonHeight";
            case EditorThemeMetricRole::IconSize: return "IconSize";
            case EditorThemeMetricRole::PaddingSmall: return "PaddingSmall";
            case EditorThemeMetricRole::Padding: return "Padding";
            case EditorThemeMetricRole::PaddingLarge: return "PaddingLarge";
            case EditorThemeMetricRole::InspectorLabelWidth: return "InspectorLabelWidth";
            case EditorThemeMetricRole::AssetCellWidth: return "AssetCellWidth";
            case EditorThemeMetricRole::AssetCellHeight: return "AssetCellHeight";
            case EditorThemeMetricRole::PopupRowHeight: return "PopupRowHeight";
            case EditorThemeMetricRole::ScrollbarWidth: return "ScrollbarWidth";
            case EditorThemeMetricRole::SplitterHitSize: return "SplitterHitSize";
            case EditorThemeMetricRole::Count: return "Count";
        }
        return "Unknown";
    }

    EditorRgba MakeEditorRgba(u8 r, u8 g, u8 b, u8 a)
    {
        return {r, g, b, a};
    }

    u32 PackEditorRgba(EditorRgba color)
    {
        return static_cast<u32>(color.r)
            | (static_cast<u32>(color.g) << 8u)
            | (static_cast<u32>(color.b) << 16u)
            | (static_cast<u32>(color.a) << 24u);
    }

    EditorRgba GetEditorThemeColor(const EditorTheme& theme, EditorThemeColorRole role)
    {
        return theme.colors[static_cast<std::size_t>(role)];
    }

    i32 GetEditorThemeMetric(const EditorTheme& theme, EditorThemeMetricRole role)
    {
        switch (role)
        {
            case EditorThemeMetricRole::MenuHeight: return theme.metrics.menuHeight;
            case EditorThemeMetricRole::ToolbarHeight: return theme.metrics.toolbarHeight;
            case EditorThemeMetricRole::StatusHeight: return theme.metrics.statusHeight;
            case EditorThemeMetricRole::DockGap: return theme.metrics.dockGap;
            case EditorThemeMetricRole::TabHeight: return theme.metrics.tabHeight;
            case EditorThemeMetricRole::RowHeight: return theme.metrics.rowHeight;
            case EditorThemeMetricRole::HeaderHeight: return theme.metrics.headerHeight;
            case EditorThemeMetricRole::FieldHeight: return theme.metrics.fieldHeight;
            case EditorThemeMetricRole::ButtonHeight: return theme.metrics.buttonHeight;
            case EditorThemeMetricRole::IconSize: return theme.metrics.iconSize;
            case EditorThemeMetricRole::PaddingSmall: return theme.metrics.paddingSmall;
            case EditorThemeMetricRole::Padding: return theme.metrics.padding;
            case EditorThemeMetricRole::PaddingLarge: return theme.metrics.paddingLarge;
            case EditorThemeMetricRole::InspectorLabelWidth: return theme.metrics.inspectorLabelWidth;
            case EditorThemeMetricRole::AssetCellWidth: return theme.metrics.assetCellWidth;
            case EditorThemeMetricRole::AssetCellHeight: return theme.metrics.assetCellHeight;
            case EditorThemeMetricRole::PopupRowHeight: return theme.metrics.popupRowHeight;
            case EditorThemeMetricRole::ScrollbarWidth: return theme.metrics.scrollbarWidth;
            case EditorThemeMetricRole::SplitterHitSize: return theme.metrics.splitterHitSize;
            case EditorThemeMetricRole::Count: break;
        }
        return 0;
    }

    EditorThemeStateColors GetEditorThemeStateColors(const EditorTheme& theme, bool hovered, bool active, bool focused, bool disabled, bool selected)
    {
        EditorThemeStateColors colors{};
        colors.background = GetEditorThemeColor(theme, EditorThemeColorRole::ButtonBackground);
        colors.border = GetEditorThemeColor(theme, EditorThemeColorRole::PanelBorder);
        colors.text = GetEditorThemeColor(theme, EditorThemeColorRole::TextPrimary);
        colors.accent = GetEditorThemeColor(theme, EditorThemeColorRole::Accent);

        if (selected)
        {
            colors.background = GetEditorThemeColor(theme, EditorThemeColorRole::Selection);
            colors.border = GetEditorThemeColor(theme, EditorThemeColorRole::SelectionBorder);
        }
        if (hovered)
        {
            colors.background = GetEditorThemeColor(theme, EditorThemeColorRole::ButtonHoveredBackground);
        }
        if (active || focused)
        {
            colors.background = GetEditorThemeColor(theme, EditorThemeColorRole::ButtonActiveBackground);
            colors.border = GetEditorThemeColor(theme, EditorThemeColorRole::AccentHovered);
        }
        if (disabled)
        {
            colors.text = GetEditorThemeColor(theme, EditorThemeColorRole::TextDisabled);
            colors.background = GetEditorThemeColor(theme, EditorThemeColorRole::DockBackground);
        }
        return colors;
    }

    EditorTheme BuildDefaultEditorTheme()
    {
        EditorTheme theme{};
        theme.profile = EditorThemeProfile::UnityLikeDark;
        theme.id = "unity-like-dark";
        theme.displayName = "Unity-like Dark";
        Set(theme, EditorThemeColorRole::WindowBackground, MakeEditorRgba(27, 28, 32));
        Set(theme, EditorThemeColorRole::MenuBarBackground, MakeEditorRgba(31, 31, 35));
        Set(theme, EditorThemeColorRole::ToolbarBackground, MakeEditorRgba(44, 45, 50));
        Set(theme, EditorThemeColorRole::DockBackground, MakeEditorRgba(34, 35, 40));
        Set(theme, EditorThemeColorRole::PanelBackground, MakeEditorRgba(46, 47, 53));
        Set(theme, EditorThemeColorRole::PanelHeader, MakeEditorRgba(52, 53, 59));
        Set(theme, EditorThemeColorRole::PanelBorder, MakeEditorRgba(64, 67, 76));
        Set(theme, EditorThemeColorRole::TabBackground, MakeEditorRgba(35, 36, 41));
        Set(theme, EditorThemeColorRole::TabActiveBackground, MakeEditorRgba(46, 47, 53));
        Set(theme, EditorThemeColorRole::TabHoveredBackground, MakeEditorRgba(49, 51, 58));
        Set(theme, EditorThemeColorRole::FieldBackground, MakeEditorRgba(38, 39, 44));
        Set(theme, EditorThemeColorRole::ButtonBackground, MakeEditorRgba(52, 55, 63));
        Set(theme, EditorThemeColorRole::ButtonHoveredBackground, MakeEditorRgba(60, 64, 74));
        Set(theme, EditorThemeColorRole::ButtonActiveBackground, MakeEditorRgba(64, 87, 132));
        Set(theme, EditorThemeColorRole::PopupBackground, MakeEditorRgba(42, 43, 49));
        Set(theme, EditorThemeColorRole::PopupBorder, MakeEditorRgba(82, 86, 98));
        Set(theme, EditorThemeColorRole::TextPrimary, MakeEditorRgba(231, 233, 238));
        Set(theme, EditorThemeColorRole::TextSecondary, MakeEditorRgba(190, 196, 207));
        Set(theme, EditorThemeColorRole::TextMuted, MakeEditorRgba(137, 143, 156));
        Set(theme, EditorThemeColorRole::TextDisabled, MakeEditorRgba(116, 121, 132));
        Set(theme, EditorThemeColorRole::Accent, MakeEditorRgba(73, 132, 228));
        Set(theme, EditorThemeColorRole::AccentHovered, MakeEditorRgba(101, 154, 238));
        Set(theme, EditorThemeColorRole::Selection, MakeEditorRgba(55, 76, 112));
        Set(theme, EditorThemeColorRole::SelectionBorder, MakeEditorRgba(91, 140, 226));
        Set(theme, EditorThemeColorRole::Warning, MakeEditorRgba(224, 169, 72));
        Set(theme, EditorThemeColorRole::Error, MakeEditorRgba(220, 94, 94));
        Set(theme, EditorThemeColorRole::Success, MakeEditorRgba(100, 194, 124));
        Set(theme, EditorThemeColorRole::GridMinor, MakeEditorRgba(32, 34, 40));
        Set(theme, EditorThemeColorRole::GridMajor, MakeEditorRgba(42, 45, 54));
        Set(theme, EditorThemeColorRole::ViewportBackground, MakeEditorRgba(18, 19, 23));
        return theme;
    }

    EditorTheme BuildHighContrastEditorTheme()
    {
        EditorTheme theme = BuildDefaultEditorTheme();
        theme.profile = EditorThemeProfile::HighContrastDark;
        theme.id = "high-contrast-dark";
        theme.displayName = "High Contrast Dark";
        Set(theme, EditorThemeColorRole::WindowBackground, MakeEditorRgba(10, 10, 12));
        Set(theme, EditorThemeColorRole::PanelBackground, MakeEditorRgba(22, 23, 27));
        Set(theme, EditorThemeColorRole::TextPrimary, MakeEditorRgba(250, 250, 250));
        Set(theme, EditorThemeColorRole::TextSecondary, MakeEditorRgba(215, 220, 230));
        Set(theme, EditorThemeColorRole::Accent, MakeEditorRgba(75, 155, 255));
        Set(theme, EditorThemeColorRole::Selection, MakeEditorRgba(32, 88, 168));
        theme.revision = 2;
        return theme;
    }

    EditorTheme BuildLightPreviewEditorTheme()
    {
        EditorTheme theme = BuildDefaultEditorTheme();
        theme.profile = EditorThemeProfile::LightPreview;
        theme.id = "light-preview";
        theme.displayName = "Light Preview";
        Set(theme, EditorThemeColorRole::WindowBackground, MakeEditorRgba(224, 226, 231));
        Set(theme, EditorThemeColorRole::MenuBarBackground, MakeEditorRgba(238, 239, 242));
        Set(theme, EditorThemeColorRole::ToolbarBackground, MakeEditorRgba(225, 227, 232));
        Set(theme, EditorThemeColorRole::PanelBackground, MakeEditorRgba(214, 216, 222));
        Set(theme, EditorThemeColorRole::PanelHeader, MakeEditorRgba(205, 208, 216));
        Set(theme, EditorThemeColorRole::TextPrimary, MakeEditorRgba(30, 32, 38));
        Set(theme, EditorThemeColorRole::TextSecondary, MakeEditorRgba(70, 76, 88));
        Set(theme, EditorThemeColorRole::TextMuted, MakeEditorRgba(104, 110, 124));
        Set(theme, EditorThemeColorRole::TextDisabled, MakeEditorRgba(142, 147, 158));
        Set(theme, EditorThemeColorRole::Selection, MakeEditorRgba(176, 206, 252));
        Set(theme, EditorThemeColorRole::ViewportBackground, MakeEditorRgba(202, 207, 216));
        theme.revision = 3;
        return theme;
    }

    EditorTheme ScaleEditorThemeForDpi(EditorTheme theme, float dpiScale)
    {
        const float scale = std::clamp(dpiScale, 0.5f, 4.0f);
        auto scaled = [scale](i32 value) -> i32
        {
            return std::max<i32>(1, static_cast<i32>(std::round(static_cast<float>(value) * scale)));
        };

        theme.metrics.menuHeight = scaled(theme.metrics.menuHeight);
        theme.metrics.toolbarHeight = scaled(theme.metrics.toolbarHeight);
        theme.metrics.statusHeight = scaled(theme.metrics.statusHeight);
        theme.metrics.dockGap = scaled(theme.metrics.dockGap);
        theme.metrics.tabHeight = scaled(theme.metrics.tabHeight);
        theme.metrics.rowHeight = scaled(theme.metrics.rowHeight);
        theme.metrics.headerHeight = scaled(theme.metrics.headerHeight);
        theme.metrics.fieldHeight = scaled(theme.metrics.fieldHeight);
        theme.metrics.buttonHeight = scaled(theme.metrics.buttonHeight);
        theme.metrics.iconSize = scaled(theme.metrics.iconSize);
        theme.metrics.paddingSmall = scaled(theme.metrics.paddingSmall);
        theme.metrics.padding = scaled(theme.metrics.padding);
        theme.metrics.paddingLarge = scaled(theme.metrics.paddingLarge);
        theme.metrics.inspectorLabelWidth = scaled(theme.metrics.inspectorLabelWidth);
        theme.metrics.assetCellWidth = scaled(theme.metrics.assetCellWidth);
        theme.metrics.assetCellHeight = scaled(theme.metrics.assetCellHeight);
        theme.metrics.popupRowHeight = scaled(theme.metrics.popupRowHeight);
        theme.metrics.scrollbarWidth = scaled(theme.metrics.scrollbarWidth);
        theme.metrics.splitterHitSize = scaled(theme.metrics.splitterHitSize);
        theme.typography.baseFontPixels = scaled(theme.typography.baseFontPixels);
        theme.typography.smallFontPixels = scaled(theme.typography.smallFontPixels);
        theme.typography.titleFontPixels = scaled(theme.typography.titleFontPixels);
        ++theme.revision;
        return theme;
    }

    EditorThemeDiagnostics ValidateEditorTheme(const EditorTheme& theme)
    {
        EditorThemeDiagnostics diagnostics{};
        diagnostics.colorCount = theme.colors.size();
        for (EditorRgba color : theme.colors)
        {
            if (color.a == 0)
            {
                ++diagnostics.invalidAlphaCount;
            }
        }

        const i32 metrics[] = {
            theme.metrics.menuHeight,
            theme.metrics.toolbarHeight,
            theme.metrics.statusHeight,
            theme.metrics.dockGap,
            theme.metrics.tabHeight,
            theme.metrics.rowHeight,
            theme.metrics.headerHeight,
            theme.metrics.fieldHeight,
            theme.metrics.buttonHeight,
            theme.metrics.iconSize,
            theme.metrics.padding,
            theme.metrics.inspectorLabelWidth,
            theme.metrics.assetCellWidth,
            theme.metrics.assetCellHeight,
            theme.metrics.popupRowHeight,
            theme.metrics.scrollbarWidth,
            theme.metrics.splitterHitSize
        };

        for (i32 metric : metrics)
        {
            if (!MetricPositive(metric))
            {
                ++diagnostics.invalidMetricCount;
            }
        }

        diagnostics.primaryTextContrast = ContrastRatio(GetEditorThemeColor(theme, EditorThemeColorRole::TextPrimary), GetEditorThemeColor(theme, EditorThemeColorRole::PanelBackground));
        diagnostics.mutedTextContrast = ContrastRatio(GetEditorThemeColor(theme, EditorThemeColorRole::TextMuted), GetEditorThemeColor(theme, EditorThemeColorRole::PanelBackground));
        diagnostics.contrastOk = diagnostics.primaryTextContrast >= 4.5 && diagnostics.mutedTextContrast >= 2.0;
        diagnostics.metricsOk = diagnostics.invalidMetricCount == 0
            && theme.metrics.rowHeight >= theme.metrics.fieldHeight
            && theme.metrics.tabHeight >= theme.metrics.fieldHeight
            && theme.metrics.assetCellHeight >= theme.metrics.assetCellWidth / 2
            && theme.metrics.inspectorLabelWidth >= 48;
        diagnostics.typographyOk = !theme.typography.uiFont.empty()
            && !theme.typography.monoFont.empty()
            && theme.typography.baseFontPixels > 0
            && theme.typography.smallFontPixels > 0
            && theme.typography.titleFontPixels > 0;
        diagnostics.ok = diagnostics.invalidAlphaCount == 0
            && diagnostics.contrastOk
            && diagnostics.metricsOk
            && diagnostics.typographyOk;

        std::ostringstream out;
        out << "editor-theme profile=" << ToString(theme.profile)
            << " colors=" << diagnostics.colorCount
            << " invalidAlpha=" << diagnostics.invalidAlphaCount
            << " invalidMetrics=" << diagnostics.invalidMetricCount
            << " contrast=" << diagnostics.primaryTextContrast
            << '/' << diagnostics.mutedTextContrast
            << " metrics=" << (diagnostics.metricsOk ? "ok" : "bad")
            << " typography=" << (diagnostics.typographyOk ? "ok" : "bad")
            << " ok=" << (diagnostics.ok ? "true" : "false");
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string FormatEditorThemeDiagnostics(const EditorThemeDiagnostics& diagnostics)
    {
        return diagnostics.summary;
    }

    std::string FormatEditorThemeSummary(const EditorTheme& theme)
    {
        std::ostringstream out;
        out << theme.displayName
            << " metrics(menu=" << theme.metrics.menuHeight
            << ", toolbar=" << theme.metrics.toolbarHeight
            << ", tab=" << theme.metrics.tabHeight
            << ", row=" << theme.metrics.rowHeight
            << ", field=" << theme.metrics.fieldHeight
            << ") font=" << theme.typography.uiFont
            << '/' << theme.typography.baseFontPixels
            << " revision=" << theme.revision;
        return out.str();
    }

    u64 HashEditorTheme(const EditorTheme& theme)
    {
        u64 hash = 1469598103934665603ull;
        auto mix = [&hash](u64 value)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        };

        mix(static_cast<u64>(theme.profile));
        for (EditorRgba color : theme.colors)
        {
            mix(PackEditorRgba(color));
        }
        mix(static_cast<u64>(theme.metrics.menuHeight));
        mix(static_cast<u64>(theme.metrics.toolbarHeight));
        mix(static_cast<u64>(theme.metrics.tabHeight));
        mix(static_cast<u64>(theme.metrics.rowHeight));
        mix(static_cast<u64>(theme.metrics.fieldHeight));
        mix(static_cast<u64>(theme.metrics.assetCellWidth));
        mix(static_cast<u64>(theme.metrics.assetCellHeight));
        mix(static_cast<u64>(theme.typography.baseFontPixels));
        mix(static_cast<u64>(theme.revision));
        return hash;
    }
}
