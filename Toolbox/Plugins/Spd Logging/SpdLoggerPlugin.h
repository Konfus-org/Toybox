#pragma once
#include "SpdLogger.h"
#include <Tbx/Core/Ids/UID.h>
#include <Tbx/Core/Debug/ILogger.h>
#include <Tbx/Core/Plugins/Plugin.h>
#include <Tbx/Core/Events/LogEvents.h>

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
        void OnWriteToLogEvent(Tbx::WriteLineToLogRequestEvent& e);
        void OnOpenLogEvent(Tbx::OpenLogRequestEvent& e);
        void OnCloseLogEvent(Tbx::CloseLogRequestEvent& e);

        Tbx::UID _writeToLogEventId = -1;
        Tbx::UID _openLogEventId = -1;
        Tbx::UID _closeLogEventId = -1;
    };
}