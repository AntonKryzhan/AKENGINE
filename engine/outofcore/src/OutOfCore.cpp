#include <AK/OutOfCore/OutOfCore.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

namespace AK
{
    namespace
    {
        constexpr u32 InvalidSlot = 0xFFFFFFFFu;

        u64 ClampChunkPayloadBytes(u64 value)
        {
            return std::max<u64>(value, 1ull);
        }
    }

    ByteChunkedArray::ByteChunkedArray(u64 chunkPayloadBytes)
        : mChunkPayloadBytes(ClampChunkPayloadBytes(chunkPayloadBytes))
    {
    }

    void ByteChunkedArray::Clear()
    {
        mSizeBytes = 0;
        mChunkDescs.clear();
        mChunks.clear();
    }

    u64 ByteChunkedArray::SizeBytes() const
    {
        return mSizeBytes;
    }

    u32 ByteChunkedArray::ChunkCount() const
    {
        return static_cast<u32>(mChunks.size());
    }

    u64 ByteChunkedArray::ChunkPayloadBytes() const
    {
        return mChunkPayloadBytes;
    }

    const std::vector<OutOfCoreChunkDesc>& ByteChunkedArray::Chunks() const
    {
        return mChunkDescs;
    }

    OutOfCoreChunkId ByteChunkedArray::Append(const std::vector<u8>& bytes)
    {
        return Append(bytes.data(), static_cast<u64>(bytes.size()));
    }

    OutOfCoreChunkId ByteChunkedArray::Append(const void* data, u64 bytes)
    {
        if (!data || bytes == 0)
        {
            return {};
        }

        const u8* source = static_cast<const u8*>(data);
        OutOfCoreChunkId firstWritten{};
        u64 remaining = bytes;
        while (remaining > 0)
        {
            const u64 chunkOffset = mSizeBytes % mChunkPayloadBytes;
            if (chunkOffset == 0)
            {
                OutOfCoreChunkDesc desc{};
                desc.id.index = static_cast<u32>(mChunks.size() + 1);
                desc.logicalOffsetBytes = mSizeBytes;
                desc.state = OutOfCoreChunkState::Resident;
                mChunkDescs.push_back(desc);
                mChunks.emplace_back();
                mChunks.back().reserve(static_cast<usize>(mChunkPayloadBytes));
                if (!firstWritten.IsValid())
                {
                    firstWritten = desc.id;
                }
            }

            const u64 capacity = mChunkPayloadBytes - chunkOffset;
            const u64 toWrite = std::min(capacity, remaining);
            std::vector<u8>& chunk = mChunks.back();
            chunk.insert(chunk.end(), source, source + toWrite);

            OutOfCoreChunkDesc& desc = mChunkDescs.back();
            desc.payloadBytes = static_cast<u64>(chunk.size());
            desc.residentBytes = desc.payloadBytes;
            desc.dirty = true;

            source += toWrite;
            remaining -= toWrite;
            mSizeBytes += toWrite;
        }

        return firstWritten;
    }

    bool ByteChunkedArray::Read(u64 offsetBytes, void* outData, u64 bytes) const
    {
        if (!outData || bytes == 0 || offsetBytes + bytes > mSizeBytes)
        {
            return false;
        }

        u8* dest = static_cast<u8*>(outData);
        u64 offset = offsetBytes;
        u64 remaining = bytes;
        while (remaining > 0)
        {
            const u32 chunkIndex = ChunkIndexForOffset(offset);
            if (chunkIndex == InvalidSlot || chunkIndex >= mChunks.size())
            {
                return false;
            }

            const u64 inChunk = offset % mChunkPayloadBytes;
            const std::vector<u8>& chunk = mChunks[chunkIndex];
            const u64 available = static_cast<u64>(chunk.size()) - inChunk;
            const u64 toRead = std::min(available, remaining);
            std::memcpy(dest, chunk.data() + inChunk, static_cast<usize>(toRead));
            dest += toRead;
            offset += toRead;
            remaining -= toRead;
        }

        return true;
    }

