#pragma once
#include "SpdLoggerPlugin.h"
#include "SpdLogger.h"

namespace SpdLogging
{
    Tbx::ILogger* SpdLoggerPlugin::Provide()
    {
        _loggers.push_back(std::make_shared<SpdLogger>());
        return _loggers.back().get();
    }

    void SpdLoggerPlugin::Destroy(Tbx::ILogger* toDestroy)
    {
        const auto& logger = std::find_if(_loggers.begin(), _loggers.end(), [&](std::shared_ptr<SpdLogger> logger) { return logger->GetName() == toDestroy->GetName(); });
        if (logger != _loggers.end())
        {
            (*logger)->Close();
            _loggers.erase(std::remove(_loggers.begin(), _loggers.end(), *logger), _loggers.end());
        }
    }

    void SpdLoggerPlugin::OnLoad()
    {
        _writeToLogEventId = Tbx::Events::Subscribe<Tbx::WriteLineToLogEvent>(TBX_BIND_CALLBACK(OnWriteToLog));
        _openLogEventId = Tbx::Events::Subscribe<Tbx::OpenLogEvent>(TBX_BIND_CALLBACK(OnOpenLog));
        _closeLogEventId = Tbx::Events::Subscribe<Tbx::CloseLogEvent>(TBX_BIND_CALLBACK(OnCloseLog));
    }

    void SpdLoggerPlugin::OnUnload()
    {
        Tbx::Events::Unsubscribe(_writeToLogEventId);
        Tbx::Events::Unsubscribe(_openLogEventId);
        Tbx::Events::Unsubscribe(_closeLogEventId);

        _loggers.clear();

        spdlog::drop_all();
    }

    void SpdLoggerPlugin::OnWriteToLog(Tbx::WriteLineToLogEvent& e)
    {
        const auto& logger = std::find_if(_loggers.begin(), _loggers.end(), [&](std::shared_ptr<SpdLogger> logger) { return logger->GetName() == e.GetNameOfLogToWriteTo(); });
        if (logger != _loggers.end()) 
        {
            (*logger)->Log((int)e.GetLogLevel(), e.GetLineToWriteToLog());
            e.Handled = true;
        }
    }

    void SpdLoggerPlugin::OnOpenLog(Tbx::OpenLogEvent& e)
    {
        _loggers.push_back(std::make_shared<SpdLogger>());
        _loggers.back()->Open(e.GetLogName(), e.GetLogFilePath());
        e.Handled = true;
    }

    void SpdLoggerPlugin::OnCloseLog(Tbx::CloseLogEvent& e)
    {
        const auto& logger = std::find_if(_loggers.begin(), _loggers.end(), [&](std::shared_ptr<SpdLogger> logger) { return logger->GetName() == e.GetNameOfLogToClose(); });
        if (logger != _loggers.end())
        {
            (*logger)->Close();
            _loggers.erase(std::remove(_loggers.begin(), _loggers.end(), *logger), _loggers.end());
            e.Handled = true;
        }
    }
}

TBX_REGISTER_PLUGIN(SpdLogging::SpdLoggerPlugin);
