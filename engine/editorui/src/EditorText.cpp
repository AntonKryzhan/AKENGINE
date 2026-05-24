#include <AK/EditorUI/EditorText.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr u32 ReplacementCodepoint = 0xFFFDu;

        bool IsHighSurrogate(u32 value)
        {
            return value >= 0xD800u && value <= 0xDBFFu;
        }

        bool IsLowSurrogate(u32 value)
        {
            return value >= 0xDC00u && value <= 0xDFFFu;
        }

        bool IsValidCodepoint(u32 value)
        {
            return value <= 0x10FFFFu && !IsHighSurrogate(value) && !IsLowSurrogate(value);
        }

        void AppendUtf8(std::string& out, u32 cp)
        {
            if (!IsValidCodepoint(cp))
            {
                cp = ReplacementCodepoint;
            }

            if (cp <= 0x7Fu)
            {
                out.push_back(static_cast<char>(cp));
            }
            else if (cp <= 0x7FFu)
            {
                out.push_back(static_cast<char>(0xC0u | (cp >> 6u)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
            else if (cp <= 0xFFFFu)
            {
                out.push_back(static_cast<char>(0xE0u | (cp >> 12u)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0u | (cp >> 18u)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 12u) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
        }

        std::string EncodeSlice(const std::vector<u32>& codepoints, std::size_t begin, std::size_t end)
        {
            std::string out;
            end = std::min(end, codepoints.size());
            if (begin >= end)
            {
                return out;
            }

            for (std::size_t i = begin; i < end; ++i)
            {
                AppendUtf8(out, codepoints[i]);
            }
            return out;
        }
    }

    EditorDpiScale MakeEditorDpiScale(i32 dpi, i32 baseDpi)
    {
        EditorDpiScale result{};
        result.baseDpi = baseDpi > 0 ? baseDpi : 96;
        result.dpi = dpi > 0 ? dpi : result.baseDpi;
        result.scale = static_cast<float>(result.dpi) / static_cast<float>(result.baseDpi);
        result.valid = std::isfinite(result.scale) && result.scale > 0.25f && result.scale < 8.0f;
        if (!result.valid)
        {
            result.dpi = result.baseDpi;
            result.scale = 1.0f;
            result.valid = false;
        }
        return result;
    }

    i32 EditorScaleToDevicePixels(i32 logicalPixels, EditorDpiScale dpi)
    {
        const float scaled = static_cast<float>(logicalPixels) * (dpi.valid ? dpi.scale : 1.0f);
        return static_cast<i32>(std::lround(scaled));
    }

    i32 EditorScaleToLogicalPixels(i32 devicePixels, EditorDpiScale dpi)
    {
        const float scale = dpi.valid && dpi.scale > 0.0f ? dpi.scale : 1.0f;
        return static_cast<i32>(std::lround(static_cast<float>(devicePixels) / scale));
    }

    std::vector<u32> DecodeEditorUtf8(std::string_view text, bool* valid)
    {
        std::vector<u32> out;
        bool ok = true;

        for (std::size_t i = 0; i < text.size();)
        {
            const unsigned char c0 = static_cast<unsigned char>(text[i]);
            u32 cp = ReplacementCodepoint;
            std::size_t length = 0;

            if (c0 <= 0x7Fu)
            {
                cp = c0;
                length = 1;
            }
            else if ((c0 & 0xE0u) == 0xC0u)
            {
                cp = static_cast<u32>(c0 & 0x1Fu);
                length = 2;
            }
            else if ((c0 & 0xF0u) == 0xE0u)
            {
                cp = static_cast<u32>(c0 & 0x0Fu);
                length = 3;
            }
            else if ((c0 & 0xF8u) == 0xF0u)
            {
                cp = static_cast<u32>(c0 & 0x07u);
                length = 4;
            }
            else
            {
                ok = false;
                out.push_back(ReplacementCodepoint);
                ++i;
                continue;
            }

            if (i + length > text.size())
            {
                ok = false;
                out.push_back(ReplacementCodepoint);
                break;
            }

            for (std::size_t j = 1; j < length; ++j)
            {
                const unsigned char cc = static_cast<unsigned char>(text[i + j]);
                if ((cc & 0xC0u) != 0x80u)
                {
                    ok = false;
                    length = j;
                    cp = ReplacementCodepoint;
                    break;
                }
                cp = (cp << 6u) | static_cast<u32>(cc & 0x3Fu);
            }

            const bool overlong = (length == 2 && cp < 0x80u) || (length == 3 && cp < 0x800u) || (length == 4 && cp < 0x10000u);
            if (cp == ReplacementCodepoint || overlong || !IsValidCodepoint(cp))
            {
                ok = false;
                cp = ReplacementCodepoint;
            }

            out.push_back(cp);
            i += std::max<std::size_t>(length, 1);
        }

        if (valid)
        {
            *valid = ok;
        }
        return out;
    }

    std::string EncodeEditorUtf8(const std::vector<u32>& codepoints)
    {
        std::string out;
        for (const u32 cp : codepoints)
        {
            AppendUtf8(out, cp);
        }
        return out;
    }

    bool IsValidEditorUtf8(std::string_view text)
    {
        bool valid = true;
        (void)DecodeEditorUtf8(text, &valid);
        return valid;
    }

    std::size_t CountEditorUtf8Codepoints(std::string_view text, bool* valid)
    {
        return DecodeEditorUtf8(text, valid).size();
    }

    std::wstring EditorUtf8ToWide(std::string_view text, bool* valid)
    {
        bool ok = true;
        const std::vector<u32> codepoints = DecodeEditorUtf8(text, &ok);
        std::wstring out;

        for (u32 cp : codepoints)
        {
            if (!IsValidCodepoint(cp))
            {
                cp = ReplacementCodepoint;
            }

            if constexpr (sizeof(wchar_t) == 2)
            {
                if (cp <= 0xFFFFu)
                {
                    out.push_back(static_cast<wchar_t>(cp));
                }
                else
                {
                    cp -= 0x10000u;
                    out.push_back(static_cast<wchar_t>(0xD800u + ((cp >> 10u) & 0x3FFu)));
                    out.push_back(static_cast<wchar_t>(0xDC00u + (cp & 0x3FFu)));
                }
            }
            else
            {
                out.push_back(static_cast<wchar_t>(cp));
            }
        }

        if (valid)
        {
            *valid = ok;
        }
        return out;
    }

    std::string EditorWideToUtf8(std::wstring_view text)
    {
        std::string out;
        u32 pendingHigh = 0;

        for (const wchar_t wc : text)
        {
            const u32 value = static_cast<u32>(wc);
            if constexpr (sizeof(wchar_t) == 2)
            {
                if (IsHighSurrogate(value))
                {
                    pendingHigh = value;
                    continue;
                }

                if (IsLowSurrogate(value) && pendingHigh != 0)
                {
                    const u32 cp = 0x10000u + (((pendingHigh - 0xD800u) << 10u) | (value - 0xDC00u));
                    AppendUtf8(out, cp);
                    pendingHigh = 0;
                    continue;
                }

                if (pendingHigh != 0)
                {
                    AppendUtf8(out, ReplacementCodepoint);
                    pendingHigh = 0;
                }
            }

            AppendUtf8(out, IsValidCodepoint(value) ? value : ReplacementCodepoint);
        }

        if (pendingHigh != 0)
        {
            AppendUtf8(out, ReplacementCodepoint);
        }
        return out;
    }

    std::string EditorEllipsizeEndByCodepoints(std::string_view text, std::size_t maxCodepoints)
    {
        bool valid = true;
        const std::vector<u32> codepoints = DecodeEditorUtf8(text, &valid);
        if (codepoints.size() <= maxCodepoints)
        {
            return valid ? std::string(text) : EncodeEditorUtf8(codepoints);
        }
        if (maxCodepoints == 0)
        {
            return {};
        }
        if (maxCodepoints == 1)
        {
            return "…";
        }

        std::string out = EncodeSlice(codepoints, 0, maxCodepoints - 1);
        out += "…";
        return out;
    }

    std::string EditorEllipsizeMiddleByCodepoints(std::string_view text, std::size_t maxCodepoints)
    {
        bool valid = true;
        const std::vector<u32> codepoints = DecodeEditorUtf8(text, &valid);
        if (codepoints.size() <= maxCodepoints)
        {
            return valid ? std::string(text) : EncodeEditorUtf8(codepoints);
        }
        if (maxCodepoints == 0)
        {
            return {};
        }
        if (maxCodepoints == 1)
        {
            return "…";
        }
        if (maxCodepoints == 2)
        {
            return EncodeSlice(codepoints, 0, 1) + "…";
        }

        const std::size_t keep = maxCodepoints - 1;
        const std::size_t left = (keep + 1) / 2;
        const std::size_t right = keep - left;
        return EncodeSlice(codepoints, 0, left) + "…" + EncodeSlice(codepoints, codepoints.size() - right, codepoints.size());
    }

    i32 EstimateEditorTextWidth(std::string_view text, EditorTextMetrics metrics)
    {
        bool valid = true;
        const std::vector<u32> codepoints = DecodeEditorUtf8(text, &valid);
        i32 width = 0;
        for (const u32 cp : codepoints)
        {
            if (cp == '\t')
            {
                width += metrics.averageGlyphWidth * 4;
            }
            else if (cp >= 0x1100u)
            {
                width += metrics.averageGlyphWidth * 2;
            }
            else
            {
                width += metrics.averageGlyphWidth;
            }
        }
        return EditorScaleToDevicePixels(width, metrics.dpi);
    }

    EditorTextLayoutResult LayoutEditorText(std::string_view text, EditorTextMetrics metrics, EditorTextEllipsisMode mode)
    {
        EditorTextLayoutResult result{};
        result.inputBytes = text.size();
        result.inputCodepoints = CountEditorUtf8Codepoints(text, &result.validUtf8);

        if (metrics.maxWidth <= 0 || mode == EditorTextEllipsisMode::None)
        {
            bool valid = true;
            const std::vector<u32> codepoints = DecodeEditorUtf8(text, &valid);
            result.text = valid ? std::string(text) : EncodeEditorUtf8(codepoints);
            result.outputBytes = result.text.size();
            result.outputCodepoints = CountEditorUtf8Codepoints(result.text);
            result.truncated = false;
            result.validUtf8 = valid;
            return result;
        }

        const i32 glyphWidth = std::max(1, EditorScaleToDevicePixels(metrics.averageGlyphWidth, metrics.dpi));
        const std::size_t maxCodepoints = static_cast<std::size_t>(std::max(1, metrics.maxWidth / glyphWidth));
        result.text = mode == EditorTextEllipsisMode::Middle
            ? EditorEllipsizeMiddleByCodepoints(text, maxCodepoints)
            : EditorEllipsizeEndByCodepoints(text, maxCodepoints);
        result.outputBytes = result.text.size();
        result.outputCodepoints = CountEditorUtf8Codepoints(result.text);
        result.truncated = result.outputCodepoints < result.inputCodepoints || result.outputBytes < result.inputBytes;
        return result;
    }

    EditorTextDiagnostics RunEditorTextDiagnostics()
    {
        EditorTextDiagnostics diagnostics{};
        const std::string cyrillic = "Sandbox/Сцена/Объект_Камера";
        bool valid = false;
        const std::wstring wide = EditorUtf8ToWide(cyrillic, &valid);
        const std::string roundTrip = EditorWideToUtf8(wide);

        diagnostics.validUtf8 = IsValidEditorUtf8(cyrillic);
        diagnostics.invalidUtf8Rejected = !IsValidEditorUtf8(std::string("\xC0\xAF", 2));
        diagnostics.roundTripOk = roundTrip == cyrillic;
        diagnostics.cyrillicOk = valid && CountEditorUtf8Codepoints(cyrillic) == CountEditorUtf8Codepoints(roundTrip);
        diagnostics.codepointCount = CountEditorUtf8Codepoints(cyrillic);

        EditorTextMetrics metrics{};
        metrics.averageGlyphWidth = 7;
        metrics.maxWidth = 84;
        metrics.dpi = MakeEditorDpiScale(144);
        const EditorTextLayoutResult layout = LayoutEditorText(cyrillic, metrics, EditorTextEllipsisMode::Middle);
        diagnostics.ellipsisOk = layout.truncated && IsValidEditorUtf8(layout.text) && layout.text.find("…") != std::string::npos;
        diagnostics.scaledRowHeight = EditorScaleToDevicePixels(22, metrics.dpi);
        diagnostics.dpiOk = diagnostics.scaledRowHeight == 33 && EditorScaleToLogicalPixels(diagnostics.scaledRowHeight, metrics.dpi) == 22;
        diagnostics.ok = diagnostics.validUtf8 && diagnostics.invalidUtf8Rejected && diagnostics.roundTripOk && diagnostics.cyrillicOk && diagnostics.ellipsisOk && diagnostics.dpiOk;

        std::ostringstream out;
        out << "editor-text valid=" << diagnostics.validUtf8
            << " invalidRejected=" << diagnostics.invalidUtf8Rejected
            << " roundTrip=" << diagnostics.roundTripOk
            << " cyrillic=" << diagnostics.cyrillicOk
            << " codepoints=" << diagnostics.codepointCount
            << " row144dpi=" << diagnostics.scaledRowHeight
            << " ellipsis='" << layout.text << "'"
            << " ok=" << diagnostics.ok;
        diagnostics.summary = out.str();
        return diagnostics;
    }

    std::string BuildEditorTextDiagnosticsSummary()
    {
        return RunEditorTextDiagnostics().summary;
    }
}
