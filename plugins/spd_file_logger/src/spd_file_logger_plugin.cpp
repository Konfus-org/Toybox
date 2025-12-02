#include "spd_file_logger_plugin.h"
#include "tbx/app/application.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/debugging/logging.h"
#include "tbx/file_system/string_path_operations.h"
#include "tbx/debugging/log_requests.h"
#include <filesystem>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <system_error>

namespace tbx::plugins::spdfilelogger
{
    void SpdFileLoggerPlugin::on_attach(Application& host)
    {
        const auto& desc = host.get_description();
        _log_directory = desc.logs_directory.empty() ? desc.working_root : desc.logs_directory;
        if (_log_directory.empty())
        {
            _log_directory = std::filesystem::current_path();
        }

        std::error_code ec;
        std::filesystem::create_directories(_log_directory, ec);

        _log_filename_base = sanitize_string_for_file_name_usage(desc.name);
        rotate_logs(_log_directory, _log_filename_base);

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            get_log_file_path(_log_directory, _log_filename_base, 0).string(),
            true);
        _logger = Ref<spdlog::logger>("SpdFileLogger", sink);
        _logger->info("SpdFileLoggerPlugin attached");
    }

    void SpdFileLoggerPlugin::on_detach()
    {
        if (_logger)
        {
            _logger->info("SpdFileLoggerPlugin detached");
            _logger->flush();
            _logger.reset();
        }
    }

    void SpdFileLoggerPlugin::on_update(const DeltaTime&) {}

    void SpdFileLoggerPlugin::on_message(Message& msg)
    {
        if (!_logger)
        {
            return;
        }

        const auto* log = dynamic_cast<LogMessageRequest*>(&msg);
        if (!log)
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
