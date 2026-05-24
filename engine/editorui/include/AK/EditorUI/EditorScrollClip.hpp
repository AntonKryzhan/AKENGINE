#pragma once

#include <AK/Core/Types.hpp>
#include <AK/EditorUI/EditorDock.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class EditorScrollAxis
    {
        Vertical,
        Horizontal
    };

    enum class EditorScrollPanelKind
    {
        Unknown,
        Hierarchy,
        Inspector,
        ProjectTree,
        ProjectGrid,
        Console,
        Diagnostics
    };

    struct EditorClipRect
    {
        EditorRect rect{};
        bool valid = false;
    };

    struct EditorClipStack
    {
        std::vector<EditorClipRect> stack;
        EditorClipRect current{};
        bool clipped = false;
    };

    struct EditorScrollState
    {
        EditorScrollPanelKind panel = EditorScrollPanelKind::Unknown;
        EditorScrollAxis axis = EditorScrollAxis::Vertical;
        i32 offsetPixels = 0;
        i32 contentPixels = 0;
        i32 viewportPixels = 0;
        i32 lineStepPixels = 22;
        i32 pageStepPixels = 160;
        bool overflow = false;
        u64 revision = 1;
    };

    struct EditorVirtualListRange
    {
        std::size_t firstIndex = 0;
        std::size_t visibleCount = 0;
        std::size_t requestedCount = 0;
        std::size_t totalCount = 0;
        i32 rowHeight = 22;
        i32 scrollOffsetPixels = 0;
        i32 firstRowOffsetPixels = 0;
        i32 contentPixels = 0;
        i32 viewportPixels = 0;
        bool overflow = false;
        bool valid = false;
    };

    struct EditorVirtualGridRange
    {
        std::size_t firstIndex = 0;
        std::size_t visibleCount = 0;
        std::size_t requestedCount = 0;
        std::size_t totalCount = 0;
        i32 columns = 1;
        i32 cellWidth = 84;
        i32 cellHeight = 74;
        i32 scrollOffsetPixels = 0;
        i32 firstRowOffsetPixels = 0;
        i32 contentPixels = 0;
        i32 viewportPixels = 0;
        bool overflow = false;
        bool valid = false;
    };

    struct EditorScrollbarThumb
    {
        EditorRect track{};
        EditorRect thumb{};
        bool visible = false;
        bool valid = false;
    };

    struct EditorScrollDiagnostics
    {
        bool clipIntersectionOk = false;
        bool listRangeOk = false;
        bool gridRangeOk = false;
        bool scrollClampOk = false;
        bool ensureVisibleOk = false;
        bool scrollbarOk = false;
        std::size_t visibleListItems = 0;
        std::size_t visibleGridItems = 0;
        i32 clampedOffset = 0;
        std::string summary;
        bool ok = false;
    };

    const char* ToString(EditorScrollPanelKind kind);
    const char* ToString(EditorScrollAxis axis);

    bool IsEditorRectUsable(EditorRect rect);
    EditorRect IntersectEditorRects(EditorRect a, EditorRect b);
    EditorClipStack MakeEditorClipStack(EditorRect root);
    bool PushEditorClipRect(EditorClipStack& stack, EditorRect rect);
    bool PopEditorClipRect(EditorClipStack& stack);

    i32 ClampEditorScrollOffset(i32 offsetPixels, i32 contentPixels, i32 viewportPixels);
    EditorScrollState MakeEditorScrollState(EditorScrollPanelKind panel, i32 contentPixels, i32 viewportPixels, i32 lineStepPixels = 22);
    bool SetEditorScrollMetrics(EditorScrollState& state, i32 contentPixels, i32 viewportPixels, i32 lineStepPixels = 22);
    bool ApplyEditorScrollWheel(EditorScrollState& state, i32 wheelDelta, i32 wheelDeltaPerStep = 120, i32 linesPerStep = 3);
    bool EnsureEditorItemVisible(EditorScrollState& state, std::size_t itemIndex, i32 itemPixels);

    EditorVirtualListRange BuildEditorVirtualListRange(std::size_t totalCount, i32 viewportPixels, i32 rowHeight, i32 scrollOffsetPixels, std::size_t overscanRows = 1);
    EditorVirtualGridRange BuildEditorVirtualGridRange(std::size_t totalCount, i32 viewportWidth, i32 viewportHeight, i32 cellWidth, i32 cellHeight, i32 scrollOffsetPixels, std::size_t overscanRows = 1);
    EditorScrollbarThumb BuildEditorScrollbarThumb(EditorRect track, i32 contentPixels, i32 viewportPixels, i32 scrollOffsetPixels, EditorScrollAxis axis = EditorScrollAxis::Vertical);

    EditorScrollDiagnostics RunEditorScrollDiagnostics();
    std::string FormatEditorScrollDiagnostics(const EditorScrollDiagnostics& diagnostics);
}
