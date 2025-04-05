#pragma once
#include "SpdLoggerPlugin.h"
#include "SpdLogger.h"
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/Core/Events/EventDispatcher.h>

namespace SpdLogging
{
    void SpdLoggerPlugin::OnLoad()
    {
        _writeToLogEventId = Tbx::EventDispatcher::Subscribe<Tbx::WriteLineToLogRequestEvent>(TBX_BIND_CALLBACK(OnWriteToLogEvent));
        _openLogEventId = Tbx::EventDispatcher::Subscribe<Tbx::OpenLogRequestEvent>(TBX_BIND_CALLBACK(OnOpenLogEvent));
        _closeLogEventId = Tbx::EventDispatcher::Subscribe<Tbx::CloseLogRequestEvent>(TBX_BIND_CALLBACK(OnCloseLogEvent));
    }

    void SpdLoggerPlugin::OnUnload()
    {
        Tbx::EventDispatcher::Unsubscribe<Tbx::WriteLineToLogRequestEvent>(_writeToLogEventId);
        Tbx::EventDispatcher::Unsubscribe<Tbx::OpenLogRequestEvent>(_openLogEventId);
        Tbx::EventDispatcher::Unsubscribe<Tbx::CloseLogRequestEvent>(_closeLogEventId);
        
        Close();

        spdlog::drop_all();
    }

    void SpdLoggerPlugin::OnWriteToLogEvent(Tbx::WriteLineToLogRequestEvent& e)
    {
        if (!IsOpen()) Open(e.GetLogName(), e.GetLogFilePath());
        Log((int)e.GetLogLevel(), e.GetLineToWriteToLog());
        e.IsHandled = true;
    }

    void SpdLoggerPlugin::OnOpenLogEvent(Tbx::OpenLogRequestEvent& e)
    {
        Open(e.GetLogName(), e.GetLogFilePath());
        e.IsHandled = true;
    }

    void SpdLoggerPlugin::OnCloseLogEvent(Tbx::CloseLogRequestEvent& e)
    {
        Close();
        e.IsHandled = true;
    }
}

TBX_REGISTER_PLUGIN(SpdLogging::SpdLoggerPlugin);
