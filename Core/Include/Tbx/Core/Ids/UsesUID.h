#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UID.h"

namespace Tbx
{
    class EXPORT UsesUID
    {
    public:
        UsesUID() = default;
        explicit UsesUID(const UID& uid) : _id(uid) {}

        const UID& GetId() const { return _id; }

        bool operator==(const UsesUID& other) const { return _id == other._id; }
        explicit(false) operator uint64() const { return GetId(); }
        explicit(false) operator UID() const { return GetId(); }

    private:
        UID _id = 0;
    };
}