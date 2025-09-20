#include "Tbx/PCH.h"
#include "Tbx/Ids/Uid.h"

namespace Tbx
{
    Uid Uid::Invalid = Uid(-1);

    uint64 Uid::GetNextId()
    {
        static uint64 _nextId = 0;

        auto next = _nextId;
        _nextId++;

        return next;
    }
}