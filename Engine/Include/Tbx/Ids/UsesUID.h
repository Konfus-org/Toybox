#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"

namespace Tbx
{
    class EXPORT UsesUid
    {
    public:
        UsesUid() : _id(Uid::GetNextId()) {}
        explicit UsesUid(const Uid& uid) : _id(uid) {}

        bool operator==(const UsesUid& other) const { return _id == other._id; }
        explicit(false) operator uint64() const { return _id; }
        explicit(false) operator Uid() const { return _id; }

        const Uid& GetId() const { return _id; }
        void UpdateId(const Uid& id) { _id = id; }

    private:
        Uid _id = Uid::Invalid;
    };
}