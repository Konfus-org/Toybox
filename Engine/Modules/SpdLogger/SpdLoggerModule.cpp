#pragma once
#include "SpdLoggerModule.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Toybox::ILogger* SpdLoggerModule::CreateLogger(const std::string& name)
    {
        return new SpdLogger(name);
    }

    const std::string SpdLoggerModule::GetName() const
    {
        return "Spd Logger";
    }

    const std::string SpdLoggerModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    const int SpdLoggerModule::GetVersion() const
    {
        return 0;
    }
}

Toybox::LoggerModule* Load()
{
    return new SpdLogging::SpdLoggerModule();
}