    bool ByteChunkedArray::Write(u64 offsetBytes, const void* data, u64 bytes)
    {
        if (!data || bytes == 0 || offsetBytes + bytes > mSizeBytes)
        {
            return false;
        }

        const u8* source = static_cast<const u8*>(data);
        u64 offset = offsetBytes;
        u64 remaining = bytes;
        while (remaining > 0)
        {
            const u32 chunkIndex = ChunkIndexForOffset(offset);
            if (chunkIndex == InvalidSlot || chunkIndex >= mChunks.size())
            {
                return false;
            }

            const u64 inChunk = offset % mChunkPayloadBytes;
            std::vector<u8>& chunk = mChunks[chunkIndex];
            const u64 available = static_cast<u64>(chunk.size()) - inChunk;
            const u64 toWrite = std::min(available, remaining);
            std::memcpy(chunk.data() + inChunk, source, static_cast<usize>(toWrite));
            mChunkDescs[chunkIndex].dirty = true;
            source += toWrite;
            offset += toWrite;
            remaining -= toWrite;
        }

        return true;
    }

    OutOfCoreStats ByteChunkedArray::Stats() const
    {
        OutOfCoreStats stats{};
        stats.chunks = static_cast<u32>(mChunks.size());
        stats.residentChunks = stats.chunks;
        stats.logicalBytes = mSizeBytes;
        stats.residentBytes = mSizeBytes;
        stats.summary = ToDebugString(stats);
        return stats;
    }

    u32 ByteChunkedArray::ChunkIndexForOffset(u64 offsetBytes) const
    {
        if (offsetBytes >= mSizeBytes)
        {
            return InvalidSlot;
        }
        return static_cast<u32>(offsetBytes / mChunkPayloadBytes);
    }

    VirtualChunkCache::VirtualChunkCache(OutOfCorePolicy policy)
        : mPolicy(std::move(policy))
    {
        mPolicy.chunkPayloadBytes = ClampChunkPayloadBytes(mPolicy.chunkPayloadBytes);
    }

    VirtualChunkCache::~VirtualChunkCache()
    {
        Clear();
    }

    void VirtualChunkCache::Clear()
    {
        mSlots.clear();
        mSpillFileBytes = 0;
        mEvictions = 0;
        mLoads = 0;
        mStores = 0;
        CleanupSpillFile();
        mSpillFilePrepared = false;
    }

    const OutOfCorePolicy& VirtualChunkCache::Policy() const
    {
        return mPolicy;
    }

    void VirtualChunkCache::SetPolicy(OutOfCorePolicy policy)
    {
        const bool oldPrepared = mSpillFilePrepared;
        CleanupSpillFile();
        mPolicy = std::move(policy);
        mPolicy.chunkPayloadBytes = ClampChunkPayloadBytes(mPolicy.chunkPayloadBytes);
        mSpillFilePrepared = false;
        if (oldPrepared)
        {
            PrepareSpillFile();
        }
    }

    OutOfCoreChunkId VirtualChunkCache::AddChunk(const std::vector<u8>& bytes, u64 frameIndex)
    {
        if (bytes.empty())
        {
            return {};
        }

        Slot slot{};
        slot.desc.id.index = static_cast<u32>(mSlots.size() + 1);
        slot.desc.logicalOffsetBytes = NextLogicalOffset();
        slot.desc.payloadBytes = static_cast<u64>(bytes.size());
        slot.desc.residentBytes = static_cast<u64>(bytes.size());
        slot.desc.lastTouchedFrame = frameIndex;
        slot.desc.state = OutOfCoreChunkState::Resident;
        slot.desc.dirty = true;
        slot.payload = bytes;
        const OutOfCoreChunkId id = slot.desc.id;
        mSlots.push_back(std::move(slot));
        const bool budgetOk = EnforceBudget(frameIndex);
        (void)budgetOk;
        return id;
    }

