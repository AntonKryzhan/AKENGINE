#pragma once

#include <AK/Core/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace AK
{
    enum class MemoryTag : u32
    {
        Unknown = 0,
        Frame,
        Asset,
        Render,
        ECS,
        Jobs,
        Editor,
        Count
    };

    struct MemoryStats final
    {
        u64 currentBytes = 0;
        u64 peakBytes = 0;
        u64 totalAllocatedBytes = 0;
        u64 totalFreedBytes = 0;
        u64 activeAllocations = 0;
        u64 allocationCount = 0;
        u64 freeCount = 0;
        u64 failedAllocations = 0;
    };

    struct LinearAllocatorStats final
    {
        usize capacityBytes = 0;
        usize usedBytes = 0;
        usize peakUsedBytes = 0;
        u64 allocationCount = 0;
        u64 resetCount = 0;
        u64 failedAllocations = 0;
    };

    struct MemoryProbeResult final
    {
        bool ok = false;
        MemoryStats trackingStats{};
        LinearAllocatorStats linearStats{};
        std::string summary;
    };

    const char* ToString(MemoryTag tag);
    std::string ToDebugString(const MemoryStats& stats);
    std::string ToDebugString(const LinearAllocatorStats& stats);

    bool IsPowerOfTwo(usize value);
    usize AlignUp(usize value, usize alignment);
    usize NormalizeAlignment(usize alignment);

    class TrackingAllocator final
    {
    public:
        TrackingAllocator() = default;
        TrackingAllocator(const TrackingAllocator&) = delete;
        TrackingAllocator& operator=(const TrackingAllocator&) = delete;
        TrackingAllocator(TrackingAllocator&&) = delete;
        TrackingAllocator& operator=(TrackingAllocator&&) = delete;
        ~TrackingAllocator();

        void* Allocate(usize size, usize alignment = alignof(std::max_align_t), MemoryTag tag = MemoryTag::Unknown);
        void Deallocate(void* pointer);

        [[nodiscard]] bool Owns(void* pointer) const;
        [[nodiscard]] MemoryStats Stats() const;
        [[nodiscard]] std::vector<std::string> BuildLeakReport() const;

    private:
        struct AllocationRecord final
        {
            usize size = 0;
            usize alignment = 0;
            MemoryTag tag = MemoryTag::Unknown;
            u64 sequence = 0;
        };

        std::vector<std::pair<void*, AllocationRecord>> mAllocations;
        MemoryStats mStats{};
        u64 mNextSequence = 1;
    };

    class LinearAllocator final
    {
    public:
        struct Marker final
        {
            usize offset = 0;
        };

        explicit LinearAllocator(usize capacityBytes);
        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;
        LinearAllocator(LinearAllocator&&) noexcept = default;
        LinearAllocator& operator=(LinearAllocator&&) noexcept = default;
        ~LinearAllocator() = default;

        [[nodiscard]] void* Allocate(usize size, usize alignment = alignof(std::max_align_t));
        [[nodiscard]] Marker GetMarker() const;
        void Rewind(Marker marker);
        void Reset();

        [[nodiscard]] usize Capacity() const;
        [[nodiscard]] usize Used() const;
        [[nodiscard]] usize Remaining() const;
        [[nodiscard]] LinearAllocatorStats Stats() const;

    private:
        std::vector<std::byte> mBuffer;
        usize mOffset = 0;
        usize mPeakOffset = 0;
        u64 mAllocationCount = 0;
        u64 mResetCount = 0;
        u64 mFailedAllocations = 0;
    };

    MemoryProbeResult BuildMemoryProbe();
    std::string BuildMemoryProbeSummary();
}
