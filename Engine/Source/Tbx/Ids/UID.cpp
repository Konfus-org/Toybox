#include "Tbx/PCH.h"
#include "Tbx/Ids/UID.h"

namespace Tbx
{
    uint64 Uid::GetNextId()
    {
        static uint64 _nextId = 0;

        auto next = _nextId;
        _nextId++;

        return next;
    }
}