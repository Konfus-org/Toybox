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
            _state->Capacity = capacity;
            _state->Data = MakeExclusive<char[]>(sizeof(TObject) * capacity);
            _state->States.assign(capacity, false);
            _state->FreeList.reserve(capacity);
            for (uint64 index = capacity; index > 0; --index)
            {
                _state->FreeList.push_back(index - 1);
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

            TBX_ASSERT(!state->FreeList.empty(), "MemoryPool: pool exhausted");
            if (state->FreeList.empty())
            {
                return Ref<TObject>();
            }

            const uint64 index = state->FreeList.back();
            state->FreeList.pop_back();

            TObject* instance = nullptr;

            try
            {
                void* location = state->Data.get() + index * sizeof(TObject);
                instance = ::new (location) TObject(std::forward<TArgs>(args)...);
                state->States[index] = true;
            }
            catch (...)
            {
                state->FreeList.push_back(index);
                throw;
            }

            auto deleter = [state, index](TObject* pointer)
            {
                if (pointer == nullptr)
                {
                    return;
                }

                std::destroy_at(pointer);
                Release(state, index);
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
            if (_state == nullptr)
            {
                return 0;
            }

            return _state->Capacity - static_cast<uint64>(_state->FreeList.size());
        }

    private:
        struct PoolState
        {
            ExclusiveRef<char[]> Data = nullptr;
            uint64 Capacity = 0;
            std::vector<uint64> FreeList;
            std::vector<bool> States;
        };

        static void Release(const std::shared_ptr<PoolState>& state, uint64 index)
        {
            TBX_ASSERT(state != nullptr, "MemoryPool: pool state expired");
            if (state == nullptr)
            {
                return;
            }

            TBX_ASSERT(index < state->Capacity, "MemoryPool: index out of bounds");
            if (index >= state->Capacity)
            {
                return;
            }

            TBX_ASSERT(state->States[index], "MemoryPool: double free detected");
            state->States[index] = false;
            state->FreeList.push_back(index);
        }

        std::shared_ptr<PoolState> _state;
    };
}