    OutOfCoreReadResult VirtualChunkCache::ReadChunk(OutOfCoreChunkId id, u64 frameIndex)
    {
        OutOfCoreReadResult result{};
        Slot* slot = FindSlot(id);
        if (!slot)
        {
            result.error = "chunk id is not present";
            return result;
        }

        if (!EnsureResident(*slot, frameIndex))
        {
            result.error = "failed to make chunk resident";
            result.desc = slot->desc;
            return result;
        }

        slot->desc.lastTouchedFrame = frameIndex;
        result.ok = true;
        result.desc = slot->desc;
        result.bytes = slot->payload;
        const bool budgetOk = EnforceBudget(frameIndex);
        (void)budgetOk;
        return result;
    }

    bool VirtualChunkCache::WriteChunk(OutOfCoreChunkId id, const std::vector<u8>& bytes, u64 frameIndex)
    {
        if (bytes.empty())
        {
            return false;
        }

        Slot* slot = FindSlot(id);
        if (!slot || !EnsureResident(*slot, frameIndex))
        {
            return false;
        }

        slot->payload = bytes;
        slot->desc.payloadBytes = static_cast<u64>(bytes.size());
        slot->desc.residentBytes = static_cast<u64>(bytes.size());
        slot->desc.dirty = true;
        slot->desc.lastTouchedFrame = frameIndex;
        ++mStores;
        return EnforceBudget(frameIndex);
    }

    bool VirtualChunkCache::Touch(OutOfCoreChunkId id, u64 frameIndex)
    {
        Slot* slot = FindSlot(id);
        if (!slot)
        {
            return false;
        }
        slot->desc.lastTouchedFrame = frameIndex;
        return true;
    }

    const OutOfCoreChunkDesc* VirtualChunkCache::GetChunkDesc(OutOfCoreChunkId id) const
    {
        const Slot* slot = FindSlot(id);
        return slot ? &slot->desc : nullptr;
    }

    std::vector<OutOfCoreChunkDesc> VirtualChunkCache::ChunkDescs() const
    {
        std::vector<OutOfCoreChunkDesc> descs;
        descs.reserve(mSlots.size());
        for (const Slot& slot : mSlots)
        {
            descs.push_back(slot.desc);
        }
        return descs;
    }

    OutOfCoreStats VirtualChunkCache::Stats() const
    {
        OutOfCoreStats stats{};
        stats.chunks = static_cast<u32>(mSlots.size());
        stats.evictions = mEvictions;
        stats.loads = mLoads;
        stats.stores = mStores;
        stats.spillFileBytes = mSpillFileBytes;
        for (const Slot& slot : mSlots)
        {
            stats.logicalBytes += slot.desc.payloadBytes;
            switch (slot.desc.state)
            {
                case OutOfCoreChunkState::Resident:
                    ++stats.residentChunks;
                    stats.residentBytes += slot.desc.residentBytes;
                    break;
                case OutOfCoreChunkState::Spilled:
                    ++stats.spilledChunks;
                    stats.spilledBytes += slot.desc.payloadBytes;
                    break;
                case OutOfCoreChunkState::Unloaded:
                default:
                    ++stats.unloadedChunks;
                    break;
            }
        }
        stats.ok = stats.residentChunks <= mPolicy.maxResidentChunks || mPolicy.maxResidentChunks == 0;
        stats.summary = ToDebugString(stats);
        return stats;
    }

    bool VirtualChunkCache::EnforceBudget(u64 frameIndex)
    {
        if (mPolicy.maxResidentChunks == 0)
        {
            return true;
        }

        bool ok = true;
        while (ResidentChunkCount() > mPolicy.maxResidentChunks)
        {
            if (!SpillOne(frameIndex))
            {
                ok = false;
                break;
            }
        }
        return ok;
    }

    VirtualChunkCache::Slot* VirtualChunkCache::FindSlot(OutOfCoreChunkId id)
    {
        if (!id.IsValid())
        {
            return nullptr;
        }
        const u32 index = id.index - 1;
        if (index >= mSlots.size())
        {
            return nullptr;
        }
        return &mSlots[index];
    }

    const VirtualChunkCache::Slot* VirtualChunkCache::FindSlot(OutOfCoreChunkId id) const
    {
        if (!id.IsValid())
        {
            return nullptr;
        }
        const u32 index = id.index - 1;
        if (index >= mSlots.size())
        {
            return nullptr;
        }
        return &mSlots[index];
    }

