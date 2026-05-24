#include <AK/EditorUI/EditorThemeProbe.hpp>

#include <AK/EditorUI/EditorTheme.hpp>
#include <AK/EditorUI/EditorWidgets.hpp>

#include <sstream>

namespace AK
{
    std::string RunEditorThemeProbe()
    {
        const EditorTheme dark = BuildDefaultEditorTheme();
        const EditorTheme highContrast = BuildHighContrastEditorTheme();
        const EditorTheme scaled = ScaleEditorThemeForDpi(dark, 1.5f);
        const EditorThemeDiagnostics darkDiagnostics = ValidateEditorTheme(dark);
        const EditorThemeDiagnostics highContrastDiagnostics = ValidateEditorTheme(highContrast);
        const EditorThemeDiagnostics scaledDiagnostics = ValidateEditorTheme(scaled);
        const EditorWidgetSkin skin = MakeEditorWidgetSkinFromTheme(scaled);

        const bool hashOk = HashEditorTheme(dark) != 0 && HashEditorTheme(dark) != HashEditorTheme(highContrast);
        const bool scaleOk = scaled.metrics.rowHeight > dark.metrics.rowHeight
            && scaled.metrics.tabHeight > dark.metrics.tabHeight
            && skin.rowHeight == scaled.metrics.rowHeight
            && skin.fieldHeight == scaled.metrics.fieldHeight;
        const bool ok = darkDiagnostics.ok && highContrastDiagnostics.ok && scaledDiagnostics.ok && hashOk && scaleOk;

        std::ostringstream out;
        out << (ok ? "[ ok ]" : "[fail]")
            << " editor theme / skin / visual metrics foundation"
            << " colors=" << darkDiagnostics.colorCount
            << " row=" << dark.metrics.rowHeight
            << " scaledRow=" << scaled.metrics.rowHeight
            << " contrast=" << darkDiagnostics.primaryTextContrast
            << " skinField=" << skin.fieldHeight
            << " hash=0x" << std::hex << HashEditorTheme(dark) << std::dec
            << '\n'
            << FormatEditorThemeDiagnostics(darkDiagnostics);
        return out.str();
    }
}
