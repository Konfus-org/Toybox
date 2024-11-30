#pragma once
#include "SpdLogger.h"
#include <Toybox.h>

namespace SpdLogging
{
    TBX_MODULE_API class SpdLoggerModule : public Toybox::Modules::LoggerModule
    {
    public:
        Toybox::Debug::ILogger* Create() override
        {
            return new SpdLogger();
        }

        const std::string GetName() const override
        {
            return "Spd Logger";
        }

        const std::string GetAuthor() const override
        {
            return "Jeremy Hummel";
        }

        const int GetVersion() const override
        {
            return 0;
        }
    };
}

extern Toybox::Modules::LoggerModule* Load()
{
    return new SpdLogging::SpdLoggerModule();
}