    bool VirtualChunkCache::EnsureResident(Slot& slot, u64 frameIndex)
    {
        if (slot.desc.state == OutOfCoreChunkState::Resident)
        {
            slot.desc.lastTouchedFrame = frameIndex;
            return true;
        }
        if (slot.desc.state != OutOfCoreChunkState::Spilled)
        {
            return false;
        }
        if (!LoadSlot(slot))
        {
            return false;
        }
        slot.desc.lastTouchedFrame = frameIndex;
        ++mLoads;
        return true;
    }

    bool VirtualChunkCache::SpillOne(u64, OutOfCoreChunkId protectedChunk)
    {
        Slot* candidate = nullptr;
        for (Slot& slot : mSlots)
        {
            if (slot.desc.state != OutOfCoreChunkState::Resident || slot.desc.id == protectedChunk)
            {
                continue;
            }
            if (!candidate || slot.desc.lastTouchedFrame < candidate->desc.lastTouchedFrame ||
                (slot.desc.lastTouchedFrame == candidate->desc.lastTouchedFrame && slot.desc.id.index < candidate->desc.id.index))
            {
                candidate = &slot;
            }
        }

        if (!candidate)
        {
            return false;
        }

        return SpillSlot(*candidate);
    }

    bool VirtualChunkCache::SpillSlot(Slot& slot)
    {
        if (!mPolicy.allowSpillFile || !HasSpillPath() || slot.payload.empty())
        {
            return false;
        }
        if (!PrepareSpillFile())
        {
            return false;
        }

        std::ofstream stream(mPolicy.spillFilePath, std::ios::binary | std::ios::app);
        if (!stream)
        {
            return false;
        }

        slot.desc.spillOffsetBytes = mSpillFileBytes;
        stream.write(reinterpret_cast<const char*>(slot.payload.data()), static_cast<std::streamsize>(slot.payload.size()));
        if (!stream)
        {
            return false;
        }

        mSpillFileBytes += static_cast<u64>(slot.payload.size());
        slot.payload.clear();
        slot.payload.shrink_to_fit();
        slot.desc.residentBytes = 0;
        slot.desc.state = OutOfCoreChunkState::Spilled;
        slot.desc.dirty = false;
        ++mEvictions;
        return true;
    }

    bool VirtualChunkCache::LoadSlot(Slot& slot)
    {
        if (!HasSpillPath() || slot.desc.state != OutOfCoreChunkState::Spilled || slot.desc.payloadBytes == 0)
        {
            return false;
        }

        std::ifstream stream(mPolicy.spillFilePath, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        stream.seekg(static_cast<std::streamoff>(slot.desc.spillOffsetBytes), std::ios::beg);
        slot.payload.resize(static_cast<usize>(slot.desc.payloadBytes));
        stream.read(reinterpret_cast<char*>(slot.payload.data()), static_cast<std::streamsize>(slot.payload.size()));
        if (!stream)
        {
            slot.payload.clear();
            return false;
        }

        slot.desc.residentBytes = static_cast<u64>(slot.payload.size());
        slot.desc.state = OutOfCoreChunkState::Resident;
        return true;
    }

    u32 VirtualChunkCache::ResidentChunkCount() const
    {
        u32 count = 0;
        for (const Slot& slot : mSlots)
        {
            if (slot.desc.state == OutOfCoreChunkState::Resident)
            {
                ++count;
            }
        }
        return count;
    }

    u64 VirtualChunkCache::ResidentBytes() const
    {
        u64 bytes = 0;
        for (const Slot& slot : mSlots)
        {
            if (slot.desc.state == OutOfCoreChunkState::Resident)
            {
                bytes += slot.desc.residentBytes;
            }
        }
        return bytes;
    }

    u64 VirtualChunkCache::NextLogicalOffset() const
    {
        u64 offset = 0;
        for (const Slot& slot : mSlots)
        {
            offset += slot.desc.payloadBytes;
        }
        return offset;
    }

    bool VirtualChunkCache::HasSpillPath() const
    {
        return !mPolicy.spillFilePath.empty();
    }

    bool VirtualChunkCache::PrepareSpillFile()
    {
        if (mSpillFilePrepared)
        {
            return true;
        }
        if (!HasSpillPath())
        {
            return false;
        }

        std::error_code ignored;
        const std::filesystem::path parent = mPolicy.spillFilePath.parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent, ignored);
        }

