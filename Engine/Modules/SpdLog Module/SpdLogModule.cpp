#pragma once
#include "SpdLogModule.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Tbx::ILogger* SpdLogModule::Create()
    {
        return new SpdLogger();
    }

    void SpdLogModule::Destroy(Tbx::ILogger* logger)
    {
        delete logger;
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

Tbx::Module* Load()
{
    return new SpdLogging::SpdLogModule();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
}
