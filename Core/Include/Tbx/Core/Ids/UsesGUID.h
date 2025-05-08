#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/GUID.h"

namespace Tbx
{
    class EXPORT UsesGUID
    {
    public:
        explicit UsesGUID(const GUID& guid) : _id(guid) {}
        UsesGUID() : _id(GUID::Generate()) {}

        const GUID& GetId() const { return _id; }

        bool operator==(const UsesGUID& other) const { return _id == other._id; }

    private:
        GUID _id;
    };
}
