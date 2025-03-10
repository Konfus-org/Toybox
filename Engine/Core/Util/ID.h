#pragma once
#include "TbxAPI.h"
#include "Math/Int.h"

namespace Tbx
{
    struct TBX_API ID
    {
    public:
        explicit(false) ID(uint64 id);
        ID();
        ~ID() = default;

        const uint64& GetId() const { return _id; }

        explicit(false) operator uint64() const { return _id; }

    private:
        uint64 _id = -1;

        static uint64 GetNextId();
    };
}
