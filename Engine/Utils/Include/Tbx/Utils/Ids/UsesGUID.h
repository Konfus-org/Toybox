#pragma once
#include "Tbx/Utils/DllExport.h"
#include "Tbx/Utils/Ids/GUID.h"

namespace Tbx
{
    class EXPORT UsesGUID
    {
    public:
        UsesGUID() = default;
        explicit UsesGUID(const GUID& guid) : _id(guid) {}

        const GUID& GetId() const { return _id; }

    private:
        GUID _id = GUID::Generate();
    };
}
