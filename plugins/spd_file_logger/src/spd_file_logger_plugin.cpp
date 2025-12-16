#include "spd_file_logger_plugin.h"
#include "tbx/app/application.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/debugging/log_requests.h"
#include "tbx/debugging/logging.h"
#include "tbx/debugging/macros.h"
#include "tbx/file_system/filepath.h"
#include <filesystem>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <system_error>

namespace tbx::plugins::spdfilelogger
{
    void SpdFileLoggerPlugin::on_attach(Application& host)
    {
        const auto& desc = host.get_description();
        auto& fs = host.get_filesystem();
        _log_directory = fs.get_logs_directory();

        if (!fs.create_directory(_log_directory))
        {
            auto new_log_dir = fs.get_working_directory();
            TBX_TRACE_ERROR(
                "SpdFileLoggerPlugin failed to create log directory {}, falling back to {}",
                _log_directory,
                new_log_dir);
            _log_directory = new_log_dir;
        }

        _log_filename_base = FilePath(desc.name).get_filename();
        rotate_logs(_log_directory, _log_filename_base, 10, fs);

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            String(get_log_file_path(_log_directory, _log_filename_base, 0, fs)).get_cstr(),
            true);
        _logger = Ref<spdlog::logger>("Tbx", sink);
        _logger->info("SpdFileLoggerPlugin attached");
    }

    void SpdFileLoggerPlugin::on_detach()
    {
        _logger->flush();
        _logger.reset();
    }

    void SpdFileLoggerPlugin::on_recieve_message(Message& msg)
    {
        on_message(
            msg,
            [this](LogMessageRequest& log)
            {
                const String filename = FilePath(log.file).get_filename();
                const char* filename_cstr = filename.get_cstr();
                switch (log.level)
                {
                    case LogLevel::Info:
                        _logger
                            ->info("[{}:{}] {}", filename_cstr, log.line, log.message.get_cstr());
                        break;
                    case LogLevel::Warning:
                        _logger
                            ->warn("[{}:{}] {}", filename_cstr, log.line, log.message.get_cstr());
                        break;
                    case LogLevel::Error:
                        _logger
                            ->error("[{}:{}] {}", filename_cstr, log.line, log.message.get_cstr());
                        break;
                    case LogLevel::Critical:
                        _logger->critical(
                            "[{}:{}] {}",
                            filename_cstr,
                            log.line,
                            log.message.get_cstr());
                        break;
                }
            });
    }
}
