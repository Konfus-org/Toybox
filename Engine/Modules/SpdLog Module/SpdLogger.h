#pragma once
#include <Core.h>

namespace SpdLogging
{
    class SpdLogger : public Toybox::ILogger
    {
    public:
        SpdLogger(const std::string& name);
        ~SpdLogger() override;

        void Log(int lvl, std::string msg) override;
    };
}

