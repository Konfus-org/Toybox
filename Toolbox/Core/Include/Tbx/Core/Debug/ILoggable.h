#pragma once
#include "Tbx/Core/DllExport.h"
#include <string>

class EXPORT ILoggable
{
public:
    virtual ~ILoggable() = default;
    virtual std::string ToString() const = 0;
};