#pragma once
#include <Tbx/App/App.h>
#include "SpdLogger.h"

namespace SpdLogging
{
    class SpdLoggerPlugin : public Tbx::Plugin<Tbx::ILogger>
    {
    public:
        Tbx::ILogger* Provide() override;
        void Destroy(Tbx::ILogger* toDestroy) override;

        void OnLoad() override;
        void OnUnload() override;

    private:
        std::vector<std::shared_ptr<SpdLogger>> _loggers = {};

        Tbx::UID _writeToLogEventId;
        Tbx::UID _openLogEventId;
        Tbx::UID _closeLogEventId;

        void OnWriteToLog(Tbx::WriteLineToLogRequestEvent& e);
        void OnOpenLog(Tbx::OpenLogRequestEvent& e);
        void OnCloseLog(Tbx::CloseLogRequestEvent& e);
    };
}