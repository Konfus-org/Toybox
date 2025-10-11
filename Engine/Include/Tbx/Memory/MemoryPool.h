#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Debug/Asserts.h"
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace Tbx
{
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
            : _state(std::make_shared<PoolState>())
        {
            TBX_ASSERT(capacity > 0, "MemoryPool: capacity must be greater than zero");
            _state->capacity = capacity;
            _state->data = MakeExclusive<char[]>(sizeof(TObject) * capacity);
            _state->states.assign(capacity, false);
            _state->freeList.reserve(capacity);
            for (uint64 index = capacity; index > 0; --index)
            {
                _state->freeList.push_back(index - 1);
            }
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

            TBX_ASSERT(!state->freeList.empty(), "MemoryPool: pool exhausted");
            if (state->freeList.empty())
            {
                return Ref<TObject>();
            }

            const uint64 index = state->freeList.back();
            state->freeList.pop_back();

            TObject* instance = nullptr;

            try
            {
                void* location = state->data.get() + index * sizeof(TObject);
                instance = ::new (location) TObject(std::forward<TArgs>(args)...);
                state->states[index] = true;
            }
            catch (...)
            {
                state->freeList.push_back(index);
                throw;
            }

            auto deleter = [state, index](TObject* pointer)
            {
                if (pointer == nullptr)
                {
                    return;
                }

                std::destroy_at(pointer);
                release(state, index);
            };

            return Ref<TObject>(instance, deleter);
        }

        /// <summary>
        /// Returns true when the pool has no available slots.
        /// </summary>
        bool IsFull() const
        {
            return _state == nullptr || _state->freeList.empty();
        }

        /// <summary>
        /// Total number of slots managed by the pool.
        /// </summary>
        uint64 Capacity() const
        {
            return _state != nullptr ? _state->capacity : 0;
        }

        /// <summary>
        /// Number of currently reserved slots.
        /// </summary>
        uint64 Count() const
        {
            if (_state == nullptr)
            {
                return 0;
            }

            return _state->capacity - static_cast<uint64>(_state->freeList.size());
        }

    private:
        struct PoolState
        {
            ExclusiveRef<char[]> data = nullptr;
            uint64 capacity = 0;
            std::vector<uint64> freeList;
            std::vector<bool> states;
        };

        static void release(const std::shared_ptr<PoolState>& state, uint64 index)
        {
            TBX_ASSERT(state != nullptr, "MemoryPool: pool state expired");
            if (state == nullptr)
            {
                return;
            }

            TBX_ASSERT(index < state->capacity, "MemoryPool: index out of bounds");
            if (index >= state->capacity)
            {
                return;
            }

            TBX_ASSERT(state->states[index], "MemoryPool: double free detected");
            state->states[index] = false;
            state->freeList.push_back(index);
        }

        std::shared_ptr<PoolState> _state;
    };
}
