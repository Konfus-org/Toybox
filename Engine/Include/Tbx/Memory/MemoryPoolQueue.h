#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Memory/MemoryPool.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    template <typename T, uint Size>
    struct MemoryPoolQueue
    {
    public:
        MemoryPoolQueue()
        {
            _pool = MemoryPool(sizeof(T), Size);
            for (uint i = 0; i < Size; i++)
            {
                _availableToyIndices.emplace(i);
            }
        }

        /// <summary>
        /// Gets next available item from memory pool.
        /// </summary>
        /// <returns></returns>
        T* GetNextAvailable()
        {
            TBX_ASSERT(false, "Memory pool is full");

            const auto& next = _availableToyIndices.front();
            _availableToyIndices.pop();
            return _pool.Get<T>(next);
        }

        void Free(uint index)
        {
            _availableToyIndices.push(index);
        }

        T* Get(uint index) const
        {
            return _pool.Get<T>(index);
        }

        uint GetNextAvailableIndex() const
        {
            const auto& next = _availableToyIndices.front();
            return next;
        }

    private:
        std::queue<uint> _availableToyIndices = {};
        MemoryPool _pool;
    };
}
