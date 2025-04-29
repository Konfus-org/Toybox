#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Ids/UID.h"

namespace Tbx
{
    uint64 UID::GetNextId()
    {
        static uint64 _nextId = 0;

        auto next = _nextId;
        _nextId++;

        return next;
    }
}