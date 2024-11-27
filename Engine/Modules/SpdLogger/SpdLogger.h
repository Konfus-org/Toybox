#pragma once
#include <Toybox.h>

namespace SpdLogging
{
    class SpdLogger : public Toybox::Debug::ILogger
    {
    public:
        SpdLogger();
        SpdLogger(std::string name);
        ~SpdLogger() override;

        void Log(int lvl, std::string msg) override;
    };
}

