#pragma once

#include <AK/Core/Types.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class OutOfCoreChunkState : u32
    {
        Unloaded = 0,
        Resident = 1,
        Spilled = 2
    };

    enum class OutOfCoreAccessPattern : u32
    {
        Sequential = 0,
        Random = 1,
        Lru = 2
    };

    struct OutOfCoreChunkId final
    {
        u32 index = 0;

        constexpr bool IsValid() const
        {
            return index != 0;
        }
    };

    constexpr bool operator==(OutOfCoreChunkId a, OutOfCoreChunkId b)
    {
        return a.index == b.index;
    }

    constexpr bool operator!=(OutOfCoreChunkId a, OutOfCoreChunkId b)
    {
        return !(a == b);
    }

    struct OutOfCorePolicy final
    {
        u64 chunkPayloadBytes = 256ull * 1024ull;
        u32 maxResidentChunks = 8;
        OutOfCoreAccessPattern accessPattern = OutOfCoreAccessPattern::Lru;
        std::filesystem::path spillFilePath;
        bool allowSpillFile = true;
        bool deterministicEviction = true;
        bool keepSpillFileAfterClear = false;
    };

    struct OutOfCoreChunkDesc final
    {
        OutOfCoreChunkId id{};
        u64 logicalOffsetBytes = 0;
        u64 payloadBytes = 0;
        u64 residentBytes = 0;
        u64 spillOffsetBytes = 0;
        u64 lastTouchedFrame = 0;
        OutOfCoreChunkState state = OutOfCoreChunkState::Unloaded;
        bool dirty = false;
    };

    struct OutOfCoreStats final
    {
        u32 chunks = 0;
        u32 residentChunks = 0;
        u32 spilledChunks = 0;
        u32 unloadedChunks = 0;
        u64 logicalBytes = 0;
        u64 residentBytes = 0;
        u64 spilledBytes = 0;
        u64 spillFileBytes = 0;
        u32 evictions = 0;
        u32 loads = 0;
        u32 stores = 0;
        bool ok = true;
        std::string summary;
    };

    struct OutOfCoreReadResult final
    {
        bool ok = false;
        OutOfCoreChunkDesc desc{};
        std::vector<u8> bytes;
        std::string error;
    };

    class ByteChunkedArray final
    {
    public:
        explicit ByteChunkedArray(u64 chunkPayloadBytes = 256ull * 1024ull);

        void Clear();
        [[nodiscard]] u64 SizeBytes() const;
        [[nodiscard]] u32 ChunkCount() const;
        [[nodiscard]] u64 ChunkPayloadBytes() const;
        [[nodiscard]] const std::vector<OutOfCoreChunkDesc>& Chunks() const;

        OutOfCoreChunkId Append(const std::vector<u8>& bytes);
        OutOfCoreChunkId Append(const void* data, u64 bytes);
        bool Read(u64 offsetBytes, void* outData, u64 bytes) const;
        bool Write(u64 offsetBytes, const void* data, u64 bytes);
        [[nodiscard]] OutOfCoreStats Stats() const;

    private:
        [[nodiscard]] u32 ChunkIndexForOffset(u64 offsetBytes) const;

        u64 mChunkPayloadBytes = 256ull * 1024ull;
        u64 mSizeBytes = 0;
        std::vector<OutOfCoreChunkDesc> mChunkDescs;
        std::vector<std::vector<u8>> mChunks;
    };

    class VirtualChunkCache final
    {
    public:
        explicit VirtualChunkCache(OutOfCorePolicy policy = {});
        ~VirtualChunkCache();

        void Clear();
        [[nodiscard]] const OutOfCorePolicy& Policy() const;
        void SetPolicy(OutOfCorePolicy policy);

        [[nodiscard]] OutOfCoreChunkId AddChunk(const std::vector<u8>& bytes, u64 frameIndex);
        [[nodiscard]] OutOfCoreReadResult ReadChunk(OutOfCoreChunkId id, u64 frameIndex);
        [[nodiscard]] bool WriteChunk(OutOfCoreChunkId id, const std::vector<u8>& bytes, u64 frameIndex);
        [[nodiscard]] bool Touch(OutOfCoreChunkId id, u64 frameIndex);
        [[nodiscard]] const OutOfCoreChunkDesc* GetChunkDesc(OutOfCoreChunkId id) const;
        [[nodiscard]] std::vector<OutOfCoreChunkDesc> ChunkDescs() const;
        [[nodiscard]] OutOfCoreStats Stats() const;
        [[nodiscard]] bool EnforceBudget(u64 frameIndex);

    private:
        struct Slot final
        {
            OutOfCoreChunkDesc desc{};
            std::vector<u8> payload;
        };

        [[nodiscard]] Slot* FindSlot(OutOfCoreChunkId id);
        [[nodiscard]] const Slot* FindSlot(OutOfCoreChunkId id) const;
        [[nodiscard]] bool EnsureResident(Slot& slot, u64 frameIndex);
        [[nodiscard]] bool SpillOne(u64 frameIndex, OutOfCoreChunkId protectedChunk = {});
        [[nodiscard]] bool SpillSlot(Slot& slot);
        [[nodiscard]] bool LoadSlot(Slot& slot);
        [[nodiscard]] u32 ResidentChunkCount() const;
        [[nodiscard]] u64 ResidentBytes() const;
        [[nodiscard]] u64 NextLogicalOffset() const;
        [[nodiscard]] bool HasSpillPath() const;
        bool PrepareSpillFile();
        void CleanupSpillFile();

        OutOfCorePolicy mPolicy{};
        std::vector<Slot> mSlots;
        u64 mSpillFileBytes = 0;
        u32 mEvictions = 0;
        u32 mLoads = 0;
        u32 mStores = 0;
        bool mSpillFilePrepared = false;
    };

    const char* ToString(OutOfCoreChunkState state);
    const char* ToString(OutOfCoreAccessPattern pattern);
    std::string ToDebugString(OutOfCoreChunkId id);
    std::string ToDebugString(const OutOfCoreChunkDesc& desc);
    std::string ToDebugString(const OutOfCoreStats& stats);
    std::string BuildOutOfCoreProbeSummary();
}
