#pragma once
#include "Tbx/Core/DllExport.h"
#include <string>

namespace Tbx
{
    /// <summary>
    /// Implement this interface to provide a string representation of the object.
    /// Will allow for implicit casting to a string, and also allows the object to be passed directly to the logger to be logged.
    /// </summary>
    class EXPORT IPrintable
    {
    public:
        virtual ~IPrintable() = default;
        virtual std::string ToString() const = 0;
        explicit(false) operator std::string() const { return ToString(); }
    };
}