#pragma once
#include "SpdLogModule.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Toybox::ILogger* SpdLogModule::Create()
    {
        return new SpdLogger();
    }

    void SpdLogModule::Destroy(Toybox::ILogger* logger)
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

Toybox::Module* Load()
{
    return new SpdLogging::SpdLogModule();
}

void Unload(Toybox::Module* moduleToUnload)
{
    delete moduleToUnload;
}
