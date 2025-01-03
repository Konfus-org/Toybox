#pragma once
#include "SpdLoggerFactory.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Tbx::ILogger* SpdLoggerFactory::Create()
    {
        return new SpdLogger();
    }

    void SpdLoggerFactory::Destroy(Tbx::ILogger* logger)
    {
        delete logger;
    }

    std::string SpdLoggerFactory::GetName() const
    {
        return "Spd Log";
    }

    std::string SpdLoggerFactory::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int SpdLoggerFactory::GetVersion() const
    {
        return 0;
    }
}

Tbx::Module* Load()
{
    return new SpdLogging::SpdLoggerFactory();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
}
