#pragma once
#include "SpdLoggerModule.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Toybox::ILogger* SpdLoggerModule::CreateLogger(const std::string& name)
    {
        return new SpdLogger(name);
    }

    void SpdLoggerModule::DestroyLogger(Toybox::ILogger* loggerToDestroy)
    {
        delete loggerToDestroy;
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

SpdLogging::SpdLoggerModule* _module = nullptr;
Toybox::LoggerModule* Load()
{
    if (_module == nullptr) _module = new SpdLogging::SpdLoggerModule();
    return _module;
}

void Unload()
{
    delete _module;
}
