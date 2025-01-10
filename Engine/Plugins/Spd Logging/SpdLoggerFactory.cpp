#pragma once
#include "SpdLoggerFactory.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Tbx::ILogger* SpdLoggerFactory::Create()
    {
        return new SpdLogger();
    }

    void SpdLoggerFactory::Destroy(Tbx::ILogger* toDestroy)
    {
        delete toDestroy;
    }

    void SpdLoggerFactory::OnLoad()
    {
    }

    void SpdLoggerFactory::OnUnload()
    {
    }
}

TBX_REGISTER_PLUGIN(SpdLogging::SpdLoggerFactory);
