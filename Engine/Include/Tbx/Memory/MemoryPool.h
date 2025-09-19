#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
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
        EXPORT T* Get(uint64 index) const
        {
            if (index >= _poolSize)
            {
                TBX_ASSERT(false, "Index out of bounds!");
                return nullptr;
            }
            void* data = _data.get() + index * _elementSize;
            return static_cast<T*>(data);
        }

        template<typename T>
        EXPORT void Set(uint64 index, const T& value) const
        {
            if (index >= _poolSize)
            {
                TBX_ASSERT(false, "Index out of bounds!");
                return;
            }
            void* data = _data.get() + index * _elementSize;
            memcpy(data, &value, _elementSize);
        }

    private:
        Tbx::Ref<char[]> _data = nullptr;
        uint64 _elementSize = 0;
        uint64 _poolSize = 0;
    };
}