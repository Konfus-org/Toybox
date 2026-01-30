#include "spd_console_logger_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/log_requests.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace tbx::plugins
{
    SpdConsoleLoggerPlugin::~SpdConsoleLoggerPlugin() noexcept
    {
        _logger->info(
            "[{}:{}] {}",
            "spd_console_logger_plugin.cpp",
            9,
            "Unloading SpdConsoleLoggerPlugin");
        _logger->flush();
        _logger.reset();
    }

    void SpdConsoleLoggerPlugin::on_attach(Application& app)
    {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        _logger = std::make_shared<spdlog::logger>(app.get_name(), sink);
    }

    void SpdConsoleLoggerPlugin::on_detach()
    {
        _logger->info(
            "[{}:{}] {}",
            "spd_console_logger_plugin.cpp",
            27,
            "Detaching SpdConsoleLoggerPlugin");
    }

    void SpdConsoleLoggerPlugin::on_recieve_message(Message& msg)
    {
        auto* log = handle_message<LogMessageRequest>(msg);
        if (!log)
        {
            return;
        }

        const std::string filename = log->file.filename().string();
        const char* filename_cstr = filename.c_str();
        switch (log->level)
        {
            case LogLevel::Info:
                _logger->info("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
            case LogLevel::Warning:
                _logger->warn("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
            case LogLevel::Error:
                _logger->error("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
            case LogLevel::Critical:
                _logger->critical("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
        }
    }
}
