#pragma once
#include "ILogger.h"

namespace Toybox::Debug
{
    class SpdLogger : public ILogger
    {
    public:
        SpdLogger();
        SpdLogger(std::string name);
        ~SpdLogger() override;

        void Log(int lvl, std::string msg) override;
    };
}

