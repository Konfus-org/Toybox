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
            : _data(MakeExclusive<char[]>(sizeof(TObject) * capacity)),
              _capacity(capacity),
              _states(capacity, false)
        {
            TBX_ASSERT(capacity > 0, "MemoryPool: capacity must be greater than zero");
            _freeList.reserve(capacity);
            for (uint64 index = capacity; index > 0; --index)
            {
                _freeList.push_back(index - 1);
            }
        }

        /// <summary>
        /// Provides an instance constructed in-place and backed by the pool storage.
        /// </summary>
        template <typename... TArgs>
        Ref<TObject> Provide(TArgs&&... args)
        {
            TBX_ASSERT(!_freeList.empty(), "MemoryPool: pool exhausted");
            if (_freeList.empty())
            {
                return Ref<TObject>();
            }

            const uint64 index = _freeList.back();
            _freeList.pop_back();

            TObject* instance = nullptr;

            try
            {
                void* location = _data.get() + index * sizeof(TObject);
                instance = ::new (location) TObject(std::forward<TArgs>(args)...);
                _states[index] = true;
            }
            catch (...)
            {
                _freeList.push_back(index);
                throw;
            }

            auto deleter = [this, index](TObject* pointer)
            {
                if (pointer == nullptr)
                {
                    return;
                }

                std::destroy_at(pointer);
                Release(index);
            };

            return Ref<TObject>(instance, deleter);
        }

        /// <summary>
        /// Returns true when the pool has no available slots.
        /// </summary>
        bool IsFull() const
        {
            return _freeList.empty();
        }

        /// <summary>
        /// Total number of slots managed by the pool.
        /// </summary>
        uint64 Capacity() const
        {
            return _capacity;
        }

        /// <summary>
        /// Number of currently reserved slots.
        /// </summary>
        uint64 Count() const
        {
            return _capacity - static_cast<uint64>(_freeList.size());
        }

    private:
        void Release(uint64 index)
        {
            TBX_ASSERT(index < _capacity, "MemoryPool: index out of bounds");
            if (index >= _capacity)
            {
                return;
            }

            TBX_ASSERT(_states[index], "MemoryPool: double free detected");
            _states[index] = false;
            _freeList.push_back(index);
        }

        ExclusiveRef<char[]> _data = nullptr;
        uint64 _capacity = 0;
        std::vector<uint64> _freeList;
        std::vector<bool> _states;
    };
}
