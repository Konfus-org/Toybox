#pragma once
#include "Tbx/Core/DllExport.h"

class EXPORT ILoggable
{
public:
    virtual ~ILoggable() = default;
    virtual std::string ToString() const = 0;
};