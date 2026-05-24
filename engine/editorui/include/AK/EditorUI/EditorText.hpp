#pragma once

#include <AK/Core/Types.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class EditorTextEllipsisMode
    {
        None,
        End,
        Middle
    };

    struct EditorDpiScale
    {
        i32 dpi = 96;
        i32 baseDpi = 96;
        float scale = 1.0f;
        bool valid = true;
    };

    struct EditorTextMetrics
    {
        i32 fontPixelHeight = 12;
        i32 averageGlyphWidth = 7;
        i32 maxWidth = 0;
        EditorDpiScale dpi{};
    };

    struct EditorTextLayoutResult
    {
        std::string text;
        std::size_t inputBytes = 0;
        std::size_t outputBytes = 0;
        std::size_t inputCodepoints = 0;
        std::size_t outputCodepoints = 0;
        bool truncated = false;
        bool validUtf8 = true;
    };

    struct EditorTextDiagnostics
    {
        bool validUtf8 = false;
        bool invalidUtf8Rejected = false;
        bool roundTripOk = false;
        bool cyrillicOk = false;
        bool ellipsisOk = false;
        bool dpiOk = false;
        std::size_t codepointCount = 0;
        i32 scaledRowHeight = 0;
        std::string summary;
        bool ok = false;
    };

    EditorDpiScale MakeEditorDpiScale(i32 dpi, i32 baseDpi = 96);
    i32 EditorScaleToDevicePixels(i32 logicalPixels, EditorDpiScale dpi);
    i32 EditorScaleToLogicalPixels(i32 devicePixels, EditorDpiScale dpi);

    bool IsValidEditorUtf8(std::string_view text);
    std::vector<u32> DecodeEditorUtf8(std::string_view text, bool* valid = nullptr);
    std::string EncodeEditorUtf8(const std::vector<u32>& codepoints);
    std::size_t CountEditorUtf8Codepoints(std::string_view text, bool* valid = nullptr);
    std::wstring EditorUtf8ToWide(std::string_view text, bool* valid = nullptr);
    std::string EditorWideToUtf8(std::wstring_view text);

    std::string EditorEllipsizeEndByCodepoints(std::string_view text, std::size_t maxCodepoints);
    std::string EditorEllipsizeMiddleByCodepoints(std::string_view text, std::size_t maxCodepoints);
    EditorTextLayoutResult LayoutEditorText(std::string_view text, EditorTextMetrics metrics, EditorTextEllipsisMode mode = EditorTextEllipsisMode::End);
    i32 EstimateEditorTextWidth(std::string_view text, EditorTextMetrics metrics);

    EditorTextDiagnostics RunEditorTextDiagnostics();
    std::string BuildEditorTextDiagnosticsSummary();
}
