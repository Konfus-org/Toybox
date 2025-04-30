#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Int.h"

namespace Tbx
{
    /// <summary>
    /// A simple struct used to reserve a block of continuous memory.
    /// </summary>
    struct EXPORT MemoryPool
    {
    public:
        MemoryPool(const uint64& elementSize, const uint64& poolSize)
            : _data(new char[elementSize * poolSize]), _elementSize(elementSize) {}

        ~MemoryPool()
        {
            delete[] _data;
        }

        template<typename T>
        T* Get(uint64 index)
        {
            void* data = _data + index * _elementSize;
            return static_cast<T*>(data);
        }

    private:
        char* _data = nullptr;
        uint64 _elementSize = 0;
    };
}