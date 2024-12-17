#pragma once
#include "SpdLogModule.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Toybox::ILogger* SpdLogModule::CreateLogger(const std::string& name)
    {
        return new SpdLogger(name);
    }

    void SpdLogModule::DestroyLogger(Toybox::ILogger* loggerToDestroy)
    {
        delete loggerToDestroy;
    }

    std::string SpdLogModule::GetName() const
    {
        return "Spd Log";
    }

    std::string SpdLogModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int SpdLogModule::GetVersion() const
    {
        return 0;
    }
}

SpdLogging::SpdLogModule* _module = nullptr;
Toybox::LoggerModule* Load()
{
    if (_module == nullptr) _module = new SpdLogging::SpdLogModule();
    return _module;
}

void Unload()
{
    delete _module;
}
