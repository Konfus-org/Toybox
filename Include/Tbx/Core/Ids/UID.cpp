#include "Tbx/Core/PCH.h"
#include "UID.h"

namespace Tbx
{
    UID::UID(uint64 id)
    {
        _id = id;
    }

    UID::UID()
    {
        _id = GetNextId();
    }

    uint64 UID::GetNextId()
    {
        static uint64 _nextId = 0;

        auto next = _nextId;
        _nextId++;

        return next;
    }
}