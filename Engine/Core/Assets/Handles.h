#pragma once
#include "Math/Int.h"

namespace Tbx
{
    struct RuntimeHandle
    {
    public:
        RuntimeHandle() = default;
        ~RuntimeHandle() = default;

        const uint64& GetId() const { return _id; }

    private:
        uint64 _id = GetNextId();

        static uint64 nextId;
        static uint64 GetNextId() { return nextId++; }
    };
}
