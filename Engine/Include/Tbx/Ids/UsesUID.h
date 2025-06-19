#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UID.h"

namespace Tbx
{
    class EXPORT UsesUID
    {
    public:
        UsesUID() = default;
        explicit UsesUID(const UID& uid) : _id(uid) {}

        bool operator==(const UsesUID& other) const { return _id == other._id; }
        explicit(false) operator uint64() const { return _id; }
        explicit(false) operator UID() const { return _id; }

        const UID& GetId() const { return _id; }
        void UpdateId(const UID& id) { _id = id; }

    private:
        UID _id = UID::GetNextId();
    };
}