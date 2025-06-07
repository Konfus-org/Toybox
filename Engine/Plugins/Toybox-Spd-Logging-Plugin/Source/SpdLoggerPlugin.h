#pragma once
#include "SpdLogger.h"
#include <Tbx/Core/Ids/UID.h>
#include <Tbx/Core/Debug/ILogger.h>
#include <Tbx/Core/Plugins/Plugin.h>
#include <Tbx/Core/Events/LogEvents.h>
#include <Tbx/Core/Plugins/RegisterPlugin.h>

namespace SpdLogging
{
    class SpdLoggerPlugin : public Tbx::Plugin, public SpdLogger
    {
    public:
        SpdLoggerPlugin() = default;
        ~SpdLoggerPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;

    private:
        void OnWriteToLogEvent(Tbx::WriteLineToLogRequest& e);
        void OnOpenLogEvent(Tbx::OpenLogRequest& e);
        void OnCloseLogEvent(Tbx::CloseLogRequest& e);

        Tbx::UID _writeToLogEventId = -1;
        Tbx::UID _openLogEventId = -1;
        Tbx::UID _closeLogEventId = -1;
    };

    TBX_REGISTER_PLUGIN(SpdLoggerPlugin);
}