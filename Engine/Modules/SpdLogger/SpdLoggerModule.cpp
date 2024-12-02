#pragma once
#include "SpdLoggerModule.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Toybox::Debug::ILogger* SpdLoggerModule::Create()
    {
        return new SpdLogger();
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