        std::ofstream stream(mPolicy.spillFilePath, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }
        mSpillFilePrepared = true;
        mSpillFileBytes = 0;
        return true;
    }

    void VirtualChunkCache::CleanupSpillFile()
    {
        if (!mPolicy.keepSpillFileAfterClear && HasSpillPath())
        {
            std::error_code ignored;
            std::filesystem::remove(mPolicy.spillFilePath, ignored);
        }
    }

    const char* ToString(OutOfCoreChunkState state)
    {
        switch (state)
        {
            case OutOfCoreChunkState::Resident:
                return "resident";
            case OutOfCoreChunkState::Spilled:
                return "spilled";
            case OutOfCoreChunkState::Unloaded:
            default:
                return "unloaded";
        }
    }

    const char* ToString(OutOfCoreAccessPattern pattern)
    {
        switch (pattern)
        {
            case OutOfCoreAccessPattern::Sequential:
                return "sequential";
            case OutOfCoreAccessPattern::Random:
                return "random";
            case OutOfCoreAccessPattern::Lru:
            default:
                return "lru";
        }
    }

    std::string ToDebugString(OutOfCoreChunkId id)
    {
        std::ostringstream out;
        out << "chunk#" << id.index;
        return out.str();
    }

    std::string ToDebugString(const OutOfCoreChunkDesc& desc)
    {
        std::ostringstream out;
        out << ToDebugString(desc.id)
            << " state=" << ToString(desc.state)
            << " logical=" << desc.logicalOffsetBytes
            << " payload=" << desc.payloadBytes
            << " resident=" << desc.residentBytes
            << " spill=" << desc.spillOffsetBytes
            << " touched=" << desc.lastTouchedFrame;
        return out.str();
    }

    std::string ToDebugString(const OutOfCoreStats& stats)
    {
        std::ostringstream out;
        out << "chunks=" << stats.chunks
            << " resident=" << stats.residentChunks
            << " spilled=" << stats.spilledChunks
            << " unloaded=" << stats.unloadedChunks
            << " logical=" << stats.logicalBytes
            << " residentBytes=" << stats.residentBytes
            << " spilledBytes=" << stats.spilledBytes
            << " spillFile=" << stats.spillFileBytes
            << " evictions=" << stats.evictions
            << " loads=" << stats.loads
            << " stores=" << stats.stores
            << " ok=" << (stats.ok ? "true" : "false");
        return out.str();
    }

    std::string BuildOutOfCoreProbeSummary()
    {
        OutOfCorePolicy policy{};
        policy.chunkPayloadBytes = 64;
        policy.maxResidentChunks = 2;
        policy.spillFilePath = std::filesystem::temp_directory_path() / "ak_outofcore_probe.spill";

        VirtualChunkCache cache(policy);
        std::vector<OutOfCoreChunkId> ids;
        for (u32 i = 0; i < 5; ++i)
        {
            std::vector<u8> bytes(64, static_cast<u8>(17 + i));
            ids.push_back(cache.AddChunk(bytes, i + 1));
        }
        const OutOfCoreStats beforeRead = cache.Stats();
        const OutOfCoreReadResult read = cache.ReadChunk(ids.front(), 10);
        const OutOfCoreStats afterRead = cache.Stats();
        const bool ok = beforeRead.spilledChunks >= 3 && read.ok && read.bytes.size() == 64 && afterRead.residentChunks <= 2;

        std::ostringstream out;
        out << "OutOfCore probe: " << (ok ? "ok" : "failed") << " " << ToDebugString(afterRead);
        return out.str();
    }
}
