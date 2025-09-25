#include "Tbx/PCH.h"
#include "Tbx/Ids/Uid.h"

namespace Tbx
{
    Uid Uid::Invalid = Uid(-1);

    Uid Uid::Generate()
    {
        static uint64 _nextId = 0;

        auto next = _nextId;
        _nextId++;

        return Uid(next);
    }
}
