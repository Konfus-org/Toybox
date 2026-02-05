#include "spd_logger_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/log_requests.h"
#include <filesystem>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef TBX_PLATFORM_WINDOWS
    #include <spdlog/sinks/msvc_sink.h>
#endif

namespace tbx::plugins
{
    void SpdLoggerPlugin::on_attach(IPluginHost& host)
    {
        auto& settings = host.get_settings();
        FileOperator ops = FileOperator(settings.working_directory);
        auto path = ops.rotate(settings.logs_directory, "TbxDebug", ".log", 10);
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#ifdef TBX_PLATFORM_WINDOWS
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        _logger = std::make_shared<spdlog::logger>(
            host.get_name(),
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
                msvc_sink,
            });
#else
        _logger = std::make_shared<spdlog::logger>(
            host.get_name(),
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
            });
#endif
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
            return;

        std::string filename = log->file.filename().string();
        const char* filename_cstr = filename.c_str();
        switch (log->level)
        {
            case LogLevel::INFO:
                _logger->info("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
            case LogLevel::WARNING:
                _logger->warn("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
            case LogLevel::ERROR:
                _logger->error("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
            case LogLevel::CRITICAL:
                _logger->critical("[{}:{}] {}", filename_cstr, log->line, log->message);
                break;
        }
    }
}
