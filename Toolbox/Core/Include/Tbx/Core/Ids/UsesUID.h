#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UID.h"

namespace Tbx
{
    class EXPORT UsesUID
    {
    public:
        explicit UsesUID(const UID& uid) : _id(uid) {}
        UsesUID() = default;
        virtual ~UsesUID() = default;

        const UID& GetId() const { return _id; }

        bool operator==(const UsesUID& other) const { return _id == other._id; }

    private:
        UID _id;
    };
}