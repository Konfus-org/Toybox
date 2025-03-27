#pragma once
#include "ToolboxAPI.h"

class TBX_API ILoggable
{
public:
    virtual ~ILoggable() = default;
    virtual std::string ToString() const = 0;
};