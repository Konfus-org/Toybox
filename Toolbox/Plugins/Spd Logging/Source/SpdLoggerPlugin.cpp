#pragma once
#include "SpdLoggerPlugin.h"
#include "SpdLogger.h"
#include <Tbx/Core/Events/EventCoordinator.h>

namespace SpdLogging
{
    void SpdLoggerPlugin::OnLoad()
    {
        _writeToLogEventId = Tbx::EventCoordinator::Subscribe<Tbx::WriteLineToLogRequest>(TBX_BIND_FN(OnWriteToLogEvent));
        _openLogEventId = Tbx::EventCoordinator::Subscribe<Tbx::OpenLogRequest>(TBX_BIND_FN(OnOpenLogEvent));
        _closeLogEventId = Tbx::EventCoordinator::Subscribe<Tbx::CloseLogRequest>(TBX_BIND_FN(OnCloseLogEvent));
    }

    void SpdLoggerPlugin::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::WriteLineToLogRequest>(_writeToLogEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::OpenLogRequest>(_openLogEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::CloseLogRequest>(_closeLogEventId);
        
        Close();

        spdlog::drop_all();
    }

    void SpdLoggerPlugin::OnWriteToLogEvent(Tbx::WriteLineToLogRequest& e)
    {
        if (!IsOpen()) Open(e.GetLogName(), e.GetLogFilePath());
        Log((int)e.GetLogLevel(), e.GetLineToWriteToLog());
        e.IsHandled = true;
    }

    void SpdLoggerPlugin::OnOpenLogEvent(Tbx::OpenLogRequest& e)
    {
        Open(e.GetLogName(), e.GetLogFilePath());
        e.IsHandled = true;
    }

    void SpdLoggerPlugin::OnCloseLogEvent(Tbx::CloseLogRequest& e)
    {
        Close();
        e.IsHandled = true;
    }
}
