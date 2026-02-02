#include "spd_logger_plugin.h"
#include "tbx/debugging/log_requests.h"
#include "tbx/debugging/logging.h"
#include "tbx/debugging/macros.h"
#include <filesystem>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <system_error>

namespace tbx::plugins
{
    void SpdLoggerPlugin::on_attach(IPluginHost& host)
    {
        auto path = Log::open(host.get_filesystem());
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        _logger = std::make_shared<spdlog::logger>(
            host.get_name(),
            spdlog::sinks_init_list {console_sink, file_sink, msvc_sink});
    }

    void SpdLoggerPlugin::on_detach()
    {
        _logger->flush();
        _logger.reset();
    }

    void SpdLoggerPlugin::on_recieve_message(Message& msg)
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
