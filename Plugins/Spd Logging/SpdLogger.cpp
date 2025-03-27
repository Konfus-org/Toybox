#pragma once
#include "SpdLogger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <TbxCore.h>

namespace SpdLogging
{
    void SpdLogger::Open(const std::string& name, const std::string& filePath)
    {
        // Create console and file sinks
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        if (!filePath.empty())
        {
            // We have file path, make file sync
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath);

            // Combine file and console sinks into a logger
            spdlog::logger multiSinkLogger(name, { consoleSink, fileSink });
            _spdLogger = std::make_shared<spdlog::logger>(multiSinkLogger);
        }
        else
        {
            // Just use console
            _spdLogger = std::make_shared<spdlog::logger>(name, consoleSink);
        }

        // Set up and register the logger
        _spdLogger->set_pattern("%^[%T]: %v%$");
        _spdLogger->set_level(spdlog::level::level_enum::trace);
        spdlog::register_logger(_spdLogger);
    }

    void SpdLogger::Close()
    {
        Flush();
        spdlog::drop(_spdLogger->name());
    }

    void SpdLogger::Log(int lvl, const std::string& msg)
    {
        _spdLogger->log(spdlog::source_loc{}, (spdlog::level::level_enum)lvl, msg);
    }

    void SpdLogger::Flush()
    {
        _spdLogger->flush();
    }
}
