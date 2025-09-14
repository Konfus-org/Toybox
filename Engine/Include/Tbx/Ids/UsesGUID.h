#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/GUID.h"

namespace Tbx
{
    class UsesGuid
    {
    public:
        UsesGuid() : _id(Guid::Generate()) {}
        explicit UsesGuid(const Guid& guid) : _id(guid) {}

        bool operator==(const UsesGuid& other) const { return _id == other._id; }
        explicit(false) operator std::string() const { return _id; }
        explicit(false) operator Guid() const { return _id; }

        const Guid& GetId() const { return _id; }
        void UpdateId(const Guid& id) { _id = id; }

    private:
        Guid _id = Consts::Invalid::Guid;
    };
}
