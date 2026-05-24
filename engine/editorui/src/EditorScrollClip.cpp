#include <AK/EditorUI/EditorScrollClip.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace AK
{
    namespace
    {
        i32 SafePositive(i32 value)
        {
            return value > 0 ? value : 0;
        }

        i32 SaturatingRectEnd(i32 origin, i32 size)
        {
            const long long value = static_cast<long long>(origin) + static_cast<long long>(size);
            return static_cast<i32>(std::clamp(value,
                                               static_cast<long long>(std::numeric_limits<i32>::min()),
                                               static_cast<long long>(std::numeric_limits<i32>::max())));
        }

        i32 CeilDiv(i32 value, i32 divisor)
        {
            if (value <= 0 || divisor <= 0)
            {
                return 0;
            }
            const long long numerator = static_cast<long long>(value) + static_cast<long long>(divisor) - 1;
            const long long quotient = numerator / static_cast<long long>(divisor);
            const long long maxValue = static_cast<long long>(std::numeric_limits<i32>::max());
            return quotient > maxValue ? std::numeric_limits<i32>::max() : static_cast<i32>(quotient);
        }

        i32 SaturatingPixelProduct(std::size_t count, i32 pixels)
        {
            const long long maxPixels = static_cast<long long>(std::numeric_limits<i32>::max());
            const std::size_t maxCount = static_cast<std::size_t>(std::numeric_limits<i32>::max());
            const std::size_t clampedCount = std::min<std::size_t>(count, maxCount);
            const long long itemPixels = static_cast<long long>(std::max<i32>(1, pixels));
            const long long product = static_cast<long long>(clampedCount) * itemPixels;
            return product > maxPixels ? std::numeric_limits<i32>::max() : static_cast<i32>(product);
        }

        i32 SaturatingPixelEnd(i32 startPixels, i32 pixels)
        {
            const long long maxPixels = static_cast<long long>(std::numeric_limits<i32>::max());
            const long long end = static_cast<long long>(SafePositive(startPixels)) + static_cast<long long>(std::max<i32>(1, pixels));
            return end > maxPixels ? std::numeric_limits<i32>::max() : static_cast<i32>(end);
        }

        i32 SaturatingI32(long long value)
        {
            const long long minValue = static_cast<long long>(std::numeric_limits<i32>::min());
            const long long maxValue = static_cast<long long>(std::numeric_limits<i32>::max());
            return static_cast<i32>(std::clamp(value, minValue, maxValue));
        }

        std::size_t SaturatingSizeAdd(std::size_t a, std::size_t b)
        {
            const std::size_t maxValue = std::numeric_limits<std::size_t>::max();
            return a > maxValue - b ? maxValue : a + b;
        }

        std::size_t SaturatingSizeProduct(std::size_t a, std::size_t b)
        {
            const std::size_t maxValue = std::numeric_limits<std::size_t>::max();
            return b != 0 && a > maxValue / b ? maxValue : a * b;
        }
    }

    const char* ToString(EditorScrollPanelKind kind)
    {
        switch (kind)
        {
            case EditorScrollPanelKind::Unknown: return "Unknown";
            case EditorScrollPanelKind::Hierarchy: return "Hierarchy";
            case EditorScrollPanelKind::Inspector: return "Inspector";
            case EditorScrollPanelKind::ProjectTree: return "ProjectTree";
            case EditorScrollPanelKind::ProjectGrid: return "ProjectGrid";
            case EditorScrollPanelKind::Console: return "Console";
            case EditorScrollPanelKind::Diagnostics: return "Diagnostics";
        }
        return "Unknown";
    }

    const char* ToString(EditorScrollAxis axis)
    {
        switch (axis)
        {
            case EditorScrollAxis::Vertical: return "Vertical";
            case EditorScrollAxis::Horizontal: return "Horizontal";
        }
        return "Unknown";
    }

    bool IsEditorRectUsable(EditorRect rect)
    {
        return rect.width > 0 && rect.height > 0;
    }

    EditorRect IntersectEditorRects(EditorRect a, EditorRect b)
    {
        const i32 left = std::max(a.x, b.x);
        const i32 top = std::max(a.y, b.y);
        const i32 right = std::min(SaturatingRectEnd(a.x, a.width), SaturatingRectEnd(b.x, b.width));
        const i32 bottom = std::min(SaturatingRectEnd(a.y, a.height), SaturatingRectEnd(b.y, b.height));
        if (right <= left || bottom <= top)
        {
            return {left, top, 0, 0};
        }
        return {left, top, right - left, bottom - top};
    }

    EditorClipStack MakeEditorClipStack(EditorRect root)
    {
        EditorClipStack stack{};
        stack.current = {root, IsEditorRectUsable(root)};
        stack.clipped = stack.current.valid;
        if (stack.current.valid)
        {
            stack.stack.push_back(stack.current);
        }
        return stack;
    }

    bool PushEditorClipRect(EditorClipStack& stack, EditorRect rect)
    {
        if (!stack.stack.empty() && !stack.current.valid)
        {
            EditorClipRect next{};
            next.rect = stack.current.rect;
            next.valid = false;
            stack.stack.push_back(next);
            stack.current = next;
            stack.clipped = false;
            return false;
        }

        const EditorRect base = stack.current.valid ? stack.current.rect : rect;
        const EditorRect clipped = stack.current.valid ? IntersectEditorRects(base, rect) : rect;
        EditorClipRect next{};
        next.rect = clipped;
        next.valid = IsEditorRectUsable(clipped);
        stack.stack.push_back(next);
        stack.current = next;
        stack.clipped = next.valid;
        return next.valid;
    }

    bool PopEditorClipRect(EditorClipStack& stack)
    {
        if (stack.stack.empty())
        {
            stack.current = {};
            stack.clipped = false;
            return false;
        }
        stack.stack.pop_back();
        if (stack.stack.empty())
        {
            stack.current = {};
            stack.clipped = false;
            return true;
        }
        stack.current = stack.stack.back();
        stack.clipped = stack.current.valid;
        return true;
    }

    i32 ClampEditorScrollOffset(i32 offsetPixels, i32 contentPixels, i32 viewportPixels)
    {
        contentPixels = SafePositive(contentPixels);
        viewportPixels = SafePositive(viewportPixels);
        const i32 maxOffset = std::max<i32>(0, contentPixels - viewportPixels);
        return std::clamp(offsetPixels, 0, maxOffset);
    }

    EditorScrollState MakeEditorScrollState(EditorScrollPanelKind panel, i32 contentPixels, i32 viewportPixels, i32 lineStepPixels)
    {
        EditorScrollState state{};
        state.panel = panel;
        state.contentPixels = SafePositive(contentPixels);
        state.viewportPixels = SafePositive(viewportPixels);
        state.lineStepPixels = std::max<i32>(1, lineStepPixels);
        state.pageStepPixels = std::max<i32>(state.lineStepPixels, state.viewportPixels - state.lineStepPixels);
        state.offsetPixels = ClampEditorScrollOffset(0, state.contentPixels, state.viewportPixels);
        state.overflow = state.contentPixels > state.viewportPixels;
        return state;
    }

    bool SetEditorScrollMetrics(EditorScrollState& state, i32 contentPixels, i32 viewportPixels, i32 lineStepPixels)
    {
        const i32 oldOffset = state.offsetPixels;
        const i32 oldContentPixels = state.contentPixels;
        const i32 oldViewportPixels = state.viewportPixels;
        const i32 oldLineStepPixels = state.lineStepPixels;
        const i32 oldPageStepPixels = state.pageStepPixels;
        const bool oldOverflow = state.overflow;
        state.contentPixels = SafePositive(contentPixels);
        state.viewportPixels = SafePositive(viewportPixels);
        state.lineStepPixels = std::max<i32>(1, lineStepPixels);
        state.pageStepPixels = std::max<i32>(state.lineStepPixels, state.viewportPixels - state.lineStepPixels);
        state.offsetPixels = ClampEditorScrollOffset(state.offsetPixels, state.contentPixels, state.viewportPixels);
        state.overflow = state.contentPixels > state.viewportPixels;
        const bool changed = oldOffset != state.offsetPixels
            || oldContentPixels != state.contentPixels
            || oldViewportPixels != state.viewportPixels
            || oldLineStepPixels != state.lineStepPixels
            || oldPageStepPixels != state.pageStepPixels
            || oldOverflow != state.overflow;
        if (changed)
        {
            ++state.revision;
        }
        return changed;
    }

    bool ApplyEditorScrollWheel(EditorScrollState& state, i32 wheelDelta, i32 wheelDeltaPerStep, i32 linesPerStep)
    {
        if (wheelDelta == 0 || !state.overflow)
        {
            return false;
        }
        const long long steps = wheelDeltaPerStep != 0 ? static_cast<long long>(std::round(static_cast<double>(wheelDelta) / static_cast<double>(wheelDeltaPerStep))) : static_cast<long long>(wheelDelta);
        const long long deltaPixels = -steps * static_cast<long long>(std::max<i32>(1, linesPerStep)) * static_cast<long long>(std::max<i32>(1, state.lineStepPixels));
        const i32 oldOffset = state.offsetPixels;
        state.offsetPixels = ClampEditorScrollOffset(SaturatingI32(static_cast<long long>(state.offsetPixels) + deltaPixels), state.contentPixels, state.viewportPixels);
        if (oldOffset != state.offsetPixels)
        {
            ++state.revision;
            return true;
        }
        return false;
    }

    bool EnsureEditorItemVisible(EditorScrollState& state, std::size_t itemIndex, i32 itemPixels)
    {
        itemPixels = std::max<i32>(1, itemPixels);
        const i32 itemTop = SaturatingPixelProduct(std::min<std::size_t>(itemIndex, static_cast<std::size_t>(0x3fffffff)), itemPixels);
        const i32 itemBottom = SaturatingPixelEnd(itemTop, itemPixels);
        i32 nextOffset = state.offsetPixels;
        if (itemTop < nextOffset)
        {
            nextOffset = itemTop;
        }
        else if (itemBottom > nextOffset + state.viewportPixels)
        {
            nextOffset = itemBottom - state.viewportPixels;
        }
        nextOffset = ClampEditorScrollOffset(nextOffset, state.contentPixels, state.viewportPixels);
        const bool changed = nextOffset != state.offsetPixels;
        if (changed)
        {
            state.offsetPixels = nextOffset;
            ++state.revision;
        }
        return changed;
    }

    EditorVirtualListRange BuildEditorVirtualListRange(std::size_t totalCount, i32 viewportPixels, i32 rowHeight, i32 scrollOffsetPixels, std::size_t overscanRows)
    {
        EditorVirtualListRange range{};
        range.totalCount = totalCount;
        range.rowHeight = std::max<i32>(1, rowHeight);
        range.viewportPixels = SafePositive(viewportPixels);
        range.contentPixels = SaturatingPixelProduct(totalCount, range.rowHeight);
        range.scrollOffsetPixels = ClampEditorScrollOffset(scrollOffsetPixels, range.contentPixels, range.viewportPixels);
        range.overflow = range.contentPixels > range.viewportPixels;
        range.firstIndex = static_cast<std::size_t>(range.scrollOffsetPixels / range.rowHeight);
        range.firstRowOffsetPixels = -(range.scrollOffsetPixels % range.rowHeight);
        const std::size_t viewportRows = static_cast<std::size_t>(CeilDiv(range.viewportPixels + (range.scrollOffsetPixels % range.rowHeight), range.rowHeight));
        range.requestedCount = SaturatingSizeAdd(viewportRows, overscanRows);
        if (range.firstIndex >= totalCount)
        {
            range.firstIndex = totalCount;
            range.visibleCount = 0;
        }
        else
        {
            range.visibleCount = std::min(totalCount - range.firstIndex, range.requestedCount);
        }
        range.valid = range.rowHeight > 0 && range.viewportPixels >= 0;
        return range;
    }

    EditorVirtualGridRange BuildEditorVirtualGridRange(std::size_t totalCount, i32 viewportWidth, i32 viewportHeight, i32 cellWidth, i32 cellHeight, i32 scrollOffsetPixels, std::size_t overscanRows)
    {
        EditorVirtualGridRange range{};
        range.totalCount = totalCount;
        range.cellWidth = std::max<i32>(1, cellWidth);
        range.cellHeight = std::max<i32>(1, cellHeight);
        range.viewportPixels = SafePositive(viewportHeight);
        range.columns = std::max<i32>(1, SafePositive(viewportWidth) / range.cellWidth);
        const i32 totalRows = CeilDiv(static_cast<i32>(std::min<std::size_t>(totalCount, static_cast<std::size_t>(0x3fffffff))), range.columns);
        range.contentPixels = SaturatingPixelProduct(static_cast<std::size_t>(totalRows), range.cellHeight);
        range.scrollOffsetPixels = ClampEditorScrollOffset(scrollOffsetPixels, range.contentPixels, range.viewportPixels);
        range.overflow = range.contentPixels > range.viewportPixels;
        const i32 firstRow = range.scrollOffsetPixels / range.cellHeight;
        range.firstRowOffsetPixels = -(range.scrollOffsetPixels % range.cellHeight);
        range.firstIndex = SaturatingSizeProduct(static_cast<std::size_t>(std::max<i32>(0, firstRow)), static_cast<std::size_t>(range.columns));
        const std::size_t visibleRows = SaturatingSizeAdd(static_cast<std::size_t>(CeilDiv(range.viewportPixels + (range.scrollOffsetPixels % range.cellHeight), range.cellHeight)), overscanRows);
        range.requestedCount = SaturatingSizeProduct(visibleRows, static_cast<std::size_t>(range.columns));
        if (range.firstIndex >= totalCount)
        {
            range.firstIndex = totalCount;
            range.visibleCount = 0;
        }
        else
        {
            range.visibleCount = std::min(totalCount - range.firstIndex, range.requestedCount);
        }
        range.valid = range.columns > 0 && range.cellWidth > 0 && range.cellHeight > 0;
        return range;
    }

    EditorScrollbarThumb BuildEditorScrollbarThumb(EditorRect track, i32 contentPixels, i32 viewportPixels, i32 scrollOffsetPixels, EditorScrollAxis axis)
    {
        EditorScrollbarThumb thumb{};
        thumb.track = track;
        contentPixels = SafePositive(contentPixels);
        viewportPixels = SafePositive(viewportPixels);
        thumb.visible = contentPixels > viewportPixels && IsEditorRectUsable(track);
        if (!thumb.visible)
        {
            thumb.valid = true;
            return thumb;
        }

        const i32 trackPixels = axis == EditorScrollAxis::Vertical ? track.height : track.width;
        const i32 minThumbPixels = 22;
        const i32 thumbPixels = std::clamp(static_cast<i32>((static_cast<double>(viewportPixels) / static_cast<double>(std::max<i32>(1, contentPixels))) * static_cast<double>(trackPixels)), minThumbPixels, std::max(minThumbPixels, trackPixels));
        const i32 maxOffset = std::max<i32>(1, contentPixels - viewportPixels);
        const i32 travelPixels = std::max<i32>(0, trackPixels - thumbPixels);
        const i32 thumbOffset = static_cast<i32>((static_cast<double>(ClampEditorScrollOffset(scrollOffsetPixels, contentPixels, viewportPixels)) / static_cast<double>(maxOffset)) * static_cast<double>(travelPixels));
        if (axis == EditorScrollAxis::Vertical)
        {
            thumb.thumb = {track.x, track.y + thumbOffset, track.width, thumbPixels};
        }
        else
        {
            thumb.thumb = {track.x + thumbOffset, track.y, thumbPixels, track.height};
        }
        thumb.valid = IsEditorRectUsable(thumb.thumb);
        return thumb;
    }

    EditorScrollDiagnostics RunEditorScrollDiagnostics()
    {
        EditorScrollDiagnostics diagnostics{};
        const EditorRect root{0, 0, 400, 300};
        EditorClipStack clip = MakeEditorClipStack(root);
        const bool pushed = PushEditorClipRect(clip, {50, 40, 120, 90});
        diagnostics.clipIntersectionOk = pushed && clip.current.valid && clip.current.rect.x == 50 && clip.current.rect.y == 40 && clip.current.rect.width == 120 && clip.current.rect.height == 90;
        PopEditorClipRect(clip);

        EditorScrollState listScroll = MakeEditorScrollState(EditorScrollPanelKind::Hierarchy, 100 * 22, 110, 22);
        listScroll.offsetPixels = 55;
        const EditorVirtualListRange listRange = BuildEditorVirtualListRange(100, 110, 22, listScroll.offsetPixels, 1);
        diagnostics.listRangeOk = listRange.valid && listRange.firstIndex == 2 && listRange.visibleCount >= 6 && listRange.overflow;
        diagnostics.visibleListItems = listRange.visibleCount;

        EditorScrollState gridScroll = MakeEditorScrollState(EditorScrollPanelKind::ProjectGrid, 20 * 74, 160, 74);
        gridScroll.offsetPixels = 80;
        const EditorVirtualGridRange gridRange = BuildEditorVirtualGridRange(48, 260, 160, 84, 74, gridScroll.offsetPixels, 1);
        diagnostics.gridRangeOk = gridRange.valid && gridRange.columns == 3 && gridRange.visibleCount >= 9 && gridRange.overflow;
        diagnostics.visibleGridItems = gridRange.visibleCount;

        EditorScrollState wheel = MakeEditorScrollState(EditorScrollPanelKind::Inspector, 2000, 300, 22);
        ApplyEditorScrollWheel(wheel, -120);
        diagnostics.clampedOffset = wheel.offsetPixels;
        diagnostics.scrollClampOk = wheel.offsetPixels == 66;
        EnsureEditorItemVisible(wheel, 40, 22);
        diagnostics.ensureVisibleOk = wheel.offsetPixels > 66;

        const EditorScrollbarThumb thumb = BuildEditorScrollbarThumb({0, 0, 10, 100}, 1000, 250, 250);
        diagnostics.scrollbarOk = thumb.visible && thumb.valid && thumb.thumb.height >= 22 && thumb.thumb.y > 0;

        diagnostics.ok = diagnostics.clipIntersectionOk && diagnostics.listRangeOk && diagnostics.gridRangeOk && diagnostics.scrollClampOk && diagnostics.ensureVisibleOk && diagnostics.scrollbarOk;
        diagnostics.summary = FormatEditorScrollDiagnostics(diagnostics);
        return diagnostics;
    }

    std::string FormatEditorScrollDiagnostics(const EditorScrollDiagnostics& diagnostics)
    {
        std::ostringstream out;
        out << "editor-scroll clip=" << (diagnostics.clipIntersectionOk ? "ok" : "fail")
            << " list=" << diagnostics.visibleListItems
            << " grid=" << diagnostics.visibleGridItems
            << " clamp=" << diagnostics.clampedOffset
            << " ensure=" << (diagnostics.ensureVisibleOk ? "ok" : "fail")
            << " scrollbar=" << (diagnostics.scrollbarOk ? "ok" : "fail")
            << " ok=" << (diagnostics.ok ? "true" : "false");
        return out.str();
    }
}
