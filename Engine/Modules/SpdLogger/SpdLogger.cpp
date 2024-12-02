#pragma once
#include "SpdLogger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace SpdLogging
{
    std::shared_ptr<spdlog::logger> _spdLogger;

    SpdLogging::SpdLogger::SpdLogger(const std::string& name)
    {
        _spdLogger = spdlog::stdout_color_mt(name);
        _spdLogger->set_pattern("%^[%T]: %v%$");
        _spdLogger->set_level(spdlog::level::level_enum::trace);
    }

    SpdLogging::SpdLogger::~SpdLogger()
    {
        _spdLogger->flush();
        spdlog::drop(_spdLogger->name());
    }

    void SpdLogging::SpdLogger::Log(int lvl, std::string msg)
    {
        _spdLogger->log(spdlog::source_loc{}, (spdlog::level::level_enum)lvl, msg);
    }
}
