#pragma once
#include "SpdLogger.h"
#include <Toybox.h>

namespace SpdLogging
{
    class SpdLoggerModule : public Toybox::Modules::LoggerModule
    {
    public:
        Toybox::Debug::ILogger* Create() override
        {
            return new SpdLogger();
        }
    };
}

extern Toybox::Modules::LoggerModule* Load()
{
    return new SpdLogging::SpdLoggerModule();
}
