#include "spd_file_logger_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/logging.h"
#include "tbx/file_system/string_path_operations.h"
#include "tbx/messages/log_commands.h"
#include "tbx/std/casting.h"
#include "tbx/std/smart_pointers.h"
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

        _log_filename_base = tbx::sanitize_string_for_file_name_usage(desc.name);
        tbx::rotate_logs(_log_directory, _log_filename_base);

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            tbx::calculate_log_path(_log_directory, _log_filename_base, 0).string(),
            true);
        _logger = tbx::Ref<spdlog::logger>("SpdFileLogger", sink);
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
        const auto* log = tbx::as<LogMessageCommand>(&msg);
        if (!log || !_logger)
        {
            return;
        }

        const auto filename = tbx::get_filename_from_string_path(log->file);
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

        msg.state = MessageState::Handled;
        _logger->flush();
    }
}
