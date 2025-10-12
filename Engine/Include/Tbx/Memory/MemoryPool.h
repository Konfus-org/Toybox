#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Debug/Tracers.h"
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace Tbx
{
    struct TBX_EXPORT MemorySlot
    {
        uint64 Chunk = 0;
        uint64 Index = 0;
    };

    struct TBX_EXPORT MemoryChunk
    {
        MemoryChunk() = default;
        MemoryChunk(const MemoryChunk&) = delete;
        MemoryChunk& operator=(const MemoryChunk&) = delete;
        MemoryChunk(MemoryChunk&&) noexcept = default;
        MemoryChunk& operator=(MemoryChunk&&) noexcept = default;

        ExclusiveRef<char[]> Data = nullptr;
        uint64 Capacity = 0;
        std::vector<bool> States;
    };

    struct TBX_EXPORT MemoryState
    {
        MemoryState() = default;
        MemoryState(const MemoryState&) = delete;
        MemoryState& operator=(const MemoryState&) = delete;
        MemoryState(MemoryState&&) noexcept = default;
        MemoryState& operator=(MemoryState&&) noexcept = default;

        uint64 Capacity = 0;
        uint64 Count = 0;
        std::vector<MemoryChunk> Chunks;
        std::vector<MemorySlot> FreeList;
    };

    /// <summary>
    /// A pool that hands out shared pointers backed by preallocated storage.
    /// When the shared pointer is destroyed the slot is released back to the pool.
    /// </summary>
    template <typename TObject>
    struct TBX_EXPORT MemoryPool
    {
    public:
        /// <summary>
        /// Builds a pool that preallocates <paramref name="capacity"/> slots sized for <typeparamref name="TObject"/>.
        /// </summary>
        MemoryPool(uint64 capacity)
            : _state(MakeRef<MemoryState>())
        {
            TBX_ASSERT(capacity > 0, "MemoryPool: capacity must be greater than zero");
            Initialize(capacity);
            TBX_TRACE_INFO("MemoryPool: allocated %llu slots of size %zu bytes", capacity, sizeof(TObject));
        }

        /// <summary>
        /// Provides an instance constructed in-place and backed by the pool storage.
        /// </summary>
        template <typename... TArgs>
        Ref<TObject> Provide(TArgs&&... args)
        {
            const auto state = _state;
            TBX_ASSERT(state != nullptr, "MemoryPool: pool state not initialized");
            if (state == nullptr)
            {
                return Ref<TObject>();
            }

            if (state->FreeList.empty())
            {
                TBX_TRACE_ERROR("MemoryPool: exhausted pool backing %zu-byte objects (capacity=%llu)", sizeof(TObject), Capacity());
                return Ref<TObject>();
            }

            const MemorySlot slot = state->FreeList.back();
            state->FreeList.pop_back();

            TObject* instance = nullptr;

            try
            {
                TBX_ASSERT(slot.Chunk < state->Chunks.size(), "MemoryPool: chunk index out of bounds");
                if (slot.Chunk >= state->Chunks.size())
                {
                    state->FreeList.push_back(slot);
                    return Ref<TObject>();
                }

                auto& chunk = state->Chunks[slot.Chunk];
                TBX_ASSERT(slot.Index < chunk.Capacity, "MemoryPool: slot index out of bounds");
                if (slot.Index >= chunk.Capacity)
                {
                    state->FreeList.push_back(slot);
                    return Ref<TObject>();
                }

                void* location = chunk.Data.get() + slot.Index * sizeof(TObject);
                instance = ::new (location) TObject(std::forward<TArgs>(args)...);
                chunk.States[slot.Index] = true;
                state->Count++;
            }
            catch (...)
            {
                state->FreeList.push_back(slot);
                throw;
            }

            auto deleter = [state, chunkIndex = slot.Chunk, slotIndex = slot.Index](TObject* pointer)
            {
                if (pointer == nullptr)
                {
                    return;
                }

                std::destroy_at(pointer);
                Release(state, chunkIndex, slotIndex);
            };

            return Ref<TObject>(instance, deleter);
        }

        /// <summary>
        /// Returns true when the pool has no available slots.
        /// </summary>
        bool IsFull() const
        {
            return _state == nullptr || _state->FreeList.empty();
        }

        /// <summary>
        /// Total number of slots managed by the pool.
        /// </summary>
        uint64 Capacity() const
        {
            return _state != nullptr ? _state->Capacity : 0;
        }

        /// <summary>
        /// Number of currently reserved slots.
        /// </summary>
        uint64 Count() const
        {
            return _state != nullptr ? _state->Count : 0;
        }

        /// <summary>
        /// Adds additional slots to the pool.
        /// </summary>
        void Reserve(uint64 additionalCapacity)
        {
            TBX_ASSERT(additionalCapacity > 0, "MemoryPool: additional capacity must be greater than zero");
            if (additionalCapacity == 0 || _state == nullptr)
            {
                return;
            }

            const auto state = _state;
            AllocateChunk(state, additionalCapacity);
            TBX_TRACE_INFO("MemoryPool: reserved %llu additional slots (object size=%zu bytes, new capacity=%llu)", additionalCapacity, sizeof(TObject), state->Capacity);
        }

    private:
        void Initialize(uint64 capacity)
        {
            if (_state == nullptr || capacity == 0)
            {
                return;
            }

            const auto state = _state;
            state->Chunks.reserve(1);
            AllocateChunk(state, capacity);
        }

        static void Release(const Ref<MemoryState>& state, uint64 chunkIndex, uint64 slotIndex)
        {
            TBX_ASSERT(state != nullptr, "MemoryPool: pool state expired");
            if (state == nullptr)
            {
                return;
            }

            TBX_ASSERT(chunkIndex < state->Chunks.size(), "MemoryPool: chunk index out of bounds");
            if (chunkIndex >= state->Chunks.size())
            {
                return;
            }

            auto& chunk = state->Chunks[chunkIndex];
            TBX_ASSERT(slotIndex < chunk.Capacity, "MemoryPool: slot index out of bounds");
            if (slotIndex >= chunk.Capacity)
            {
                return;
            }

            TBX_ASSERT(chunk.States[slotIndex], "MemoryPool: double free detected");
            if (!chunk.States[slotIndex])
            {
                return;
            }

            chunk.States[slotIndex] = false;
            state->FreeList.push_back({chunkIndex, slotIndex});
            TBX_ASSERT(state->Count > 0, "MemoryPool: release called on empty pool");
            if (state->Count > 0)
            {
                state->Count--;
            }
        }

        static void AllocateChunk(const Ref<MemoryState>& state, uint64 capacity)
        {
            TBX_ASSERT(state != nullptr, "MemoryPool: pool state expired");
            if (state == nullptr || capacity == 0)
            {
                return;
            }

            MemoryChunk chunk;
            chunk.Capacity = capacity;
            chunk.Data = MakeExclusive<char[]>(sizeof(TObject) * capacity);
            chunk.States.assign(capacity, false);

            const uint64 chunkIndex = static_cast<uint64>(state->Chunks.size());
            state->Chunks.push_back(std::move(chunk));

            const auto additional = static_cast<size_t>(capacity);
            state->FreeList.reserve(state->FreeList.size() + additional);
            for (uint64 index = capacity; index > 0; --index)
            {
                state->FreeList.push_back({chunkIndex, index - 1});
            }

            state->Capacity += capacity;
        }

        Ref<MemoryState> _state;
    };
}
