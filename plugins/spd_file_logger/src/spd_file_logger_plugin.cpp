#include "spd_file_logger_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/log_requests.h"
#include "tbx/debugging/logging.h"
#include "tbx/debugging/macros.h"
#include <filesystem>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <system_error>

namespace tbx::plugins
{
    void SpdFileLoggerPlugin::on_attach(Application& host)
    {
        auto path = Log::open(host.get_filesystem());
        _logger = std::make_shared<spdlog::logger>(
            host.get_name(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true));
    }

    void SpdFileLoggerPlugin::on_detach()
    {
        _logger->flush();
        _logger.reset();
    }

    void SpdFileLoggerPlugin::on_recieve_message(Message& msg)
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
