#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    class TBX_EXPORT IPrintable
    {
    public:
        virtual ~IPrintable();
        virtual std::string ToString() const = 0;
        explicit(false) operator std::string() const { return ToString(); }
    };
}
