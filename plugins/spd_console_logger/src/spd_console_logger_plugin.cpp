#include "spd_console_logger_plugin.h"
#include "tbx/common/casting.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/file_system/string_path_operations.h"
#include "tbx/messages/log_commands.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace tbx::plugins::spdconsolelogger
{
    void SpdConsoleLoggerPlugin::on_attach(Application&)
    {
        if (!_logger)
        {
            auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            _logger = Ref<spdlog::logger>("SpdConsoleLogger", sink);
        }

        _logger->info("SpdConsoleLoggerPlugin attached");
    }

    void SpdConsoleLoggerPlugin::on_detach()
    {
        if (_logger)
        {
            _logger->info("SpdConsoleLoggerPlugin detached");
            _logger->flush();
            _logger.reset();
        }
    }

    void SpdConsoleLoggerPlugin::on_update(const DeltaTime&) {}

    void SpdConsoleLoggerPlugin::on_message(Message& msg)
    {
        const auto* log = as<LogMessageRequest>(&msg);
        if (!log || !_logger)
        {
            return;
        }

        const auto filename = get_filename_from_string_path(log->file);
        switch (log->level)
        {
            case LogLevel::Info:
                _logger->info("[{}:{}] {}", filename, log->line, log->message);
                break;
            case LogLevel::Warning:
                _logger->warn("[{}:{}] {}", filename, log->line, log->message);
                break;
            case LogLevel::Error:
                _logger->error("[{}:{}] {}", filename, log->line, log->message);
                break;
            case LogLevel::Critical:
                _logger->critical("[{}:{}] {}", filename, log->line, log->message);
                break;
        }

        _logger->flush();
    }
}
