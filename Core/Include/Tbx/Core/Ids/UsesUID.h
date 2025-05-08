#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UID.h"

namespace Tbx
{
    class EXPORT UsesUID
    {
    public:
        UsesUID() = default;
        explicit UsesUID(const UID& uid) : Id(uid) {}

        bool operator==(const UsesUID& other) const { return Id == other.Id; }
        explicit(false) operator uint64() const { return Id; }
        explicit(false) operator UID() const { return Id; }

        UID Id; // Auto increments to next id on construct
    };
}