#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UsesUID.h"
#include "Tbx/TypeAliases/Int.h"
#include <unordered_map>
#include <typeinfo>
#include <bitset>

namespace Tbx
{
    struct Toy : public UsesUid
    {
    public:
        EXPORT Toy() = default;
    };
}