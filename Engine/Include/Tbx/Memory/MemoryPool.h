#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Type Aliases/Int.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// A simple struct used to reserve a block of continuous memory.
    /// </summary>
    struct MemoryPool
    {
    public:
        EXPORT MemoryPool(const uint64& elementSize, const uint64& poolSize)
            : _data(std::make_shared<char[]>(elementSize * poolSize)), _elementSize(elementSize), _poolSize(poolSize) { }

        template<typename T>
        EXPORT T* Get(uint64 index)
        {
            if (index >= _poolSize)
            {
                // Handle out-of-bounds access (see point 2)
                return nullptr; // Or throw an exception
            }
            void* data = _data.get() + index * _elementSize;
            return static_cast<T*>(data);
        }

    private:
        std::shared_ptr<char[]> _data = nullptr;
        uint64 _elementSize = 0;
        uint64 _poolSize = 0;
    };
}