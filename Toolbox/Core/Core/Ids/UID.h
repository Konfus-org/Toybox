#pragma once
#include "Core/ToolboxAPI.h"
#include "Core/Math/Int.h"

namespace Tbx
{
    struct TBX_API UID
    {
    public:
        explicit(false) UID(uint64 id);
        UID();
        ~UID() = default;

        const uint64& GetId() const { return _id; }

        explicit(false) operator uint64() const { return _id; }

        std::string ToString() const { return std::to_string(_id); }

    private:
        uint64 _id = -1;

        static uint64 GetNextId();
    };
}
