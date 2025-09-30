#include "Tbx/PCH.h"
#include "Tbx/Ids/Uid.h"
#include <atomic>

namespace Tbx
{
    TBX_EXPORT Uid Uid::Invalid = Uid(-1);

    Uid Uid::Generate()
    {
        static std::atomic<uint64> nextId = 0;

        auto next = nextId.fetch_add(1, std::memory_order_relaxed);
        return Uid(next);
    }
}
