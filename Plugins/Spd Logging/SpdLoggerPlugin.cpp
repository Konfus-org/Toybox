#pragma once
#include "SpdLoggerPlugin.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Tbx::ILogger* SpdLoggerPlugin::Provide()
    {
        return new SpdLogger();
    }

    void SpdLoggerPlugin::Destroy(Tbx::ILogger* toDestroy)
    {
        delete toDestroy;
    }

    void SpdLoggerPlugin::OnLoad()
    {
        // No need to do anything on load
    }

    void SpdLoggerPlugin::OnUnload()
    {
        spdlog::drop_all();
    }
}

TBX_REGISTER_PLUGIN(SpdLogging::SpdLoggerPlugin);
