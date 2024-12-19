#pragma once
#include "tbxapi.h"

namespace Toybox
{
    class TBX_API ILogger
    {
    public:
        virtual ~ILogger() = default;
        virtual void Log(int lvl, std::string msg) = 0;
    };
}