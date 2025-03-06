#include "TbxPCH.h"
#include "ID.h"

namespace Tbx
{
    ID::ID(uint64 id)
    {
        _id = id;
    }

    ID::ID()
    {
        _id = GetNextId();
    }

    uint64 ID::GetNextId()
    {
        static uint64 _nextId = 0;

        auto next = _nextId;
        _nextId++;

        return next;
    }
}