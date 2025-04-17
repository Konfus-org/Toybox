#pragma once
#include "Tbx/Core/DllExport.h"
#include <string>

namespace Tbx
{
    /// <summary>
    /// Implement this interface to provide a string representation of the object.
    /// This also allows the object to be passed directly to the logger to be logged.
    /// </summary>
    class EXPORT ILoggable
    {
    public:
        virtual ~ILoggable() = default;
        virtual std::string ToString() const = 0;
    };
}