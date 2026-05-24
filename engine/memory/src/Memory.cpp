#include <AK/Memory/Memory.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>

#if defined(_MSC_VER)
    #include <malloc.h>
#endif

namespace AK
{
    namespace
    {
        void* AllocateAligned(usize size, usize alignment)
        {
            if (size == 0)
            {
                return nullptr;
            }

            alignment = NormalizeAlignment(alignment);

#if defined(_MSC_VER)
            return _aligned_malloc(size, alignment);
#else
            void* pointer = nullptr;
            if (posix_memalign(&pointer, alignment, size) != 0)
            {
                return nullptr;
            }
            return pointer;
#endif
        }

        void FreeAligned(void* pointer)
        {
            if (!pointer)
            {
                return;
            }

#if defined(_MSC_VER)
            _aligned_free(pointer);
#else
            std::free(pointer);
#endif
        }
    }

    const char* ToString(MemoryTag tag)
    {
        switch (tag)
        {
            case MemoryTag::Unknown:
                return "Unknown";
            case MemoryTag::Frame:
                return "Frame";
            case MemoryTag::Asset:
                return "Asset";
            case MemoryTag::Render:
                return "Render";
            case MemoryTag::ECS:
                return "ECS";
            case MemoryTag::Jobs:
                return "Jobs";
            case MemoryTag::Editor:
                return "Editor";
            case MemoryTag::Count:
                return "Count";
            default:
                return "Invalid";
        }
    }

    std::string ToDebugString(const MemoryStats& stats)
    {
        std::ostringstream out;
        out << "current=" << stats.currentBytes
            << " peak=" << stats.peakBytes
            << " active=" << stats.activeAllocations
            << " allocs=" << stats.allocationCount
            << " frees=" << stats.freeCount
            << " failed=" << stats.failedAllocations;
        return out.str();
    }

    std::string ToDebugString(const LinearAllocatorStats& stats)
    {
        std::ostringstream out;
        out << "used=" << stats.usedBytes << "/" << stats.capacityBytes
            << " peak=" << stats.peakUsedBytes
            << " allocs=" << stats.allocationCount
            << " resets=" << stats.resetCount
            << " failed=" << stats.failedAllocations;
        return out.str();
    }

    bool IsPowerOfTwo(usize value)
    {
        return value != 0 && (value & (value - 1)) == 0;
    }

    usize NormalizeAlignment(usize alignment)
    {
        const usize minimumAlignment = alignof(void*);
        if (alignment < minimumAlignment)
        {
            alignment = minimumAlignment;
        }

        if (IsPowerOfTwo(alignment))
        {
            return alignment;
        }

        usize normalized = minimumAlignment;
        while (normalized < alignment && normalized <= (std::numeric_limits<usize>::max() / 2))
        {
            normalized *= 2;
        }
        return normalized;
    }

    usize AlignUp(usize value, usize alignment)
    {
        alignment = NormalizeAlignment(alignment);
        return (value + alignment - 1) & ~(alignment - 1);
    }

    TrackingAllocator::~TrackingAllocator()
    {
        for (const auto& allocation : mAllocations)
        {
            FreeAligned(allocation.first);
        }
        mAllocations.clear();
    }

    void* TrackingAllocator::Allocate(usize size, usize alignment, MemoryTag tag)
    {
        if (size == 0)
        {
            ++mStats.failedAllocations;
            return nullptr;
        }

        alignment = NormalizeAlignment(alignment);
        void* pointer = AllocateAligned(size, alignment);
        if (!pointer)
        {
            ++mStats.failedAllocations;
            return nullptr;
        }

        mAllocations.push_back({pointer, AllocationRecord{size, alignment, tag, mNextSequence++}});
        mStats.currentBytes += static_cast<u64>(size);
        mStats.totalAllocatedBytes += static_cast<u64>(size);
        mStats.peakBytes = std::max(mStats.peakBytes, mStats.currentBytes);
        ++mStats.activeAllocations;
        ++mStats.allocationCount;
        return pointer;
    }

    void TrackingAllocator::Deallocate(void* pointer)
    {
        if (!pointer)
        {
            return;
        }

        const auto it = std::find_if(mAllocations.begin(), mAllocations.end(), [pointer](const auto& allocation)
        {
            return allocation.first == pointer;
        });

        if (it == mAllocations.end())
        {
            ++mStats.failedAllocations;
            return;
        }

        const usize size = it->second.size;
        FreeAligned(pointer);
        mStats.currentBytes -= static_cast<u64>(size);
        mStats.totalFreedBytes += static_cast<u64>(size);
        --mStats.activeAllocations;
        ++mStats.freeCount;
        mAllocations.erase(it);
    }

