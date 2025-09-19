#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    class EXPORT IStringConvertible
    {
    public:
        virtual ~IStringConvertible() = default;
        virtual std::string ToString() const = 0;
        explicit(false) operator std::string() const { return ToString(); }
    };
}
