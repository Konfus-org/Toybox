#pragma once
#include "SpdLoggerPlugin.h"
#include "SpdLogger.h"
#include <Tbx/Core/Plugins/PluginAPI.h>
#include <Tbx/Core/Events/EventDispatcher.h>

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
        _writeToLogEventId = Tbx::EventDispatcher::Subscribe<Tbx::WriteLineToLogRequestEvent>(TBX_BIND_CALLBACK(OnWriteToLog));
        _openLogEventId = Tbx::EventDispatcher::Subscribe<Tbx::OpenLogRequestEvent>(TBX_BIND_CALLBACK(OnOpenLog));
        _closeLogEventId = Tbx::EventDispatcher::Subscribe<Tbx::CloseLogRequestEvent>(TBX_BIND_CALLBACK(OnCloseLog));
    }

    void SpdLoggerPlugin::OnUnload()
    {
        Tbx::EventDispatcher::Unsubscribe(_writeToLogEventId);
        Tbx::EventDispatcher::Unsubscribe(_openLogEventId);
        Tbx::EventDispatcher::Unsubscribe(_closeLogEventId);

        _loggers.clear();

        spdlog::drop_all();
    }

    void SpdLoggerPlugin::OnWriteToLog(Tbx::WriteLineToLogRequestEvent& e)
    {
        const auto& logger = std::find_if(_loggers.begin(), _loggers.end(), [&](std::shared_ptr<SpdLogger> logger) { return logger->GetName() == e.GetNameOfLogToWriteTo(); });
        if (logger != _loggers.end()) 
        {
            (*logger)->Log((int)e.GetLogLevel(), e.GetLineToWriteToLog());
            e.IsHandled = true;
        }
    }

    void SpdLoggerPlugin::OnOpenLog(Tbx::OpenLogRequestEvent& e)
    {
        Provide()->Open(e.GetLogName(), e.GetLogFilePath());
        e.IsHandled = true;
    }

    void SpdLoggerPlugin::OnCloseLog(Tbx::CloseLogRequestEvent& e)
    {
        const auto& logger = std::find_if(_loggers.begin(), _loggers.end(), [&](std::shared_ptr<SpdLogger> logger) { return logger->GetName() == e.GetNameOfLogToClose(); });
        if (logger != _loggers.end())
        {
            (*logger)->Close();
            _loggers.erase(std::remove(_loggers.begin(), _loggers.end(), *logger), _loggers.end());
            e.IsHandled = true;
        }
    }
}

TBX_REGISTER_PLUGIN(SpdLogging::SpdLoggerPlugin);