    bool TrackingAllocator::Owns(void* pointer) const
    {
        return std::any_of(mAllocations.begin(), mAllocations.end(), [pointer](const auto& allocation)
        {
            return allocation.first == pointer;
        });
    }

    MemoryStats TrackingAllocator::Stats() const
    {
        return mStats;
    }

    std::vector<std::string> TrackingAllocator::BuildLeakReport() const
    {
        std::vector<std::string> report;
        report.reserve(mAllocations.size());

        for (const auto& allocation : mAllocations)
        {
            std::ostringstream out;
            out << "leak seq=" << allocation.second.sequence
                << " size=" << allocation.second.size
                << " align=" << allocation.second.alignment
                << " tag=" << ToString(allocation.second.tag);
            report.push_back(out.str());
        }

        return report;
    }

    LinearAllocator::LinearAllocator(usize capacityBytes)
        : mBuffer(capacityBytes)
    {
    }

    void* LinearAllocator::Allocate(usize size, usize alignment)
    {
        if (size == 0 || mBuffer.empty())
        {
            ++mFailedAllocations;
            return nullptr;
        }

        alignment = NormalizeAlignment(alignment);
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mBuffer.data());
        const std::uintptr_t current = base + mOffset;
        const std::uintptr_t aligned = static_cast<std::uintptr_t>(AlignUp(static_cast<usize>(current), alignment));
        const usize padding = static_cast<usize>(aligned - current);

        if (padding > mBuffer.size() - mOffset || size > mBuffer.size() - mOffset - padding)
        {
            ++mFailedAllocations;
            return nullptr;
        }

        mOffset += padding + size;
        mPeakOffset = std::max(mPeakOffset, mOffset);
        ++mAllocationCount;
        return reinterpret_cast<void*>(aligned);
    }

    LinearAllocator::Marker LinearAllocator::GetMarker() const
    {
        return Marker{mOffset};
    }

    void LinearAllocator::Rewind(Marker marker)
    {
        mOffset = std::min(marker.offset, mBuffer.size());
    }

    void LinearAllocator::Reset()
    {
        mOffset = 0;
        ++mResetCount;
    }

    usize LinearAllocator::Capacity() const
    {
        return mBuffer.size();
    }

    usize LinearAllocator::Used() const
    {
        return mOffset;
    }

    usize LinearAllocator::Remaining() const
    {
        return mBuffer.size() - mOffset;
    }

    LinearAllocatorStats LinearAllocator::Stats() const
    {
        return LinearAllocatorStats{
            mBuffer.size(),
            mOffset,
            mPeakOffset,
            mAllocationCount,
            mResetCount,
            mFailedAllocations
        };
    }

    MemoryProbeResult BuildMemoryProbe()
    {
        MemoryProbeResult result{};

        TrackingAllocator tracker;
        void* frameBlock = tracker.Allocate(1024, 64, MemoryTag::Frame);
        void* renderBlock = tracker.Allocate(4096, 256, MemoryTag::Render);
        void* assetBlock = tracker.Allocate(2048, 128, MemoryTag::Asset);

        const bool trackingOk = frameBlock && renderBlock && assetBlock
            && tracker.Owns(frameBlock)
            && tracker.Owns(renderBlock)
            && tracker.Owns(assetBlock);

        tracker.Deallocate(renderBlock);
        tracker.Deallocate(assetBlock);
        tracker.Deallocate(frameBlock);
        result.trackingStats = tracker.Stats();

        LinearAllocator frameArena(64 * 1024);
        void* a = frameArena.Allocate(128, 16);
        const LinearAllocator::Marker marker = frameArena.GetMarker();
        void* b = frameArena.Allocate(2048, 64);
        frameArena.Rewind(marker);
        void* c = frameArena.Allocate(512, 32);
        const bool linearOk = a && b && c && frameArena.Used() <= frameArena.Capacity();
        frameArena.Reset();
        result.linearStats = frameArena.Stats();

        result.ok = trackingOk && linearOk
            && result.trackingStats.currentBytes == 0
            && result.trackingStats.activeAllocations == 0
            && result.linearStats.usedBytes == 0;

        std::ostringstream out;
        out << "Memory probe: " << (result.ok ? "ok" : "failed")
            << " | tracking " << ToDebugString(result.trackingStats)
            << " | linear " << ToDebugString(result.linearStats);
        result.summary = out.str();
        return result;
    }

    std::string BuildMemoryProbeSummary()
    {
        return BuildMemoryProbe().summary;
    }
}
