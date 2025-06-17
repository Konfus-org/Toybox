#pragma once
#include "SpdLogger.h"
#include <Tbx/Systems/Debug/ILogger.h>
#include <Tbx/Systems/Plugins/IPlugin.h>
#include <Tbx/Systems/Plugins/RegisterPlugin.h>
#include <Tbx/Systems/Debug/LogEvents.h>
#include <Tbx/Utils/Ids/UID.h>

namespace SpdLogging
{
    class SpdLoggerPlugin : public Tbx::IPlugin, public SpdLogger
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