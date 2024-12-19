#pragma once

namespace Toybox
{
    class ILogger
    {
    public:
        virtual ~ILogger() = default;
        virtual void Log(int lvl, std::string msg) = 0;
    };
}