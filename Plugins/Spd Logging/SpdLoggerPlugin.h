#pragma once
#include <TbxCore.h>
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

        Tbx::UUID _writeToLogEventId;
        Tbx::UUID _openLogEventId;
        Tbx::UUID _closeLogEventId;

        void OnWriteToLog(Tbx::WriteLineToLogEvent& e);
        void OnOpenLog(Tbx::OpenLogEvent& e);
        void OnCloseLog(Tbx::CloseLogEvent& e);
    };
}